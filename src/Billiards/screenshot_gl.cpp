#define GLEW_STATIC
#include <GL/glew.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "screenshot.h"

namespace billiardgl {

bool saveFramebufferToPpm(const std::string& path, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 3));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0]);

    std::vector<unsigned char> flipped(static_cast<std::size_t>(width * height * 3));
    const int rowSize = width * 3;
    for (int y = 0; y < height; ++y) {
        const int sourceRow = y;
        const int destRow = height - 1 - y;
        for (int x = 0; x < rowSize; ++x) {
            flipped[static_cast<std::size_t>(destRow * rowSize + x)] =
                pixels[static_cast<std::size_t>(sourceRow * rowSize + x)];
        }
    }

    return hasVisiblePixels(flipped) && writePpm(path, width, height, flipped);
}

}  // namespace billiardgl
