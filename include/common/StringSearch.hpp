#pragma once

#include "common/Metrics.hpp"

namespace pou {

struct SearchOptions {
    string pattern;
    bool regex = false;
    bool ignore_case = false;
    uint64_t max_file_bytes = 512ULL * 1024ULL * 1024ULL;
};

bool file_contains(const FsPath &path,
                   const SearchOptions &options,
                   Metrics &metrics);

} 
