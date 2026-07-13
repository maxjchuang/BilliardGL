#!/usr/bin/env python3

import importlib.util
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SCANNER_PATH = REPO_ROOT / "scripts" / "check_text_encoding.py"


def load_scanner():
    if not SCANNER_PATH.exists():
        raise AssertionError(f"encoding scanner does not exist: {SCANNER_PATH}")
    spec = importlib.util.spec_from_file_location("check_text_encoding", SCANNER_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class CheckTextEncodingTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self):
        self.temporary_directory.cleanup()

    def write_bytes(self, relative_path, data):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def test_accepts_utf8_and_skips_nul_binary(self):
        scanner = load_scanner()
        self.write_bytes("valid.txt", "中文 UTF-8\n".encode("utf-8"))
        self.write_bytes("asset.bin", b"\xff\x00\xfe")

        self.assertEqual(
            [], scanner.find_violations(self.root, ["valid.txt", "asset.bin"])
        )

    def test_reports_invalid_utf8_byte_offset(self):
        scanner = load_scanner()
        self.write_bytes("legacy.h", b"prefix\n\xce\xc6")

        self.assertEqual(
            ["legacy.h: invalid UTF-8 at byte 7"],
            scanner.find_violations(self.root, ["legacy.h"]),
        )

    def test_reports_replacement_character_and_known_mojibake(self):
        scanner = load_scanner()
        self.write_bytes(
            "broken.cpp", "\ufffd and \u951f\u65a4\u62f7\n".encode("utf-8")
        )

        self.assertEqual(
            [
                "broken.cpp: contains suspicious token U+FFFD",
                "broken.cpp: contains suspicious token U+951F U+65A4 U+62F7",
            ],
            scanner.find_violations(self.root, ["broken.cpp"]),
        )


if __name__ == "__main__":
    unittest.main()
