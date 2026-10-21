#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::render::ui {

struct CoverageBitmap {
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<std::uint8_t> pixels;
};

struct SdfBitmap {
    bool ok = false;
    std::string errorCode;
    int width = 0;
    int height = 0;
    int spread = 0;
    std::vector<std::uint8_t> pixels;
};

// Deterministic thresholded signed-distance field. 128 is the contour,
// values above are inside, and values below are outside.
SdfBitmap generateSignedDistanceField(const CoverageBitmap& source,
    int spread);

}  // namespace billiardgl::render::ui
