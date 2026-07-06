#include "screenshot.h"

#include <fstream>

namespace billiardgl {

bool writePpm(const std::string& path, int width, int height, const std::vector<unsigned char>& rgb)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (rgb.size() != static_cast<std::size_t>(width * height * 3)) {
        return false;
    }

    std::ofstream file(path.c_str(), std::ios::binary);
    if (!file) {
        return false;
    }

    file << "P6\n" << width << " " << height << "\n255\n";
    file.write(reinterpret_cast<const char*>(&rgb[0]), static_cast<std::streamsize>(rgb.size()));
    return static_cast<bool>(file);
}

bool hasVisiblePixels(const std::vector<unsigned char>& rgb)
{
    for (unsigned char value : rgb) {
        if (value > 8) {
            return true;
        }
    }
    return false;
}

int countVisiblePixelsInRegion(const std::vector<unsigned char>& rgb, int width, int height, int x, int y, int regionWidth, int regionHeight)
{
    int count = 0;
    const int xEnd = x + regionWidth;
    const int yEnd = y + regionHeight;
    for (int row = y; row < yEnd && row < height; ++row) {
        if (row < 0) {
            continue;
        }
        for (int col = x; col < xEnd && col < width; ++col) {
            if (col < 0) {
                continue;
            }
            const int index = (row * width + col) * 3;
            if (index + 2 < static_cast<int>(rgb.size()) &&
                (rgb[index] > 8 || rgb[index + 1] > 8 || rgb[index + 2] > 8)) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace billiardgl
