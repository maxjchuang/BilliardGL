#!/usr/bin/env python3
"""macOS lossless filesystem compression; keep evidence paths and content hashes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", action="append", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    suffixes = {".jsonl", ".ndjson", ".json", ".bmp", ".raw", ".bgll", ".bgll13", ".bgll15", ".wav", ".stderr"}
    count = saved = 0
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open("x") as report:
        for root in args.root:
            for parent, dirs, files in os.walk(root):
                dirs[:] = [name for name in dirs if name not in {".git", "__pycache__"}]
                for name in files:
                    path = Path(parent) / name
                    if path.is_symlink() or path.suffix not in suffixes:
                        continue
                    before = path.stat()
                    if before.st_size < 1024 * 1024 or before.st_nlink != 1 or getattr(before, "st_flags", 0) & 0x20:
                        continue
                    row = {"path": str(path.resolve()), "bytes": before.st_size, "allocated_before": before.st_blocks * 512}
                    if not args.apply:
                        row["status"] = "PLANNED"
                    else:
                        original_sha = digest(path)
                        fd, temporary = tempfile.mkstemp(prefix=".bgl-compact-", dir=parent)
                        os.close(fd)
                        temp = Path(temporary)
                        try:
                            subprocess.run(["ditto", "--noclone", "--hfsCompression", str(path), str(temp)], check=True, capture_output=True)
                            after = temp.stat()
                            if digest(temp) != original_sha:
                                raise RuntimeError("Copy content hash mismatch: " + str(path))
                            current = path.stat()
                            if (current.st_size, current.st_mtime_ns, current.st_ino) != (before.st_size, before.st_mtime_ns, before.st_ino):
                                raise RuntimeError("Source changed while compacting: " + str(path))
                            if after.st_blocks < before.st_blocks:
                                os.replace(temp, path)
                                row.update(status="COMPRESSED_SAME_BYTES", sha256=original_sha, allocated_after=after.st_blocks * 512)
                                saved += (before.st_blocks - after.st_blocks) * 512
                            else:
                                row.update(status="UNCHANGED_NO_SAVING", sha256=original_sha, allocated_after=before.st_blocks * 512)
                        finally:
                            temp.unlink(missing_ok=True)
                    report.write(json.dumps(row) + "\n")
                    report.flush()
                    count += 1
                    if count % 100 == 0:
                        os.fsync(report.fileno())
                        print(json.dumps({"processed": count, "saved_bytes": saved}), flush=True)
        report.write(json.dumps({"summary": True, "processed": count, "saved_bytes": saved, "apply": args.apply}) + "\n")
        report.flush()
        os.fsync(report.fileno())
    print(json.dumps({"processed": count, "saved_bytes": saved, "apply": args.apply}))


if __name__ == "__main__":
    main()
