#pragma once

#include <string>
#include <vector>

namespace billiardgl::render {

struct FormalAssetBinding {
    std::string id;
    std::string canonicalPath;
    std::string sha256;
};

struct FormalAssetLoadError {
    std::string code;
    std::string file;
    std::string field;
    std::string message;
};

struct FormalAssetPreflightResult {
    bool ok = false;
    bool silentMaterialFallbackAllowed = false;
    std::string canonicalAssetRoot;
    std::vector<FormalAssetBinding> requiredAssets;
    std::vector<FormalAssetBinding> runtimeDependencies;
    std::vector<FormalAssetLoadError> errors;
};

FormalAssetPreflightResult preflightFormalAssetPackage(
    const std::string& assetRoot);
std::string formatFormalAssetLoadError(const FormalAssetLoadError& error);

}  // namespace billiardgl::render
