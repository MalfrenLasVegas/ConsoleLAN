"""Fetch the pinned, unmodified MSYS2 C ABI runtime and corresponding sources.

Requires Python 3.10+ and zstandard==0.25.0 (build-time only).
Only five runtime DLLs are shipped, in a private nat/ directory.
"""
import argparse
import hashlib
import io
import json
import lzma
import pathlib
import tarfile
import urllib.request

try:
    import zstandard
except ImportError:
    raise SystemExit("Run: python -m pip install zstandard==0.25.0")

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEST = ROOT / "deps" / "slirp"
MANIFEST = json.loads((ROOT / "scripts" / "internet-deps.json").read_text(encoding="utf-8"))


def fetch(name, sha, source=False):
    folder = DEST / "sources" if source else DEST / "packages"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / name
    if not path.exists():
        url = "https://mirror.msys2.org/mingw/" + ("sources/" if source else "clang64/") + name
        print("Downloading", name, flush=True)
        temporary = path.with_suffix(".download")
        urllib.request.urlretrieve(url, temporary)
        temporary.replace(path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != sha:
        raise SystemExit("SHA256 mismatch: " + name)
    return path


def members(path):
    with path.open("rb") as raw, zstandard.ZstdDecompressor().stream_reader(raw) as stream:
        with tarfile.open(fileobj=stream, mode="r|") as archive:
            for entry in archive:
                if entry.isfile():
                    yield entry.name, archive.extractfile(entry).read()


def write(relative, data):
    path = (DEST / relative).resolve()
    if DEST.resolve() not in path.parents:
        raise SystemExit("Unsafe archive path: " + str(relative))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def licenses_from_source(path, name):
    for filename, data in members(path):
        if filename.endswith((".tar.bz2", ".tar.xz", ".tar.gz", ".tar.lz")):
            if filename.endswith(".lz"):
                # GNU lzip member: 6-byte header, raw LZMA1 stream, 20-byte trailer.
                if data[:5] != b"LZIP\x01":
                    raise SystemExit("Unsupported lzip header")
                code = data[5]
                dictionary = 1 << (code & 31)
                dictionary -= dictionary // 16 * ((code >> 5) & 7)
                data = lzma.decompress(data[6:-20], format=lzma.FORMAT_RAW,
                    filters=[dict(id=lzma.FILTER_LZMA1, dict_size=dictionary, lc=3, lp=0, pb=2)])
            with tarfile.open(fileobj=io.BytesIO(data)) as upstream:
                for entry in upstream:
                    basename = pathlib.PurePosixPath(entry.name).name.upper()
                    if entry.isfile() and basename.startswith(("COPYING", "LICENSE", "COPYRIGHT", "AUTHORS")):
                        write(pathlib.Path("licenses") / name / entry.name, upstream.extractfile(entry).read())


def main():
    args = argparse.ArgumentParser()
    args.add_argument("--sources", action="store_true")
    args = args.parse_args()
    for item in MANIFEST:
        package = fetch(item["package"], item["sha256"])
        for name, data in members(package):
            if name.startswith(("clang64/include/slirp/", "clang64/share/licenses/")) or name in item["files"]:
                write(pathlib.Path(name), data)
        if args.sources:
            source = fetch(item["source"], item["sourceSha256"], True)
            licenses_from_source(source, item["name"])
    print("Internet runtime verified; no system network configuration changed.")


if __name__ == "__main__":
    main()
