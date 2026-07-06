#include "screenshot.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main()
{
    const std::string path = "/tmp/billiardgl-screenshot-test.ppm";
    const std::vector<unsigned char> rgb = {
        0, 0, 0,
        255, 0, 0,
        0, 128, 0,
        0, 0, 255,
    };

    if (!billiardgl::writePpm(path, 2, 2, rgb)) {
        return fail("writePpm should write a valid image");
    }

    std::ifstream file(path.c_str(), std::ios::binary);
    std::string magic;
    int width = 0;
    int height = 0;
    int maxValue = 0;
    file >> magic >> width >> height >> maxValue;
    if (magic != "P6" || width != 2 || height != 2 || maxValue != 255) {
        return fail("PPM header should match written dimensions");
    }

    if (!billiardgl::hasVisiblePixels(rgb)) {
        return fail("visible pixel check should detect non-black image data");
    }

    const std::vector<unsigned char> black(12, 0);
    if (billiardgl::hasVisiblePixels(black)) {
        return fail("visible pixel check should reject all-black image data");
    }

    return EXIT_SUCCESS;
}
