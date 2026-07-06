#pragma once

#include <string>
#include <vector>

namespace billiardgl {

struct ImageData {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char> pixels;
    std::string error;
};

ImageData loadImageFile(const std::string& path);

}  // namespace billiardgl
