"""Create benchmark plots from results/benchmarks.csv."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def save_runtime_plot(df: pd.DataFrame, out_dir: Path, time_col: str, title_suffix: str) -> None:
    project = df[~df["tool"].isin(["find", "grep"])].copy()
    grouped = project.groupby(["tool", "mpi_ranks", "omp_threads"], as_index=False)[time_col].mean()
    grouped = grouped.sort_values(["mpi_ranks", "omp_threads", "tool"])
    configs = (
        grouped[["mpi_ranks", "omp_threads"]]
        .drop_duplicates()
        .sort_values(["mpi_ranks", "omp_threads"])
    )
    labels = [f"{int(row.mpi_ranks)}x{int(row.omp_threads)}" for row in configs.itertuples()]

    plt.figure(figsize=(9, 5))
    for tool, tool_df in grouped.groupby("tool"):
        tool_df = configs.merge(tool_df, on=["mpi_ranks", "omp_threads"], how="left")
        plt.plot(labels, tool_df[time_col], marker="o", label=tool)

    baselines = {
        "find baseline": df[df["tool"] == "find"][time_col].mean(),
        "grep baseline": df[df["tool"] == "grep"][time_col].mean(),
    }
    baseline_colors = {
        "find baseline": "tab:blue",
        "grep baseline": "tab:orange",
    }
    for label, value in baselines.items():
        if not pd.isna(value):
            plt.axhline(value, linestyle="--", linewidth=1.2, color=baseline_colors[label], label=label)

    plt.ylabel(f"Mean {title_suffix} time (s)")
    plt.xlabel("MPI ranks x OpenMP threads")
    plt.xticks(rotation=35, ha="right")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "runtime_by_configuration.png", dpi=180)
    plt.close()


def save_speedup_plot(df: pd.DataFrame, out_dir: Path, time_col: str) -> None:
    project = df[~df["tool"].isin(["find", "grep"])].copy()
    baselines = {
        "pfind": df[df["tool"] == "find"][time_col].mean(),
        "pgrep": df[df["tool"] == "grep"][time_col].mean(),
    }
    if any(pd.isna(value) for value in baselines.values()):
        return
    grouped = project.groupby(["tool", "mpi_ranks", "omp_threads"], as_index=False)[time_col].mean()
    grouped["family"] = grouped["tool"].apply(lambda value: "pfind" if value.startswith("pfind") else "pgrep")
    grouped["speedup"] = grouped.apply(lambda row: baselines[row["family"]] / row[time_col], axis=1)

    plt.figure(figsize=(9, 5))
    for tool, tool_df in grouped.groupby("tool"):
        label = tool_df.apply(lambda row: f"{int(row.mpi_ranks)}x{int(row.omp_threads)}", axis=1)
        plt.plot(label, tool_df["speedup"], marker="o", label=tool)
    plt.axhline(1.0, color="black", linewidth=0.8, linestyle="--")
    plt.ylabel("Speedup vs serial utility")
    plt.xlabel("MPI ranks x OpenMP threads")
    plt.xticks(rotation=35, ha="right")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_dir / "speedup_vs_serial.png", dpi=180)
    plt.close()


def save_throughput_plot(df: pd.DataFrame, out_dir: Path, time_col: str) -> None:
    pgrep = df[df["tool"].str.startswith("pgrep")].copy()
    if pgrep.empty or "bytes_read" not in pgrep:
        return
    pgrep["mb_per_second"] = pd.to_numeric(pgrep["bytes_read"], errors="coerce") / 1_000_000 / pgrep[time_col]
    grouped = pgrep.groupby(["mpi_ranks", "omp_threads"], as_index=False)["mb_per_second"].mean()

    plt.figure(figsize=(9, 5))
    labels = grouped.apply(lambda row: f"{int(row.mpi_ranks)}x{int(row.omp_threads)}", axis=1)
    plt.bar(labels, grouped["mb_per_second"])
    plt.ylabel("Mean pgrep throughput (MB/s)")
    plt.xlabel("MPI ranks x OpenMP threads")
    plt.xticks(rotation=35, ha="right")
    plt.tight_layout()
    plt.savefig(out_dir / "pgrep_throughput.png", dpi=180)
    plt.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=Path("results/benchmarks.csv"))
    parser.add_argument("--out-dir", type=Path, default=Path("plots"))
    parser.add_argument(
        "--time-column",
        choices=["elapsed_seconds", "internal_elapsed_seconds"],
        default="elapsed_seconds",
    )
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    df = pd.read_csv(args.input)
    df["elapsed_seconds"] = pd.to_numeric(df["elapsed_seconds"], errors="coerce")
    if args.time_column not in df.columns:
        raise SystemExit(f"{args.input} does not contain {args.time_column}")
    df[args.time_column] = pd.to_numeric(df[args.time_column], errors="coerce")
    df["mpi_ranks"] = pd.to_numeric(df["mpi_ranks"], errors="coerce")
    df["omp_threads"] = pd.to_numeric(df["omp_threads"], errors="coerce")
    df = df.dropna(subset=[args.time_column, "mpi_ranks", "omp_threads"])

    title_suffix = "wall-clock" if args.time_column == "elapsed_seconds" else "internal"
    save_runtime_plot(df, args.out_dir, args.time_column, title_suffix)
    save_speedup_plot(df, args.out_dir, args.time_column)
    save_throughput_plot(df, args.out_dir, args.time_column)
    print(f"plots={args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
