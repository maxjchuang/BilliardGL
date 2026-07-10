#include "screenshot.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

int processId()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

std::string temporaryScreenshotPath()
{
    const char* tmpDir = std::getenv("TMPDIR");
    if (tmpDir == NULL || tmpDir[0] == '\0') {
        tmpDir = "/tmp";
    }

    std::ostringstream path;
    path << tmpDir;
    const std::string directory = path.str();
    if (!directory.empty() && directory[directory.size() - 1] != '/') {
        path << "/";
    }
    path << "billiardgl-screenshot-test-" << processId() << ".ppm";
    return path.str();
}

}  // namespace

int main()
{
    const std::string path = temporaryScreenshotPath();
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

    const std::vector<unsigned char> regionImage = {
        0, 0, 0,   0, 0, 0,    0, 0, 0,
        0, 0, 0,   5, 6, 7,    0, 0, 0,
        0, 0, 0,   0, 0, 0,    9, 10, 11,
    };
    if (billiardgl::countVisiblePixelsInRegion(regionImage, 3, 3, 1, 1, 2, 2) != 1) {
        return fail("region visible pixel count should detect visible pixels in bounds");
    }

    return EXIT_SUCCESS;
}
