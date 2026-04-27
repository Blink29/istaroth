#include "common/Cli.hpp"

#include <set>
#include <stdexcept>

using namespace std;

namespace pou {

Cli::Cli(int argc, char **argv) {
    args_.reserve(static_cast<size_t>(argc));
    for (int i = 1; i < argc; ++i) {
        args_.emplace_back(argv[i]);
    }
}

bool Cli::has(const string &name) const {
    const string long_name = "--" + name;
    for (const auto &arg : args_) {
        if (arg == long_name || arg.rfind(long_name + "=", 0) == 0) {
            return true;
        }
    }
    return false;
}

optional<string> Cli::value(const string &name) const {
    const string long_name = "--" + name;
    for (size_t i = 0; i < args_.size(); ++i) {
        const auto &arg = args_[i];
        if (arg.rfind(long_name + "=", 0) == 0) {
            return arg.substr(long_name.size() + 1);
        }
        if (arg == long_name && i + 1 < args_.size()) {
            return args_[i + 1];
        }
    }
    return nullopt;
}

string Cli::value_or(const string &name, const string &fallback) const {
    return value(name).value_or(fallback);
}

int Cli::int_value_or(const string &name, int fallback) const {
    const auto parsed = value(name);
    if (!parsed) {
        return fallback;
    }
    try {
        return stoi(*parsed);
    } catch (const exception &) {
        throw runtime_error("--" + name + " expects an integer");
    }
}

vector<string> Cli::positional() const {
    static const set<string> options_with_values = {
        "--max-depth",
        "--max-file-bytes",
        "--metrics",
        "--name",
        "--pattern",
        "--threads",
        "--traversal",
        "--type",
    };

    vector<string> out;
    for (size_t i = 0; i < args_.size(); ++i) {
        const auto &arg = args_[i];
        if (arg.rfind("--", 0) == 0) {
            if (options_with_values.count(arg) > 0 && i + 1 < args_.size()
                && args_[i + 1].rfind("--", 0) != 0) {
                ++i;
            }
            continue;
        }
        out.push_back(arg);
    }
    return out;
}

} 
