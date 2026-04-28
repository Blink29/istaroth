#include "common/MpiWork.hpp"

#ifdef USE_MPI
#include <mpi.h>
#endif

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <deque>
#include <cstring>
#include <sstream>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

namespace pou {
namespace {

string serialize_work(const vector<WorkItem> &items) {
    ostringstream out;
    for (const auto &item : items) {
        out << item.max_depth << '\t' << item.path.string() << '\n';
    }
    return out.str();
}

vector<WorkItem> deserialize_work(const string &text) {
    vector<WorkItem> items;
    istringstream in(text);
    string line;
    while (getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto tab = line.find('\t');
        if (tab == string::npos) {
            continue;
        }
        items.push_back({fs::path(line.substr(tab + 1)), stoi(line.substr(0, tab))});
    }
    return items;
}

bool expandable(const WorkItem &item) {
    if (item.max_depth == 0) {
        return false;
    }

    struct stat st {};
    return lstat(item.path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

vector<WorkItem> list_children(const fs::path &path, int child_depth) {
    vector<WorkItem> children;
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return children;
    }

    while (dirent *child = readdir(dir)) {
        const char *name = child->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        children.push_back({path / name, child_depth});
    }
    closedir(dir);
    return children;
}

}

vector<WorkItem> build_seed_work(const vector<fs::path> &roots,
                                      int target_items,
                                      int max_depth) {
    deque<WorkItem> frontier;
    for (const auto &root : roots) {
        frontier.push_back({root, max_depth});
    }
    if (frontier.empty()) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            throw runtime_error("failed to get current working directory");
        }
        frontier.push_back({fs::path(cwd), max_depth});
    }

    while (static_cast<int>(frontier.size()) < target_items) {
        bool expanded_one = false;
        const size_t rounds = frontier.size();
        for (size_t i = 0; i < rounds; ++i) {
            WorkItem item = frontier.front();
            frontier.pop_front();
            if (!expanded_one && expandable(item)) {
                const auto child_depth = item.max_depth < 0 ? -1 : item.max_depth - 1;
                const auto children = list_children(item.path, child_depth);
                if (!children.empty()) {
                    frontier.push_back({item.path, 0});
                    for (const auto &child : children) {
                        frontier.push_back(child);
                    }
                    expanded_one = true;
                    break;
                }
            }
            frontier.push_back(item);
        }
        if (!expanded_one) {
            break;
        }
    }

    return vector<WorkItem>(frontier.begin(), frontier.end());
}

vector<WorkItem> distribute_work(const vector<WorkItem> &all_work,
                                      int root_rank) {
#ifdef USE_MPI
    int rank = 0;
    int ranks = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);

    vector<int> sizes(static_cast<size_t>(ranks), 0);
    vector<int> offsets(static_cast<size_t>(ranks), 0);
    string packed;

    if (rank == root_rank) {
        vector<vector<WorkItem>> buckets(static_cast<size_t>(ranks));
        for (size_t i = 0; i < all_work.size(); ++i) {
            buckets[i % static_cast<size_t>(ranks)].push_back(all_work[i]);
        }

        for (int r = 0; r < ranks; ++r) {
            const auto segment = serialize_work(buckets[static_cast<size_t>(r)]);
            offsets[static_cast<size_t>(r)] = static_cast<int>(packed.size());
            sizes[static_cast<size_t>(r)] = static_cast<int>(segment.size());
            packed += segment;
        }
    }

    int my_size = 0;
    MPI_Scatter(sizes.data(), 1, MPI_INT, &my_size, 1, MPI_INT, root_rank, MPI_COMM_WORLD);
    string mine(static_cast<size_t>(my_size), '\0');
    MPI_Scatterv(packed.data(), sizes.data(), offsets.data(), MPI_CHAR,
                 mine.data(), my_size, MPI_CHAR, root_rank, MPI_COMM_WORLD);
    return deserialize_work(mine);
#else
    (void)root_rank;
    return all_work;
#endif
}

vector<string> gather_lines_to_root(const vector<string> &local,
                                              int root_rank) {
#ifdef USE_MPI
    int rank = 0;
    int ranks = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);

    ostringstream local_stream;
    for (const auto &line : local) {
        local_stream << line << '\n';
    }
    const auto local_text = local_stream.str();
    const int local_size = static_cast<int>(local_text.size());

    vector<int> sizes(static_cast<size_t>(ranks), 0);
    MPI_Gather(&local_size, 1, MPI_INT, sizes.data(), 1, MPI_INT, root_rank, MPI_COMM_WORLD);

    vector<int> offsets(static_cast<size_t>(ranks), 0);
    string packed;
    if (rank == root_rank) {
        int total = 0;
        for (int r = 0; r < ranks; ++r) {
            offsets[static_cast<size_t>(r)] = total;
            total += sizes[static_cast<size_t>(r)];
        }
        packed.resize(static_cast<size_t>(total));
    }

    MPI_Gatherv(local_text.data(), local_size, MPI_CHAR,
                packed.data(), sizes.data(), offsets.data(), MPI_CHAR,
                root_rank, MPI_COMM_WORLD);

    vector<string> lines;
    if (rank == root_rank) {
        istringstream in(packed);
        string line;
        while (getline(in, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
    }
    return lines;
#else
    (void)root_rank;
    return local;
#endif
}

} 
