"""Download a nested Project Gutenberg text corpus for realistic benchmarks.

The script uses Gutendex for metadata and download links, then saves real book
text into a nested directory tree. Filenames preserve title and author words so
pfind can search natural names such as "war", "pride", "sherlock", or "science".
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


GUTENDEX = "https://gutendex.com/books/"
USER_AGENT = "ID5130-pfind-pgrep-benchmark/1.0"


def slug(value: str, max_len: int = 72) -> str:
    value = value.lower()
    value = re.sub(r"[^a-z0-9]+", "_", value)
    value = re.sub(r"_+", "_", value).strip("_")
    return (value or "unknown")[:max_len]


def fetch_json(url: str, retries: int = 3) -> dict:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(1, retries + 1):
        try:
            with urllib.request.urlopen(request, timeout=25) as response:
                return json.loads(response.read().decode("utf-8"))
        except (TimeoutError, urllib.error.URLError) as exc:
            if attempt == retries:
                raise
            print(f"retry metadata attempt={attempt} reason={exc}", flush=True)
            time.sleep(1.5 * attempt)
    raise RuntimeError("unreachable")


def fetch_text(url: str, retries: int = 3) -> str:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(1, retries + 1):
        try:
            with urllib.request.urlopen(request, timeout=45) as response:
                raw = response.read()
            return raw.decode("utf-8", errors="replace")
        except (TimeoutError, urllib.error.URLError) as exc:
            if attempt == retries:
                raise
            print(f"retry text attempt={attempt} reason={exc}", flush=True)
            time.sleep(1.5 * attempt)
    raise RuntimeError("unreachable")


def text_url(book: dict) -> str | None:
    formats = book.get("formats", {})
    preferred = [
        "text/plain; charset=utf-8",
        "text/plain; charset=us-ascii",
        "text/plain",
    ]
    for key in preferred:
        if key in formats:
            return formats[key]
    for key, value in formats.items():
        if key.startswith("text/plain"):
            return value
    return None


def author_name(book: dict) -> str:
    authors = book.get("authors") or []
    if not authors:
        return "Unknown Author"
    return authors[0].get("name") or "Unknown Author"


def make_output_path(root: Path, book: dict, index: int, fanout: int) -> Path:
    title = book.get("title") or f"book_{book['id']}"
    author = author_name(book)
    title_slug = slug(title)
    author_slug = slug(author)
    shard = f"shard_{index % fanout:03d}"
    filename = f"{title_slug}__{author_slug}__pg{book['id']}.txt"
    return root / shard / author_slug / filename


def query_url(term: str, page: int) -> str:
    params = {
        "languages": "en",
        "mime_type": "text/plain",
        "search": term,
        "page": str(page),
    }
    return f"{GUTENDEX}?{urllib.parse.urlencode(params)}"


def collect_books(search_terms: list[str], limit: int, delay: float) -> list[dict]:
    books: list[dict] = []
    seen: set[int] = set()

    for term in search_terms:
        page = 1
        while len(books) < limit:
            print(f"metadata term={term} page={page} collected={len(books)}", flush=True)
            data = fetch_json(query_url(term, page))
            results = data.get("results", [])
            if not results:
                break

            for book in results:
                if len(books) >= limit:
                    break
                book_id = int(book["id"])
                if book_id in seen or book.get("copyright") is True:
                    continue
                if text_url(book) is None:
                    continue
                seen.add(book_id)
                books.append(book)

            if not data.get("next"):
                break
            page += 1
            time.sleep(delay)

        if len(books) >= limit:
            break

    return books


def write_metadata(root: Path, books: list[dict], search_terms: list[str]) -> None:
    rows = ["id,title,author,download_count,languages,filename"]
    for index, book in enumerate(books):
        path = make_output_path(root, book, index, max(1, min(32, len(books))))
        title = (book.get("title") or "").replace('"', '""')
        author = author_name(book).replace('"', '""')
        languages = " ".join(book.get("languages") or [])
        rows.append(
            f'{book["id"]},"{title}","{author}",{book.get("download_count", 0)},"{languages}","{path.relative_to(root)}"'
        )

    (root / "_metadata.csv").write_text("\n".join(rows) + "\n", encoding="utf-8")
    (root / "_README.txt").write_text(
        "Project Gutenberg benchmark dataset\n"
        f"Books: {len(books)}\n"
        f"Search terms used for collection: {', '.join(search_terms)}\n"
        "Files are real public-domain book texts downloaded through Gutendex metadata links.\n"
        "Suggested pfind queries: war, science, history, love, pride, sherlock\n"
        "Suggested pgrep queries: government, liberty, science, king, england, war\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("data/gutenberg"))
    parser.add_argument("--books", type=int, default=120)
    parser.add_argument("--fanout", type=int, default=16)
    parser.add_argument(
        "--search-terms",
        default="war,science,history,love,sherlock,pride,government,adventure",
        help="Comma-separated Gutendex search terms used to collect real books.",
    )
    parser.add_argument("--delay", type=float, default=0.25)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--resume", action="store_true", help="Keep existing files and skip books already downloaded.")
    args = parser.parse_args()

    if args.root.exists():
        if args.force:
            shutil.rmtree(args.root)
        elif not args.resume:
            raise SystemExit(f"{args.root} already exists; pass --force to replace it")
    args.root.mkdir(parents=True, exist_ok=True)

    search_terms = [term.strip() for term in args.search_terms.split(",") if term.strip()]
    books = collect_books(search_terms, args.books, args.delay)
    if not books:
        raise SystemExit("No downloadable plain-text books found")

    downloaded = []
    for index, book in enumerate(books):
        url = text_url(book)
        if url is None:
            continue
        out_path = make_output_path(args.root, book, index, args.fanout)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        if args.resume and out_path.exists():
            downloaded.append(book)
            print(f"{len(downloaded):04d}/{args.books} exists id={book['id']} {book.get('title', '')}", flush=True)
            continue
        try:
            text = fetch_text(url)
        except urllib.error.URLError as exc:
            print(f"skip id={book['id']} reason={exc}")
            continue
        out_path.write_text(text, encoding="utf-8")
        downloaded.append(book)
        print(f"{len(downloaded):04d}/{args.books} id={book['id']} {book.get('title', '')}", flush=True)
        time.sleep(args.delay)

    write_metadata(args.root, downloaded, search_terms)
    print(f"dataset={args.root}")
    print(f"books={len(downloaded)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
