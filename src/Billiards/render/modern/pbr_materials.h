#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace billiardgl::render {

enum class TextureEncoding { SrgbRgba8, LinearRgba8 };
enum class TextureSemantic { BaseColor, NormalOpenGlPositiveY, Orm, BallAtlas };

enum class EnvironmentRadianceContract {
    LegacyAbsoluteLinearRgbV1,
    NormalizedTintLuminanceV2,
};

enum class KeyFixtureOwnership {
    LegacyEnvironmentAndDirect,
    DirectRectAreaOnly,
};

struct TextureReference {
    std::string path;
    TextureSemantic semantic = TextureSemantic::BaseColor;
    TextureEncoding encoding = TextureEncoding::SrgbRgba8;
    int expectedWidth = 0;
    int expectedHeight = 0;
};

struct PbrMaterialDefinition {
    std::string name;
    TextureReference baseColor;
    TextureReference normal;
    TextureReference orm;
    std::array<float, 2> uvRepeat{{1.0f, 1.0f}};
    float roughnessFallback = 0.5f;
    float metallicFallback = 0.0f;
    float clearcoatFallback = 0.0f;
};

struct LightingRigDefinition {
    std::string id;
    std::string keyFixtureId;
    std::string keyFixtureType;
    float exposureEv100 = 8.1f;
    float keyWidthMetres = 1.85f;
    float keyDepthMetres = 0.65f;
    float keyFluxLumens = 6200.0f;
    float keyReferenceFluxLumens = 6200.0f;
    float keyTemperatureKelvin = 3900.0f;
    std::array<float, 3> keyPositionMetres{{0.0f, 0.0f, 0.0f}};
    std::array<float, 3> keyEmissionNormal{{0.0f, -1.0f, 0.0f}};
    std::array<float, 3> environmentUpper{{0.035f, 0.043f, 0.050f}};
    std::array<float, 3> environmentLower{{0.018f, 0.015f, 0.013f}};
    EnvironmentRadianceContract environmentContract =
        EnvironmentRadianceContract::LegacyAbsoluteLinearRgbV1;
    KeyFixtureOwnership keyFixtureOwnership =
        KeyFixtureOwnership::LegacyEnvironmentAndDirect;
    std::array<float, 3> environmentUpperTint{{1.0f, 1.0f, 1.0f}};
    std::array<float, 3> environmentLowerTint{{1.0f, 1.0f, 1.0f}};
    float environmentUpperLuminance = 0.0f;
    float environmentLowerLuminance = 0.0f;
    std::array<int, 3> shadowMapSizes{{1024, 2048, 4096}};
    std::array<int, 3> shadowFilterTaps{{4, 12, 24}};
    std::array<float, 3> shadowSoftnessMetres{{0.035f, 0.08f, 0.14f}};
};

struct NormalizedLightingScales {
    float ambientFloor = 0.0f;
    float irradiance = 1.0f;
    float reflected = 1.0f;
    float directKey = 1.0f;
};

struct PbrPackageDescription {
    std::string id;
    std::vector<PbrMaterialDefinition> materials;
    TextureReference ballAtlas;
    LightingRigDefinition lighting;
    std::size_t expectedGpuBytes = 0;
    int expectedTextureCount = 0;
    bool baseColorSrgbNormalOrmLinear = false;
    bool openGlPositiveYNormals = false;
    bool ormChannelsAoRoughnessMetallic = false;
    bool metallicRoughnessFallbackPresent = false;
};

struct PbrPackageValidation {
    bool contractValid = false;
    std::vector<std::string> missingFiles;
    std::vector<std::string> errors;
};

std::size_t rgba8FullMipBytes(int width, int height) noexcept;
std::array<float, 4> ballAtlasUvBounds(int identity) noexcept;
PbrPackageDescription loadPbrPackageDescription(
    const std::string& parametersPath,
    const std::string& assetManifestPath,
    const std::string& lightingRigPath);
LightingRigDefinition loadLightingRigDefinition(const std::string& lightingRigPath);
float directKeyIntensityScale(const LightingRigDefinition& lighting) noexcept;
NormalizedLightingScales normalizedLightingScales(
    const LightingRigDefinition& lighting) noexcept;
PbrPackageValidation validatePbrPackage(const PbrPackageDescription& package);

}  // namespace billiardgl::render
