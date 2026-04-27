#pragma once

#include "common/Metrics.hpp"
#include "common/MpiWork.hpp"

namespace pou {

enum class TraversalMode {
    Bfs,
    Dfs
};

enum class EntryType {
    Any,
    File,
    Directory
};

struct TraverseOptions {
    TraversalMode mode = TraversalMode::Bfs;
    EntryType type = EntryType::Any;
    int omp_threads = 0;
};

struct Entry {
    FsPath path;
    bool is_directory = false;
    uintmax_t size = 0;
};

TraversalMode parse_traversal_mode(const string &value);
EntryType parse_entry_type(const string &value);

vector<Entry> traverse_matching(const vector<WorkItem> &work,
                                const TraverseOptions &options,
                                const function<bool(const Entry &)> &predicate,
                                Metrics &metrics);

} 
