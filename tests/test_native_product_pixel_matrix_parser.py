"""Binary PPM boundary regressions for the native pixel audit reader."""

import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from run_native_product_pixel_matrix import read_ppm


class NativePixelPpmReaderTests(unittest.TestCase):
    def read(self, payload):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "frame.ppm"
            path.write_bytes(payload)
            return read_ppm(path)

    def test_leading_raster_whitespace_is_not_header(self):
        for first in (9, 10, 13, 32):
            with self.subTest(first=first):
                pixels = bytes((first, 10, 32))
                image = self.read(b"P6\n1 1\n255\n" + pixels)
                self.assertEqual(image.pixels, pixels)

    def test_header_comments_do_not_change_raster(self):
        image = self.read(b"P6\n# capture\n2 1\n255\n" + bytes(range(6)))
        self.assertEqual((image.width, image.height), (2, 1))
        self.assertEqual(image.rgb(1, 0), (3, 4, 5))

    def test_invalid_dimensions_and_payload_rejected(self):
        for payload in (
            b"P6\n0 1\n255\n",
            b"P6\n-1 -1\n255\nabc",
            b"P6\n1 1\n255\nab",
            b"P6\n1 1\n255\nabcd",
            b"P6\n1 1\n256\nabc",
            b"P6\n1 1\n255",
        ):
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                self.read(payload)


if __name__ == "__main__":
    unittest.main()
