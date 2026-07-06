#include "assets.h"
#include "image_loader.h"

#include <cstdlib>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

}  // namespace

int main()
{
    const billiardgl::ImageData missing = billiardgl::loadImageFile("/tmp/billiardgl-file-that-does-not-exist.png");
    if (missing.error.empty()) {
        return fail("missing image should return a clear error");
    }

    const billiardgl::ImageData ball = billiardgl::loadImageFile(billiardgl::getTexturePath("B16.bmp"));
    if (!ball.error.empty()) {
        return fail("existing BMP texture should load through image loader");
    }
    if (ball.width <= 0 || ball.height <= 0 || ball.channels != 4 || ball.pixels.empty()) {
        return fail("loaded BMP texture should have dimensions and RGBA pixels");
    }

    return EXIT_SUCCESS;
}
