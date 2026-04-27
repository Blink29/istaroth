#include "common/Metrics.hpp"

#ifdef USE_MPI
#include <mpi.h>
#endif

#include <chrono>
#include <iomanip>
#include <sstream>

using namespace std;

namespace pou {

double now_seconds() {
    using clock = chrono::steady_clock;
    const auto now = clock::now().time_since_epoch();
    return chrono::duration<double>(now).count();
}

Metrics reduce_metrics_to_root(const Metrics &local, int root) {
#ifdef USE_MPI
    Metrics reduced;
    uint64_t send_counts[5] = {
        local.directories,
        local.files,
        local.bytes_read,
        local.matches,
        local.errors,
    };
    uint64_t recv_counts[5] = {0, 0, 0, 0, 0};

    MPI_Reduce(send_counts, recv_counts, 5, MPI_UNSIGNED_LONG_LONG, MPI_SUM, root, MPI_COMM_WORLD);
    MPI_Reduce(&local.elapsed_seconds, &reduced.elapsed_seconds, 1, MPI_DOUBLE, MPI_MAX, root, MPI_COMM_WORLD);

    reduced.directories = recv_counts[0];
    reduced.files = recv_counts[1];
    reduced.bytes_read = recv_counts[2];
    reduced.matches = recv_counts[3];
    reduced.errors = recv_counts[4];
    return reduced;
#else
    (void)root;
    return local;
#endif
}

string metrics_csv_header() {
    return "tool,query,mpi_ranks,omp_threads,elapsed_seconds,directories,files,bytes_read,matches,errors";
}

string metrics_csv_row(const string &tool,
                            const string &query,
                            int mpi_ranks,
                            int omp_threads,
                            const Metrics &metrics) {
    ostringstream out;
    out << tool << ','
        << '"' << query << '"' << ','
        << mpi_ranks << ','
        << omp_threads << ','
        << fixed << setprecision(6) << metrics.elapsed_seconds << ','
        << metrics.directories << ','
        << metrics.files << ','
        << metrics.bytes_read << ','
        << metrics.matches << ','
        << metrics.errors;
    return out.str();
}

} 
