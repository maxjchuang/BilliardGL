#include "render/ui/sdf_generator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace billiardgl::render::ui {
namespace {

bool sourceInside(const CoverageBitmap& source, int x, int y)
{
    if (x < 0 || y < 0 || x >= source.width || y >= source.height)
        return false;
    return source.pixels[static_cast<std::size_t>(y * source.stride + x)] >= 128;
}

}  // namespace

SdfBitmap generateSignedDistanceField(const CoverageBitmap& source,
    int spread)
{
    SdfBitmap result;
    if (source.width <= 0 || source.height <= 0 ||
        source.stride < source.width || spread <= 0 || spread > 64 ||
        source.pixels.size() < static_cast<std::size_t>(
            source.stride * source.height)) {
        result.errorCode = "sdf_invalid_bitmap";
        return result;
    }
    result.width = source.width + spread * 2;
    result.height = source.height + spread * 2;
    result.spread = spread;
    result.pixels.resize(static_cast<std::size_t>(result.width * result.height));
    const int maximumDistanceSquared = spread * spread;
    for (int outputY = 0; outputY < result.height; ++outputY) {
        for (int outputX = 0; outputX < result.width; ++outputX) {
            const int sourceX = outputX - spread;
            const int sourceY = outputY - spread;
            const bool inside = sourceInside(source, sourceX, sourceY);
            int nearestSquared = maximumDistanceSquared;
            for (int dy = -spread; dy <= spread; ++dy) {
                for (int dx = -spread; dx <= spread; ++dx) {
                    const int squared = dx * dx + dy * dy;
                    if (squared >= nearestSquared || squared > maximumDistanceSquared)
                        continue;
                    if (sourceInside(source, sourceX + dx, sourceY + dy) != inside)
                        nearestSquared = squared;
                }
            }
            const float distance = std::sqrt(static_cast<float>(nearestSquared));
            const float signedDistance = inside ? distance : -distance;
            const float normalized = 128.0f + signedDistance /
                static_cast<float>(spread) * 127.0f;
            result.pixels[static_cast<std::size_t>(
                outputY * result.width + outputX)] =
                static_cast<std::uint8_t>(std::round(
                    std::max(0.0f, std::min(255.0f, normalized))));
        }
    }
    result.ok = true;
    return result;
}

}  // namespace billiardgl::render::ui
