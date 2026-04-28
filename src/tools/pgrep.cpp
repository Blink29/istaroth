#include "common/Cli.hpp"
#include "common/Metrics.hpp"
#include "common/MpiWork.hpp"
#include "common/StringSearch.hpp"
#include "common/Traversal.hpp"

#ifdef USE_MPI
#include <mpi.h>
#endif
#ifdef USE_OPENMP
#include <omp.h>
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

#ifndef TOOL_NAME
#define TOOL_NAME "pgrep"
#endif

namespace {

void print_usage(int rank) {
    if (rank != 0) {
        return;
    }
    cerr
        << "Usage: pgrep [options] --pattern TEXT [root...]\n"
        << "Options:\n"
        << "  --pattern TEXT        Text or regex to search for inside files\n"
        << "  --regex               Treat --pattern as an ECMAScript regex\n"
        << "  --ignore-case         Case-insensitive search\n"
        << "  --traversal bfs|dfs   Local traversal strategy (default: bfs)\n"
        << "  --max-depth N         Limit traversal depth from each root (default: unlimited)\n"
        << "  --threads N           OpenMP threads per MPI rank\n"
        << "  --max-file-bytes N    Skip files larger than N bytes (default: 512 MiB)\n"
        << "  --metrics PATH        Write one CSV metrics row on rank 0\n"
        << "  --count               Print only the number of matching files\n"
        << "  --no-output           Suppress matched path output\n";
}

vector<fs::path> roots_from_positionals(const vector<string> &values) {
    vector<fs::path> roots;
    for (const auto &value : values) {
        roots.emplace_back(value);
    }
    if (roots.empty()) {
        roots.emplace_back(".");
    }
    return roots;
}

} 

int main(int argc, char **argv) {
#ifdef USE_MPI
    MPI_Init(&argc, &argv);
#endif

    int rank = 0;
    int ranks = 1;
#ifdef USE_MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);
#endif

    try {
        const pou::Cli cli(argc, argv);
        if (cli.has("help")) {
            print_usage(rank);
#ifdef USE_MPI
            MPI_Finalize();
#endif
            return 0;
        }

        pou::SearchOptions search;
        search.pattern = cli.value_or("pattern", "");
        search.regex = cli.has("regex");
        search.ignore_case = cli.has("ignore-case");
        search.max_file_bytes = static_cast<uint64_t>(cli.int_value_or("max-file-bytes", 512 * 1024 * 1024));
        if (search.pattern.empty()) {
            throw runtime_error("--pattern is required");
        }

        pou::TraverseOptions traversal;
        traversal.mode = pou::parse_traversal_mode(cli.value_or("traversal", "bfs"));
        traversal.type = pou::EntryType::File;
        traversal.omp_threads = cli.int_value_or("threads", 0);

        const int max_depth = cli.int_value_or("max-depth", -1);
        const bool count_only = cli.has("count");
        const bool no_output = cli.has("no-output");
        const auto metrics_path = cli.value("metrics");
        const auto roots = roots_from_positionals(cli.positional());

        const int effective_threads =
#ifdef USE_OPENMP
            traversal.omp_threads > 0 ? traversal.omp_threads : omp_get_max_threads();
#else
            1;
#endif
        const double start = pou::now_seconds();

        vector<pou::WorkItem> all_work;
        if (rank == 0) {
            all_work = pou::build_seed_work(roots, ranks * max(32, effective_threads * 16), max_depth);
        }
        const auto local_work = pou::distribute_work(all_work, 0);

        pou::Metrics local_metrics;
        const auto files = pou::traverse_matching(
            local_work,
            traversal,
            [](const pou::Entry &) { return true; },
            local_metrics);
        local_metrics.matches = 0;

        vector<string> local_lines;

#ifdef USE_OPENMP
#pragma omp parallel
        {
            vector<string> thread_lines;
            pou::Metrics thread_metrics;

#pragma omp for schedule(dynamic)
            for (int64_t i = 0; i < static_cast<int64_t>(files.size()); ++i) {
                const auto &file = files[static_cast<size_t>(i)];
                if (pou::file_contains(file.path, search, thread_metrics)) {
                    thread_lines.push_back(file.path.string());
                    ++thread_metrics.matches;
                }
            }

#pragma omp critical
            {
                local_lines.insert(local_lines.end(), thread_lines.begin(), thread_lines.end());
                local_metrics.bytes_read += thread_metrics.bytes_read;
                local_metrics.matches += thread_metrics.matches;
                local_metrics.errors += thread_metrics.errors;
            }
        }
#else
        pou::Metrics search_metrics;
        for (const auto &file : files) {
            if (pou::file_contains(file.path, search, search_metrics)) {
                local_lines.push_back(file.path.string());
                ++search_metrics.matches;
            }
        }
        local_metrics.bytes_read += search_metrics.bytes_read;
        local_metrics.matches += search_metrics.matches;
        local_metrics.errors += search_metrics.errors;
#endif

        local_metrics.elapsed_seconds = pou::now_seconds() - start;

        auto lines = pou::gather_lines_to_root(local_lines, 0);
        auto metrics = pou::reduce_metrics_to_root(local_metrics, 0);

        if (rank == 0) {
            sort(lines.begin(), lines.end());
            if (count_only) {
                cout << lines.size() << '\n';
            } else if (!no_output) {
                for (const auto &line : lines) {
                    cout << line << '\n';
                }
            }
            if (metrics_path) {
                error_code ec;
                const bool needs_header = !fs::exists(*metrics_path, ec) || fs::file_size(*metrics_path, ec) == 0;
                ofstream out(*metrics_path, ios::app);
                if (needs_header) {
                    out << pou::metrics_csv_header() << '\n';
                }
                out << pou::metrics_csv_row(TOOL_NAME, search.pattern, ranks, effective_threads, metrics) << '\n';
            }
        }
    } catch (const exception &ex) {
        if (rank == 0) {
            cerr << TOOL_NAME << ": " << ex.what() << '\n';
            print_usage(rank);
        }
#ifdef USE_MPI
        MPI_Finalize();
#endif
        return 2;
    }

#ifdef USE_MPI
    MPI_Finalize();
#endif
    return 0;
}
