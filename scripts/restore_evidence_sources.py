#!/usr/bin/env python3
"""Restore exact research sources from the maintenance checkpoint (no raw evidence)."""
import argparse
import hashlib
import json
import io
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=Path(__file__).resolve().parents[1] / "docs/recovery/20261021-maintenance/evidence-source-snapshot.zip")
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--prefix", default="docs/evidence/")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    root = args.destination.resolve()
    count = 0
    if args.archive.exists():
        source = args.archive
    else:
        parts = sorted(args.archive.parent.glob(args.archive.name + ".part*"))
        if not parts:
            raise FileNotFoundError(args.archive)
        source = io.BytesIO(b"".join(part.read_bytes() for part in parts))
    with zipfile.ZipFile(source) as archive:
        index_name = "INDEX-v2.json" if "INDEX-v2.json" in archive.namelist() else "INDEX.json"
        index = json.loads(archive.read(index_name))
        for row in index["files"]:
            if not row["path"].startswith(args.prefix):
                continue
            target = (root / row["path"]).resolve()
            if not target.is_relative_to(root):
                raise ValueError("Archive path escapes destination")
            data = archive.read("objects/" + row["sha256"])
            if hashlib.sha256(data).hexdigest() != row["sha256"] or len(data) != row["bytes"]:
                raise ValueError("Archive object integrity failure: " + row["path"])
            if target.exists():
                if hashlib.sha256(target.read_bytes()).hexdigest() != row["sha256"]:
                    raise FileExistsError("Refusing to overwrite different content: " + str(target))
            elif not args.verify_only:
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open("xb") as output:
                    output.write(data)
                target.chmod(row["mode"])
            count += 1
    print(json.dumps({"verified_files": count, "verify_only": args.verify_only}))


if __name__ == "__main__":
    main()
