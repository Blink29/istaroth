# Distributed Parallel OS Utilities

Group: Paurush Kumar (NA22B002), Akshay Pratap Singh (CE21B006)

Project title: A Distributed and Parallel Implementation of Core OS Utilities: Scaling File System Traversal with OpenMPI and OpenMP

This repository contains two hybrid MPI/OpenMP utilities:

- `pfind`: distributed and threaded file-system traversal with name/type matching.
- `pgrep`: distributed and threaded traversal plus parallel file-content search.

The implementation is intentionally modular: MPI work distribution, OpenMP traversal, metrics, CLI parsing, and string search live in shared source files under `src/common`.

## Layout

```text
include/common/          Shared C++ headers
src/common/              Shared traversal, MPI, metrics, search, CLI code
src/tools/pfind.cpp      pfind executable
src/tools/pgrep.cpp      pgrep executable
scripts/                 Dataset generation
benchmarks/              Benchmark runner and plotting scripts
docs/PROJECT_PLAN.md     Implementation and evaluation plan
results/                 CSV outputs from benchmark runs
plots/                   Generated figures
```

## Build

Requirements:

- CMake 3.16+
- C++17 compiler
- OpenMPI or another MPI implementation with `mpicxx`
- OpenMP-enabled compiler

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run the tools

Single machine, one MPI rank:

```bash
mpirun -np 1 ./build/pfind --name target --threads 4 data/generated
mpirun -np 1 ./build/pgrep --pattern distributed_needle --threads 4 data/generated
```

Two-node style run, assuming MPI host configuration is already available:

```bash
mpirun -np 4 --host node1,node2 ./build/pfind --name target --threads 4 /shared/dataset
mpirun -np 4 --host node1,node2 ./build/pgrep --pattern distributed_needle --threads 4 /shared/dataset
```

Both tools support:

- `--traversal bfs|dfs`
- `--max-depth N`
- `--threads N`
- `--metrics results/run.csv`
- `--no-output`

## Generate Benchmark Data

```bash
./scripts/generate_dataset.py --root data/generated --depth 5 --fanout 4 --files-per-dir 8 --file-size-kib 16 --force
```

This creates a nested directory tree with predictable target filenames and target text content.

For a more realistic text corpus, download public-domain Project Gutenberg books:

```bash
./scripts/download_gutenberg_dataset.py --root data/gutenberg --books 120 --force
```

This preserves real title/author words in filenames. Use natural `pfind` queries such as `war`, `science`, `pride`, or `sherlock`, and natural `pgrep` patterns such as `government`, `liberty`, `king`, or `england`.

## Run Benchmarks

```bash
./benchmarks/run_benchmarks.py \
  --dataset data/generated \
  --build-dir build \
  --output results/benchmarks.csv \
  --ranks 1 2 \
  --threads 1 2 4 \
  --repeats 3
```

Example with the Gutenberg dataset:

```bash
./benchmarks/run_benchmarks.py \
  --dataset data/gutenberg \
  --build-dir build \
  --output results/gutenberg_benchmarks.csv \
  --query war \
  --pattern government \
  --ranks 1 2 \
  --threads 1 2 4 \
  --repeats 3
```

The benchmark runner records serial `find` and `grep` baselines, then runs `pfind` and `pgrep` across MPI rank and OpenMP thread configurations.

## Plot Results

Install Python plotting dependencies:

```bash
python3 -m pip install -r requirements.txt
```

Generate figures:

```bash
./benchmarks/plot_results.py --input results/benchmarks.csv --out-dir plots
```

Generated plots:

- `runtime_by_configuration.png`
- `speedup_vs_serial.png`
- `pgrep_throughput.png`

## Metrics

The CSV schema is:

```text
tool,query,mpi_ranks,omp_threads,repeat,elapsed_seconds,internal_elapsed_seconds,directories,files,bytes_read,matches,errors
```

Core metrics:

- elapsed wall-clock time measured around the whole command
- internal elapsed time reported by `pfind`/`pgrep`, reduced by max rank time
- directories visited
- files visited
- bytes read by `pgrep`
- match count
- permission/read/regex errors

## Notes

The current MPI strategy expands the input root into seed work items on rank 0, distributes those seed paths round-robin across ranks, and then each rank performs local OpenMP traversal. This is a clean baseline for the course project and can be extended with dynamic work stealing if the generated or real datasets are highly imbalanced.
