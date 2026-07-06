#include "assets.h"

#include "resource_path.h"

namespace billiardgl {

AssetPaths getDefaultAssetPaths()
{
    return AssetPaths{
        objectPath("table.obj"),
        objectPath("cue.obj"),
        objectPath("bench.obj"),
        objectPath("wardrobe.obj")
    };
}

std::string getTexturePath(const char* name)
{
    return texturePath(name);
}

std::string getTexturePath(const std::string& name)
{
    return texturePath(name);
}

std::string getObjectPath(const char* name)
{
    return objectPath(name);
}

std::string getObjectPath(const std::string& name)
{
    return objectPath(name);
}

std::string getAudioPath(const char* name)
{
    return audioPath(name);
}

std::string getAudioPath(const std::string& name)
{
    return audioPath(name);
}

}  // namespace billiardgl
