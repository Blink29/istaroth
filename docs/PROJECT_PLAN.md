# Project Plan

## Objective

Build distributed replacements for `find` and `grep` that use:

- OpenMPI for inter-node work distribution.
- OpenMP for intra-node directory traversal and file scanning.
- CSV metrics and Python plots for repeatable performance evaluation.

## Architecture

1. Rank 0 expands the root path list into seed work items.
2. Seed work is distributed round-robin to MPI ranks.
3. Each rank uses OpenMP to traverse assigned seed paths.
4. `pfind` applies filename/type predicates during traversal.
5. `pgrep` first collects candidate files, then scans file contents in parallel.
6. Matches are gathered to rank 0 for sorted output.
7. Metrics are reduced to rank 0 and optionally appended to CSV.

## Source Modules

- `Cli`: minimal shared `--option value` and `--option=value` parser.
- `MpiWork`: seed generation, MPI scatter, and match gathering.
- `Traversal`: BFS/DFS traversal and OpenMP scheduling.
- `StringSearch`: file loading and substring/regex matching.
- `Metrics`: wall-clock timing, MPI reductions, and CSV formatting.
- `pfind`: filename and type search executable.
- `pgrep`: content search executable.

## Implementation Milestones

1. Build skeleton with CMake, MPI, and OpenMP.
2. Implement distributed seed generation and scatter.
3. Implement local BFS/DFS traversal.
4. Implement `pfind` name/type matching.
5. Implement `pgrep` file-content matching.
6. Add metrics collection and CSV output.
7. Add synthetic dataset generator.
8. Add benchmark runner against serial `find` and `grep`.
9. Add plots for runtime, speedup, and throughput.
10. Run final two-node experiments and write analysis.

## Evaluation Plan

### Independent Variables

- MPI ranks: `1, 2, 4, ...` depending on cluster size.
- OpenMP threads per rank: `1, 2, 4, 8`.
- Traversal strategy: `bfs`, `dfs`.
- Dataset depth and fanout.
- File size and number of target files.

### Dependent Metrics

- Wall-clock runtime.
- Speedup over serial Linux utility.
- Directory traversal rate.
- File scanning throughput in MB/s.
- Match count correctness.
- Error count from inaccessible or oversized files.

### Recommended Experiments

1. Fixed dataset, vary OpenMP threads with one MPI rank.
2. Fixed dataset, vary MPI ranks with one thread per rank.
3. Full hybrid scaling with both ranks and threads.
4. BFS versus DFS comparison on deeply nested trees.
5. `pgrep` throughput comparison across small and large files.

## Expected Plots

- Runtime by MPI-rank/OpenMP-thread configuration.
- Speedup compared with serial `find` and `grep`.
- `pgrep` throughput in MB/s.

## Risks and Extensions

- Static round-robin seed assignment can underperform on highly skewed trees.
- Network filesystems may dominate runtime and hide CPU parallelism.
- Regex matching is slower and may not scale like fixed substring matching.
- Future improvement: add dynamic MPI work stealing for unbalanced trees.
- Future improvement: add mmap-based file reading for large files.
- Future improvement: add correctness tests against `find`/`grep` on generated datasets.
