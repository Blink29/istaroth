# Distributed Parallel OS Utilities

Group: Paurush Kumar (NA22B002), Akshay Pratap Singh (CE21B006)

Project title: A Distributed and Parallel Implementation of Core OS Utilities: Scaling File System Traversal with OpenMPI and OpenMP

This repository contains parallel replacements for two common Linux utilities:

- `pfind`: file-system traversal with name/type matching, comparable to `find -name`.
- `pgrep`: recursive file-content search, comparable to `grep -RIl`.

The implementation is intentionally modular: MPI work distribution, OpenMP traversal, metrics, CLI parsing, and string search live in shared source files under `src/common`.

Each tool is built in three variants:

- `pfind_serial`, `pgrep_serial`: project serial baseline, no OpenMP and no MPI.
- `pfind_omp`, `pgrep_omp`: OpenMP-only version, no `mpirun` startup.
- `pfind`, `pgrep`: MPI + OpenMP hybrid version, launched with `mpirun`.

The benchmark plots use two baselines:

- primary speedup baseline: the project serial binaries.
- reference baseline: GNU `find` and `grep`.

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
report/                  Final ASME report source, class file, and PDF
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

## Final Report

The final report is in `report/`:

- `report/report.tex`: ASME-format LaTeX source
- `report/asme2e.cls`: ASME class file needed to compile the report
- `report/report.pdf`: compiled final report

The report references figures from the root `plots/` directory, so compile it from inside `report/`:

```bash
cd report
pdflatex -interaction=nonstopmode report.tex
pdflatex -interaction=nonstopmode report.tex
```

## Run the tools

Project serial baseline:

```bash
./build/pfind_serial --name target data/deep_synthetic
./build/pgrep_serial --pattern distributed_needle data/deep_synthetic
```

OpenMP-only version:

```bash
./build/pfind_omp --name target --threads 8 data/deep_synthetic
./build/pgrep_omp --pattern distributed_needle --threads 8 data/deep_synthetic
```

MPI + OpenMP version:

```bash
mpirun -np 1 ./build/pfind --name target --threads 4 data/deep_synthetic
mpirun -np 1 ./build/pgrep --pattern distributed_needle --threads 4 data/deep_synthetic
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

### Deep Traversal Dataset

This dataset stresses directory traversal and many-small-file scanning. It is the main `pfind` dataset.

```bash
./scripts/generate_dataset.py \
  --root data/deep_synthetic \
  --depth 7 \
  --fanout 4 \
  --files-per-dir 4 \
  --file-size-kib 8 \
  --force
```

### Large Content Dataset

This dataset stresses `pgrep` large-file scanning. Files are generated above the 1 MiB threshold, so `pgrep` uses the streaming Boyer-Moore search path.

```bash
./scripts/generate_dataset.py \
  --root data/large_content \
  --depth 5 \
  --fanout 3 \
  --files-per-dir 4 \
  --file-size-kib-min 1025 \
  --file-size-kib-max 4096 \
  --force
```

The generator supports either a fixed size:

```bash
--file-size-kib 16
```

or a random inclusive range:

```bash
--file-size-kib-min 1025 --file-size-kib-max 4096
```

For a more realistic text corpus, download public-domain Project Gutenberg books:

```bash
./scripts/download_gutenberg_dataset.py --root data/gutenberg --books 120 --force
```

This preserves real title/author words in filenames. Use natural `pfind` queries such as `war`, `science`, `pride`, or `sherlock`, and natural `pgrep` patterns such as `government`, `liberty`, `king`, or `england`.

## Run Benchmarks

The benchmark runner records GNU `find`/`grep`, project serial, OpenMP-only, and MPI+OpenMP modes in one normalized CSV.

### Correctness Check

Before timing a dataset, compare all project variants against GNU `find`/`grep`:

```bash
./benchmarks/check_correctness.py \
  --dataset data/deep_synthetic \
  --build-dir build \
  --threads 8 \
  --ranks 2
```

For the large-content dataset:

```bash
./benchmarks/check_correctness.py \
  --dataset data/large_content \
  --build-dir build \
  --threads 8 \
  --ranks 2
```

### Deep Traversal Benchmark

```bash
./benchmarks/run_benchmarks.py \
  --dataset data/deep_synthetic \
  --build-dir build \
  --output results/deep_synthetic_final.csv \
  --ranks 1 2 \
  --threads 1 2 4 8 \
  --repeats 3 \
  --overwrite
```

### Large Content Benchmark

```bash
./benchmarks/run_benchmarks.py \
  --dataset data/large_content \
  --build-dir build \
  --output results/large_content_final.csv \
  --ranks 1 2 \
  --threads 1 2 4 8 \
  --repeats 3 \
  --overwrite
```

The `mpi_ranks x omp_threads` labels mean:

- `pfind_omp` / `pgrep_omp` rows are always `1xN`; they are pure OpenMP and do not use `mpirun`.
- `pfind` / `pgrep` rows are MPI+OpenMP and are launched with `mpirun -np R`.

## Plot Results

Install Python plotting dependencies:

```bash
python3 -m pip install -r requirements.txt
```

Generate figures:

```bash
./benchmarks/plot_results.py \
  --input results/deep_synthetic_final.csv \
  --out-dir plots/deep_synthetic_final_wallclock \
  --time-column elapsed_seconds

./benchmarks/plot_results.py \
  --input results/deep_synthetic_final.csv \
  --out-dir plots/deep_synthetic_final_internal \
  --time-column internal_elapsed_seconds
```

For the large-content dataset:

```bash
./benchmarks/plot_results.py \
  --input results/large_content_final.csv \
  --out-dir plots/large_content_final_wallclock \
  --time-column elapsed_seconds

./benchmarks/plot_results.py \
  --input results/large_content_final.csv \
  --out-dir plots/large_content_final_internal \
  --time-column internal_elapsed_seconds
```

Generated plots:

- `runtime_by_configuration.png`
- `speedup_vs_serial.png`
- `pgrep_throughput.png`

Wall-clock plots include process launch overhead, including `mpirun` startup for hybrid runs. Internal-time plots use the tools' own measured traversal/search time and are useful for showing scaling separate from launch overhead.

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

Implementation notes:

- Traversal uses POSIX `opendir`, `readdir`, and `lstat` instead of `std::filesystem` directory iteration in the hot path.
- OpenMP parallelizes traversal across seed work items.
- `pgrep_omp` and hybrid `pgrep` also parallelize content search across files.
- For files up to 1 MiB, `pgrep` uses a buffered fixed-string search.
- For files larger than 1 MiB, `pgrep` streams 1 MiB chunks with pattern overlap and uses Boyer-Moore fixed-string search.
- Regex mode is still available with `--regex`, but fixed-string mode is the intended benchmark path.
- MPI rank 0 expands roots into seed work items, preserves expanded directories as depth-0 work, distributes seeds round-robin, and each rank performs local OpenMP work.
- Static MPI distribution is a clean baseline; dynamic work stealing is still a possible future extension for highly imbalanced trees.
