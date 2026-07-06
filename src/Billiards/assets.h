#pragma once

#include <string>

namespace billiardgl {

struct AssetPaths {
    std::string tableObj;
    std::string cueObj;
    std::string benchObj;
    std::string wardrobeObj;
};

AssetPaths getDefaultAssetPaths();
std::string getTexturePath(const char* name);
std::string getTexturePath(const std::string& name);
std::string getObjectPath(const char* name);
std::string getObjectPath(const std::string& name);
std::string getAudioPath(const char* name);
std::string getAudioPath(const std::string& name);

}  // namespace billiardgl
