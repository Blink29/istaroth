#include "common/Traversal.hpp"

#ifdef USE_OPENMP
#include <omp.h>
#endif

#include <deque>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

namespace pou {
namespace {

bool type_matches(const Entry &entry, EntryType type) {
    if (type == EntryType::Any) {
        return true;
    }
    if (type == EntryType::File) {
        return !entry.is_directory;
    }
    return entry.is_directory;
}

void traverse_one(const WorkItem &item,
                  const TraverseOptions &options,
                  const function<bool(const Entry &)> &predicate,
                  vector<Entry> &matches,
                  Metrics &metrics) {
    deque<pair<fs::path, int>> pending;
    pending.push_back({item.path, item.max_depth});

    while (!pending.empty()) {
        auto current = options.mode == TraversalMode::Bfs ? pending.front() : pending.back();
        if (options.mode == TraversalMode::Bfs) {
            pending.pop_front();
        } else {
            pending.pop_back();
        }

        error_code ec;
        const bool is_dir = fs::is_directory(current.first, ec);
        if (ec) {
            ++metrics.errors;
            continue;
        }

        Entry entry{current.first, is_dir, 0};
        if (is_dir) {
            ++metrics.directories;
        } else {
            ++metrics.files;
            const auto size = fs::file_size(current.first, ec);
            if (!ec) {
                entry.size = size;
            }
        }

        if (type_matches(entry, options.type) && predicate(entry)) {
            matches.push_back(entry);
            ++metrics.matches;
        }

        if (!is_dir || current.second == 0) {
            continue;
        }

        const int child_depth = current.second < 0 ? -1 : current.second - 1;
        for (const auto &child : fs::directory_iterator(current.first, fs::directory_options::skip_permission_denied, ec)) {
            pending.push_back({child.path(), child_depth});
        }
        if (ec) {
            ++metrics.errors;
        }
    }
}

} 

TraversalMode parse_traversal_mode(const string &value) {
    if (value == "bfs") {
        return TraversalMode::Bfs;
    }
    if (value == "dfs") {
        return TraversalMode::Dfs;
    }
    throw runtime_error("--traversal must be bfs or dfs");
}

EntryType parse_entry_type(const string &value) {
    if (value == "any") {
        return EntryType::Any;
    }
    if (value == "f" || value == "file") {
        return EntryType::File;
    }
    if (value == "d" || value == "dir" || value == "directory") {
        return EntryType::Directory;
    }
    throw runtime_error("--type must be any, f, or d");
}

vector<Entry> traverse_matching(const vector<WorkItem> &work,
                                     const TraverseOptions &options,
                                     const function<bool(const Entry &)> &predicate,
                                     Metrics &metrics) {
#ifdef USE_OPENMP
    if (options.omp_threads > 0) {
        omp_set_num_threads(options.omp_threads);
    }

    vector<Entry> all_matches;

#pragma omp parallel
    {
        vector<Entry> local_matches;
        Metrics local_metrics;

#pragma omp for schedule(dynamic)
        for (int64_t i = 0; i < static_cast<int64_t>(work.size()); ++i) {
            traverse_one(work[static_cast<size_t>(i)], options, predicate, local_matches, local_metrics);
        }

#pragma omp critical
        {
            all_matches.insert(all_matches.end(), local_matches.begin(), local_matches.end());
            metrics.directories += local_metrics.directories;
            metrics.files += local_metrics.files;
            metrics.bytes_read += local_metrics.bytes_read;
            metrics.matches += local_metrics.matches;
            metrics.errors += local_metrics.errors;
        }
    }

    return all_matches;
#else
    (void)options;
    vector<Entry> all_matches;
    for (const auto &item : work) {
        traverse_one(item, options, predicate, all_matches, metrics);
    }
    return all_matches;
#endif
}

} 
