#pragma once

#include "render/modern/gpu_resources.h"
#include "render/modern/pbr_materials.h"

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace billiardgl::render {

struct UploadedTexture {
    Texture2D texture;
    TextureEncoding encoding = TextureEncoding::LinearRgba8;
    int width = 0;
    int height = 0;
    int mipLevels = 0;
    std::size_t gpuBytes = 0;
};

struct UploadedPbrMaterial {
    UploadedTexture baseColor;
    UploadedTexture normal;
    UploadedTexture orm;
    std::array<float, 2> uvRepeat{{1.0f, 1.0f}};
    float roughnessFallback = 0.5f;
    float metallicFallback = 0.0f;
    float clearcoatFallback = 0.0f;
    bool errorFallback = false;
};

struct PbrUploadReport {
    bool loadedPackage = false;
    bool anisotropySupported = false;
    float anisotropy = 1.0f;
    int uploadedTextureCount = 0;
    std::size_t uploadedGpuBytes = 0;
    std::size_t budgetGpuBytes = 0;
    std::size_t environmentGpuBytes = 0;
    int environmentResolution = 0;
    std::vector<std::string> warnings;
};

class PbrTextureResources {
public:
    PbrTextureResources() = default;
    PbrTextureResources(const PbrTextureResources&) = delete;
    PbrTextureResources& operator=(const PbrTextureResources&) = delete;
    PbrTextureResources(PbrTextureResources&&) noexcept = default;
    PbrTextureResources& operator=(PbrTextureResources&&) noexcept = default;

    void load(const PbrPackageDescription& package, int environmentResolution);
    void reset() noexcept;
    const UploadedPbrMaterial* material(const std::string& name) const noexcept;
    const UploadedTexture& ballAtlas() const noexcept { return ballAtlas_; }
    const Sampler& sampler() const noexcept { return sampler_; }
    const Texture2D& environmentMap() const noexcept { return environmentMap_; }
    const Texture2D& irradianceMap() const noexcept { return irradianceMap_; }
    const Texture2D& specularMap() const noexcept { return specularMap_; }
    const PbrUploadReport& report() const noexcept { return report_; }

private:
    std::unordered_map<std::string, UploadedPbrMaterial> materials_;
    UploadedTexture ballAtlas_;
    Texture2D environmentMap_;
    Texture2D irradianceMap_;
    Texture2D specularMap_;
    Sampler sampler_;
    PbrUploadReport report_;
};

}  // namespace billiardgl::render
