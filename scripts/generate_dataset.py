"""Generate nested directories and text files for pfind/pgrep benchmarks."""

from __future__ import annotations

import argparse
import random
import shutil
import string
from pathlib import Path


def random_word(rng: random.Random, length: int = 8) -> str:
    return "".join(rng.choice(string.ascii_lowercase) for _ in range(length))


def make_file(path: Path, size_kib: int, needle: str, include_needle: bool, rng: random.Random) -> None:
    target_bytes = size_kib * 1024
    words = []
    written = 0
    while written < target_bytes:
        word = random_word(rng, rng.randint(4, 12))
        words.append(word)
        written += len(word) + 1
    if include_needle and words:
        words[rng.randrange(len(words))] = needle
    path.write_text(" ".join(words) + "\n", encoding="utf-8")


def populate_dir(
    root: Path,
    depth: int,
    fanout: int,
    files_per_dir: int,
    file_size_kib: int,
    target_name: str,
    needle: str,
    target_every: int,
    rng: random.Random,
    counter: list[int],
) -> None:
    root.mkdir(parents=True, exist_ok=True)

    for file_idx in range(files_per_dir):
        current = counter[0]
        counter[0] += 1
        is_target = current % target_every == 0
        stem = f"{target_name}_{current:06d}" if is_target else f"file_{current:06d}"
        make_file(root / f"{stem}.txt", file_size_kib, needle, is_target, rng)

    if depth == 0:
        return

    for dir_idx in range(fanout):
        current = counter[0]
        is_target_dir = current % target_every == 0
        name = f"{target_name}_dir_{current:06d}" if is_target_dir else f"dir_{depth}_{dir_idx}"
        populate_dir(
            root / name,
            depth - 1,
            fanout,
            files_per_dir,
            file_size_kib,
            target_name,
            needle,
            target_every,
            rng,
            counter,
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("data/generated"))
    parser.add_argument("--depth", type=int, default=5)
    parser.add_argument("--fanout", type=int, default=4)
    parser.add_argument("--files-per-dir", type=int, default=8)
    parser.add_argument("--file-size-kib", type=int, default=16)
    parser.add_argument("--target-name", default="target")
    parser.add_argument("--needle", default="distributed_needle")
    parser.add_argument("--target-every", type=int, default=11)
    parser.add_argument("--seed", type=int, default=5130)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    if args.root.exists():
        if not args.force:
            raise SystemExit(f"{args.root} already exists; pass --force to replace it")
        shutil.rmtree(args.root)

    rng = random.Random(args.seed)
    populate_dir(
        args.root,
        args.depth,
        args.fanout,
        args.files_per_dir,
        args.file_size_kib,
        args.target_name,
        args.needle,
        args.target_every,
        rng,
        [0],
    )
    print(f"dataset={args.root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
