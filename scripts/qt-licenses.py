"""Copy license notices without extracting untrusted archive paths."""
import sys
import tarfile
from pathlib import Path, PurePosixPath

archive, output = Path(sys.argv[1]), Path(sys.argv[2])
with tarfile.open(archive) as source:
    for entry in source:
        parts = PurePosixPath(entry.name).parts
        if not entry.isfile() or len(parts) != 3 or parts[1] != "LICENSES":
            continue
        target = output / parts[2]
        if target.parent.resolve() != output.resolve():
            raise ValueError("Unexpected license filename")
        output.mkdir(parents=True, exist_ok=True)
        target.write_bytes(source.extractfile(entry).read())
