#pragma once

#include "common/Types.hpp"

namespace pou {

class Cli {
public:
    Cli(int argc, char **argv);

    bool has(const string &name) const;
    optional<string> value(const string &name) const;
    string value_or(const string &name, const string &fallback) const;
    int int_value_or(const string &name, int fallback) const;
    vector<string> positional() const;

private:
    vector<string> args_;
};

} 
