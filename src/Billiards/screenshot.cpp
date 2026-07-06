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

}  // namespace billiardgl
