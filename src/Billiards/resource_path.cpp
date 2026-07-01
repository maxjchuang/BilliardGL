#include "resource_path.h"

#include <string>

#ifndef BILLIARDGL_ASSET_ROOT
#define BILLIARDGL_ASSET_ROOT "."
#endif

namespace billiardgl {

namespace {

std::string joinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) {
        return right;
    }
    const char last = left[left.size() - 1];
    if (last == '/' || last == '\\') {
        return left + right;
    }
    return left + "/" + right;
}

}  // namespace

std::string resourcePath(const std::string& relativePath)
{
    return joinPath(BILLIARDGL_ASSET_ROOT, relativePath);
}

std::string texturePath(const std::string& fileName)
{
    return resourcePath(joinPath("tex", fileName));
}

std::string objectPath(const std::string& fileName)
{
    return resourcePath(joinPath("obj", fileName));
}

std::string audioPath(const std::string& fileName)
{
    return resourcePath(joinPath("audio", fileName));
}

}  // namespace billiardgl
