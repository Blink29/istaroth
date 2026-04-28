#include "common/Cli.hpp"
#include "common/Metrics.hpp"
#include "common/MpiWork.hpp"
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
#include <regex>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

#ifndef TOOL_NAME
#define TOOL_NAME "pfind"
#endif

namespace {

void print_usage(int rank) {
    if (rank != 0) {
        return;
    }
    cerr
        << "Usage: pfind [options] [root...]\n"
        << "Options:\n"
        << "  --name TEXT           Match file or directory names containing TEXT\n"
        << "  --regex               Treat --name as an ECMAScript regex\n"
        << "  --type any|f|d        Restrict matches by entry type (default: any)\n"
        << "  --traversal bfs|dfs   Local traversal strategy (default: bfs)\n"
        << "  --max-depth N         Limit traversal depth from each root (default: unlimited)\n"
        << "  --threads N           OpenMP threads per MPI rank\n"
        << "  --metrics PATH        Write one CSV metrics row on rank 0\n"
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

        pou::TraverseOptions traversal;
        traversal.mode = pou::parse_traversal_mode(cli.value_or("traversal", "bfs"));
        traversal.type = pou::parse_entry_type(cli.value_or("type", "any"));
        traversal.omp_threads = cli.int_value_or("threads", 0);

        const int max_depth = cli.int_value_or("max-depth", -1);
        const auto name_pattern = cli.value_or("name", "");
        const bool use_regex = cli.has("regex");
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

        regex compiled;
        if (use_regex && !name_pattern.empty()) {
            compiled = regex(name_pattern);
        }

        pou::Metrics local_metrics;
        const auto matches = pou::traverse_matching(
            local_work,
            traversal,
            [&](const pou::Entry &entry) {
                if (name_pattern.empty()) {
                    return true;
                }
                const auto filename = entry.path.filename().string();
                if (use_regex) {
                    return regex_search(filename, compiled);
                }
                return filename.find(name_pattern) != string::npos;
            },
            local_metrics);

        local_metrics.elapsed_seconds = pou::now_seconds() - start;

        vector<string> local_lines;
        local_lines.reserve(matches.size());
        for (const auto &match : matches) {
            local_lines.push_back(match.path.string());
        }
        auto lines = pou::gather_lines_to_root(local_lines, 0);
        auto metrics = pou::reduce_metrics_to_root(local_metrics, 0);

        if (rank == 0) {
            sort(lines.begin(), lines.end());
            if (!no_output) {
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
                out << pou::metrics_csv_row(TOOL_NAME, name_pattern, ranks, effective_threads, metrics) << '\n';
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
