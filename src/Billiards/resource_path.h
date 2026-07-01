#pragma once

#include <string>

namespace billiardgl {

std::string resourcePath(const std::string& relativePath);
std::string texturePath(const std::string& fileName);
std::string objectPath(const std::string& fileName);
std::string audioPath(const std::string& fileName);

}  // namespace billiardgl
