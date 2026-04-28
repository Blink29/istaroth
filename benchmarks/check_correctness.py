#!/usr/bin/env python3
"""Compare pfind/pgrep variants against GNU find/grep on one dataset."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


def run_lines(cmd: list[str]) -> list[str]:
    completed = subprocess.run(cmd, text=True, capture_output=True, check=True)
    return sorted(line for line in completed.stdout.splitlines() if line)


def compare(label: str, expected: list[str], actual: list[str]) -> bool:
    if expected == actual:
        print(f"OK {label}: {len(actual)} matches")
        return True

    expected_set = set(expected)
    actual_set = set(actual)
    missing = sorted(expected_set - actual_set)[:5]
    extra = sorted(actual_set - expected_set)[:5]
    print(f"FAIL {label}: expected={len(expected)} actual={len(actual)}")
    if missing:
        print("  missing:")
        for line in missing:
            print(f"    {line}")
    if extra:
        print("  extra:")
        for line in extra:
            print(f"    {line}")
    return False


def project_cmd(executable: Path,
                query_arg: str,
                query: str,
                dataset: Path,
                threads: int,
                ranks: int | None) -> list[str]:
    cmd: list[str] = []
    if ranks is not None:
        cmd.extend(["mpirun", "-np", str(ranks)])
    cmd.extend([str(executable), query_arg, query, "--threads", str(threads), str(dataset)])
    return cmd


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", type=Path, default=Path("data/generated"))
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--query", default="target")
    parser.add_argument("--pattern", default="distributed_needle")
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--ranks", type=int, default=2)
    parser.add_argument("--skip-mpi", action="store_true")
    args = parser.parse_args()

    expected_find = run_lines(["find", str(args.dataset), "-name", f"*{args.query}*"])
    expected_grep = run_lines(["grep", "-RIl", "--", args.pattern, str(args.dataset)])

    checks = [
        ("pfind_serial", project_cmd(args.build_dir / "pfind_serial", "--name", args.query, args.dataset, 1, None), expected_find),
        ("pgrep_serial", project_cmd(args.build_dir / "pgrep_serial", "--pattern", args.pattern, args.dataset, 1, None), expected_grep),
        ("pfind_omp", project_cmd(args.build_dir / "pfind_omp", "--name", args.query, args.dataset, args.threads, None), expected_find),
        ("pgrep_omp", project_cmd(args.build_dir / "pgrep_omp", "--pattern", args.pattern, args.dataset, args.threads, None), expected_grep),
    ]
    if not args.skip_mpi:
        checks.extend([
            ("pfind_mpi_omp", project_cmd(args.build_dir / "pfind", "--name", args.query, args.dataset, args.threads, args.ranks), expected_find),
            ("pgrep_mpi_omp", project_cmd(args.build_dir / "pgrep", "--pattern", args.pattern, args.dataset, args.threads, args.ranks), expected_grep),
        ])

    ok = True
    for label, cmd, expected in checks:
        ok = compare(label, expected, run_lines(cmd)) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
