"""Run serial and hybrid pfind/pgrep benchmarks and write normalized CSV."""

from __future__ import annotations

import argparse
import csv
import subprocess
import tempfile
import time
from pathlib import Path


FIELDNAMES = [
    "tool",
    "query",
    "mpi_ranks",
    "omp_threads",
    "repeat",
    "elapsed_seconds",
    "internal_elapsed_seconds",
    "directories",
    "files",
    "bytes_read",
    "matches",
    "errors",
]


def run_command(cmd: list[str]) -> tuple[float, str]:
    start = time.perf_counter()
    completed = subprocess.run(cmd, text=True, capture_output=True, check=True)
    return time.perf_counter() - start, completed.stdout


def append_rows(path: Path, rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    exists = path.exists()
    with path.open("a", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDNAMES)
        if not exists:
            writer.writeheader()
        writer.writerows(rows)


def serial_find(dataset: Path, query: str, repeat: int) -> dict[str, str]:
    elapsed, stdout = run_command(["find", str(dataset), "-name", f"*{query}*"])
    matches = len([line for line in stdout.splitlines() if line])
    return {
        "tool": "find",
        "query": query,
        "mpi_ranks": "1",
        "omp_threads": "1",
        "repeat": str(repeat),
        "elapsed_seconds": f"{elapsed:.6f}",
        "internal_elapsed_seconds": f"{elapsed:.6f}",
        "directories": "",
        "files": "",
        "bytes_read": "",
        "matches": str(matches),
        "errors": "0",
    }


def serial_grep(dataset: Path, pattern: str, repeat: int) -> dict[str, str]:
    elapsed, stdout = run_command(["grep", "-RIl", "--", pattern, str(dataset)])
    matches = len([line for line in stdout.splitlines() if line])
    return {
        "tool": "grep",
        "query": pattern,
        "mpi_ranks": "1",
        "omp_threads": "1",
        "repeat": str(repeat),
        "elapsed_seconds": f"{elapsed:.6f}",
        "internal_elapsed_seconds": f"{elapsed:.6f}",
        "directories": "",
        "files": "",
        "bytes_read": "",
        "matches": str(matches),
        "errors": "0",
    }


def project_tool(
    executable: Path,
    dataset: Path,
    query_arg: str,
    query: str,
    ranks: int,
    threads: int,
    repeat: int,
    use_mpi: bool,
) -> dict[str, str]:
    with tempfile.NamedTemporaryFile("r", delete=False, suffix=".csv") as tmp:
        metrics_path = Path(tmp.name)

    cmd = []
    if use_mpi:
        cmd.extend(["mpirun", "-np", str(ranks)])
    cmd.extend([
        str(executable),
        query_arg,
        query,
        "--threads",
        str(threads),
        "--metrics",
        str(metrics_path),
        "--no-output",
        str(dataset),
    ])
    external_elapsed, _ = run_command(cmd)

    with metrics_path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    metrics_path.unlink(missing_ok=True)
    if not rows:
        raise RuntimeError(f"{executable} did not write metrics")

    row = rows[-1]
    row["repeat"] = str(repeat)
    row["internal_elapsed_seconds"] = row.get("elapsed_seconds", "")
    row["elapsed_seconds"] = f"{external_elapsed:.6f}"
    return {name: row.get(name, "") for name in FIELDNAMES}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", type=Path, default=Path("data/generated"))
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output", type=Path, default=Path("results/benchmarks.csv"))
    parser.add_argument("--query", default="target")
    parser.add_argument("--pattern", default="distributed_needle")
    parser.add_argument("--ranks", type=int, nargs="+", default=[1, 2])
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4])
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument(
        "--modes",
        nargs="+",
        choices=["gnu", "serial", "omp", "mpi_omp"],
        default=["gnu", "serial", "omp", "mpi_omp"],
    )
    args = parser.parse_args()

    executables = {
        "serial": (args.build_dir / "pfind_serial", args.build_dir / "pgrep_serial"),
        "omp": (args.build_dir / "pfind_omp", args.build_dir / "pgrep_omp"),
        "mpi_omp": (args.build_dir / "pfind", args.build_dir / "pgrep"),
    }
    for mode in args.modes:
        if mode == "gnu":
            continue
        for executable in executables[mode]:
            if not executable.exists():
                raise SystemExit(f"build first: missing {executable}")

    for repeat in range(args.repeats):
        rows: list[dict[str, str]] = []
        if "gnu" in args.modes:
            rows.append(serial_find(args.dataset, args.query, repeat))
            rows.append(serial_grep(args.dataset, args.pattern, repeat))

        if "serial" in args.modes:
            pfind_serial, pgrep_serial = executables["serial"]
            rows.append(project_tool(pfind_serial, args.dataset, "--name", args.query, 1, 1, repeat, False))
            rows.append(project_tool(pgrep_serial, args.dataset, "--pattern", args.pattern, 1, 1, repeat, False))

        if "omp" in args.modes:
            pfind_omp, pgrep_omp = executables["omp"]
            for threads in args.threads:
                rows.append(project_tool(pfind_omp, args.dataset, "--name", args.query, 1, threads, repeat, False))
                rows.append(project_tool(pgrep_omp, args.dataset, "--pattern", args.pattern, 1, threads, repeat, False))

        if "mpi_omp" in args.modes:
            pfind, pgrep = executables["mpi_omp"]
            for ranks in args.ranks:
                for threads in args.threads:
                    rows.append(project_tool(pfind, args.dataset, "--name", args.query, ranks, threads, repeat, True))
                    rows.append(project_tool(pgrep, args.dataset, "--pattern", args.pattern, ranks, threads, repeat, True))

        append_rows(args.output, rows)
        print(f"repeat={repeat} rows={len(rows)} output={args.output}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
