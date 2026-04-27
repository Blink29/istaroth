#pragma once

#include "common/Types.hpp"

namespace pou {

struct Metrics {
    uint64_t directories = 0;
    uint64_t files = 0;
    uint64_t bytes_read = 0;
    uint64_t matches = 0;
    uint64_t errors = 0;
    double elapsed_seconds = 0.0;
};

double now_seconds();
Metrics reduce_metrics_to_root(const Metrics &local, int root);
string metrics_csv_header();
string metrics_csv_row(const string &tool,
                       const string &query,
                       int mpi_ranks,
                       int omp_threads,
                       const Metrics &metrics);

} 
