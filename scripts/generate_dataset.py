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
    alphabet = string.ascii_lowercase + "     \n"
    needle_at = rng.randrange(max(1, target_bytes - len(needle))) if include_needle else -1
    chunk_size = 256 * 1024
    written = 0

    with path.open("w", encoding="utf-8") as handle:
        while written < target_bytes:
            remaining = target_bytes - written
            current_size = min(chunk_size, remaining)
            chunk = "".join(rng.choices(alphabet, k=current_size))

            if include_needle and written <= needle_at < written + current_size:
                offset = needle_at - written
                chunk = chunk[:offset] + needle + chunk[offset + len(needle):]

            handle.write(chunk)
            written += current_size


def populate_dir(
    root: Path,
    depth: int,
    fanout: int,
    files_per_dir: int,
    file_size_kib_min: int,
    file_size_kib_max: int,
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
        size_kib = rng.randint(file_size_kib_min, file_size_kib_max)
        make_file(root / f"{stem}.txt", size_kib, needle, is_target, rng)

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
            file_size_kib_min,
            file_size_kib_max,
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
    parser.add_argument("--file-size-kib-min", type=int)
    parser.add_argument("--file-size-kib-max", type=int)
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

    file_size_kib_min = args.file_size_kib_min if args.file_size_kib_min is not None else args.file_size_kib
    file_size_kib_max = args.file_size_kib_max if args.file_size_kib_max is not None else args.file_size_kib
    if file_size_kib_min <= 0 or file_size_kib_max <= 0:
        raise SystemExit("file sizes must be positive")
    if file_size_kib_min > file_size_kib_max:
        raise SystemExit("--file-size-kib-min must be <= --file-size-kib-max")

    rng = random.Random(args.seed)
    populate_dir(
        args.root,
        args.depth,
        args.fanout,
        args.files_per_dir,
        file_size_kib_min,
        file_size_kib_max,
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
