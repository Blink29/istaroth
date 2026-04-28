#include "common/StringSearch.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
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

void normalize_in_place(string &value) {
    transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(tolower(ch));
    });
}

bool stream_contains_fixed(ifstream &in,
                           const string &pattern,
                           bool ignore_case,
                           Metrics &metrics) {
    if (pattern.empty()) {
        return true;
    }

    string needle = ignore_case ? lower_copy(pattern) : pattern;
    const auto searcher = boyer_moore_searcher(needle.begin(), needle.end());
    vector<char> buffer(1024 * 1024);
    string carry;
    string window;
    const size_t overlap = needle.size() - 1;
    window.reserve(buffer.size() + overlap);

    while (in) {
        in.read(buffer.data(), static_cast<streamsize>(buffer.size()));
        const auto got = in.gcount();
        if (got <= 0) {
            break;
        }
        metrics.bytes_read += static_cast<uint64_t>(got);

        window.assign(carry);
        window.append(buffer.data(), static_cast<size_t>(got));
        if (ignore_case) {
            normalize_in_place(window);
        }

        if (search(window.begin(), window.end(), searcher) != window.end()) {
            return true;
        }

        if (overlap == 0) {
            carry.clear();
        } else if (window.size() <= overlap) {
            carry = window;
        } else {
            carry.assign(window.end() - static_cast<ptrdiff_t>(overlap), window.end());
        }
    }

    return false;
}

bool buffer_contains_fixed(string content, const string &pattern, bool ignore_case) {
    if (pattern.empty()) {
        return true;
    }
    string needle = pattern;
    if (ignore_case) {
        normalize_in_place(content);
        needle = lower_copy(move(needle));
    }
    return content.find(needle) != string::npos;
}

optional<string> read_file(const fs::path &path, uintmax_t size, Metrics &metrics) {
    ifstream in(path, ios::binary);
    if (!in) {
        ++metrics.errors;
        return std::nullopt;
    }

    string content(static_cast<size_t>(size), '\0');
    in.read(content.data(), static_cast<streamsize>(content.size()));
    metrics.bytes_read += static_cast<uint64_t>(in.gcount());
    if (!in && !in.eof()) {
        ++metrics.errors;
        return std::nullopt;
    }
    content.resize(static_cast<size_t>(in.gcount()));
    return content;
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

    string pattern = options.pattern;

    if (options.regex) {
        auto content = read_file(path, size, metrics);
        if (!content) {
            return false;
        }
        try {
            const auto flags = options.ignore_case
                                   ? regex::ECMAScript | regex::icase
                                   : regex::ECMAScript;
            return regex_search(*content, regex(pattern, flags));
        } catch (const regex_error &) {
            ++metrics.errors;
            return false;
        }
    }

    constexpr uintmax_t streaming_threshold = 1024 * 1024;
    if (size <= streaming_threshold) {
        auto content = read_file(path, size, metrics);
        return content && buffer_contains_fixed(move(*content), pattern, options.ignore_case);
    }

    ifstream in(path, ios::binary);
    if (!in) {
        ++metrics.errors;
        return false;
    }
    const bool found = stream_contains_fixed(in, pattern, options.ignore_case, metrics);
    if (in.bad()) {
        ++metrics.errors;
        return false;
    }
    return found;
}

}
