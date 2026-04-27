#pragma once

#include "common/Types.hpp"

namespace pou {

struct WorkItem {
    FsPath path;
    int max_depth = -1;
};

vector<WorkItem> build_seed_work(const vector<FsPath> &roots,
                                 int target_items,
                                 int max_depth);

vector<WorkItem> distribute_work(const vector<WorkItem> &all_work,
                                 int root_rank);

vector<string> gather_lines_to_root(const vector<string> &local,
                                    int root_rank);

} 
