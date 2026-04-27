#include "common/StringSearch.hpp"

#ifdef USE_OPENMP
#include <omp.h>
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <vector>

using namespace std;
namespace fs = filesystem;

namespace pou {
namespace {

string lower_copy(string value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
    return value;
}

bool contains_parallel(const string &text, const string &needle) {
    if (needle.empty()) {
        return true;
    }
    if (text.size() < needle.size()) {
        return false;
    }

#ifdef USE_OPENMP
    int found = 0;
    const size_t chunk_count = static_cast<size_t>(max(1, omp_get_max_threads()));
    const size_t chunk = (text.size() + chunk_count - 1) / chunk_count;
    const size_t overlap = needle.size() - 1;

#pragma omp parallel for schedule(static) if(text.size() > 1024 * 1024 && !omp_in_parallel())
    for (int64_t i = 0; i < static_cast<int64_t>(chunk_count); ++i) {
        if (found) {
            continue;
        }
        const size_t begin = static_cast<size_t>(i) * chunk;
        if (begin >= text.size()) {
            continue;
        }
        const size_t end = min(text.size(), begin + chunk + overlap);
        if (text.find(needle, begin) < end) {
#pragma omp atomic write
            found = 1;
        }
    }

    return found != 0;
#else
    return text.find(needle) != string::npos;
#endif
}

} 

bool file_contains(const fs::path &path,
                   const SearchOptions &options,
                   Metrics &metrics) {
    error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec || size > options.max_file_bytes) {
        ++metrics.errors;
        return false;
    }

    ifstream in(path, ios::binary);
    if (!in) {
        ++metrics.errors;
        return false;
    }

    string content(static_cast<size_t>(size), '\0');
    in.read(content.data(), static_cast<streamsize>(content.size()));
    metrics.bytes_read += static_cast<uint64_t>(in.gcount());
    if (!in && !in.eof()) {
        ++metrics.errors;
        return false;
    }

    string pattern = options.pattern;
    if (options.ignore_case) {
        content = lower_copy(move(content));
        pattern = lower_copy(move(pattern));
    }

    if (options.regex) {
        try {
            const auto flags = options.ignore_case
                                   ? regex::ECMAScript | regex::icase
                                   : regex::ECMAScript;
            return regex_search(content, regex(pattern, flags));
        } catch (const regex_error &) {
            ++metrics.errors;
            return false;
        }
    }

    return contains_parallel(content, pattern);
}

}
