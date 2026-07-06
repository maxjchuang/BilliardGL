#pragma once

#include <string>
#include <vector>

namespace billiardgl {

bool writePpm(const std::string& path, int width, int height, const std::vector<unsigned char>& rgb);
bool hasVisiblePixels(const std::vector<unsigned char>& rgb);
int countVisiblePixelsInRegion(const std::vector<unsigned char>& rgb, int width, int height, int x, int y, int regionWidth, int regionHeight);
bool saveFramebufferToPpm(const std::string& path, int width, int height);

}  // namespace billiardgl
