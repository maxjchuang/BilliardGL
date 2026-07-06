#include "image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "dependencies/include/stb_image.h"

namespace billiardgl {

ImageData loadImageFile(const std::string& path)
{
    ImageData image;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr) {
        image.error = stbi_failure_reason() ? stbi_failure_reason() : "unknown image loading error";
        return image;
    }

    image.width = width;
    image.height = height;
    image.channels = 4;
    image.pixels.assign(pixels, pixels + width * height * 4);
    stbi_image_free(pixels);
    return image;
}

}  // namespace billiardgl
