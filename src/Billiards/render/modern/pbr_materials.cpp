#include "render/modern/pbr_materials.h"

#include "automation_json.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace billiardgl::render {
namespace {

json::Value readJson(const std::string& path, const char* label)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error(std::string(label) + " unavailable: " + path);
    std::ostringstream contents;
    contents << input.rdbuf();
    const auto parsed = json::parse(contents.str());
    if (!parsed.ok) throw std::runtime_error(std::string(label) + " parse failed: " + path + ": " + parsed.error);
    return parsed.value;
}

std::array<float, 2> float2(const json::Value& value, const std::string& field)
{
    const auto& array = value.asArray();
    if (array.size() != 2) throw std::runtime_error(field + " must contain two values");
    return {{static_cast<float>(array[0].asNumber()), static_cast<float>(array[1].asNumber())}};
}

std::array<float, 3> float3(const json::Value& value, const std::string& field)
{
    const auto& array=value.asArray();
    if(array.size()!=3)throw std::runtime_error(field+" must contain three values");
    return {{static_cast<float>(array[0].asNumber()),static_cast<float>(array[1].asNumber()),static_cast<float>(array[2].asNumber())}};
}

std::array<float, 3> axisDirection(const std::string& axis,
                                   const std::string& field)
{
    if (axis == "+X") return {{1.0f, 0.0f, 0.0f}};
    if (axis == "-X") return {{-1.0f, 0.0f, 0.0f}};
    if (axis == "+Y") return {{0.0f, 1.0f, 0.0f}};
    if (axis == "-Y") return {{0.0f, -1.0f, 0.0f}};
    if (axis == "+Z") return {{0.0f, 0.0f, 1.0f}};
    if (axis == "-Z") return {{0.0f, 0.0f, -1.0f}};
    throw std::runtime_error(field + " must be one of +X, -X, +Y, -Y, +Z or -Z");
}

float rec709Luminance(const std::array<float, 3>& value)
{
    return value[0] * 0.2126f + value[1] * 0.7152f + value[2] * 0.0722f;
}

void requireNormalizedTint(const std::array<float, 3>& tint,
                           const std::string& field)
{
    for (float component : tint)
        if (!std::isfinite(component) || component < 0.0f)
            throw std::runtime_error(field + " must contain finite non-negative values");
    if (std::abs(rec709Luminance(tint) - 1.0f) > 1.0e-5f)
        throw std::runtime_error(field + " must have Rec.709 luminance equal to one");
}

float nonNegativeFiniteNumber(const json::Value& value, const std::string& field)
{
    const float result = static_cast<float>(value.asNumber());
    if (!std::isfinite(result) || result < 0.0f)
        throw std::runtime_error(field + " must be finite and non-negative");
    return result;
}

std::string beside(const std::filesystem::path& manifest, const std::string& referenced)
{
    // Package manifests may contain source-root-relative paths. Runtime assets
    // are deliberately co-located, so resolving the basename keeps packages
    // relocatable without relying on process cwd.
    return (manifest.parent_path() / std::filesystem::path(referenced).filename()).string();
}

TextureReference textureRef(const std::filesystem::path& paramsPath,
                            const std::string& file, TextureSemantic semantic,
                            int width, int height)
{
    const bool srgb = semantic == TextureSemantic::BaseColor
        || semantic == TextureSemantic::BallAtlas;
    return {beside(paramsPath, file), semantic,
            srgb ? TextureEncoding::SrgbRgba8 : TextureEncoding::LinearRgba8,
            width, height};
}

}  // namespace

std::size_t rgba8FullMipBytes(int width, int height) noexcept
{
    if (width <= 0 || height <= 0) return 0;
    std::size_t bytes = 0;
    while (true) {
        bytes += static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        if (width == 1 && height == 1) break;
        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
    }
    return bytes;
}

std::array<float, 4> ballAtlasUvBounds(int identity) noexcept
{
    const int tile = identity < 0 ? 0 : (identity > 15 ? 15 : identity);
    const float u0 = static_cast<float>(tile % 8) / 8.0f;
    const float v0 = static_cast<float>(tile / 8) / 2.0f;
    return {{u0, v0, u0 + 1.0f / 8.0f, v0 + 1.0f / 2.0f}};
}

LightingRigDefinition loadLightingRigDefinition(const std::string& lightingRigPath)
{
    const auto rig = readJson(lightingRigPath, "lighting rig manifest");
    LightingRigDefinition result;
    result.id = rig.at("rig_id").asString();
    result.exposureEv100 = static_cast<float>(rig.at("exposure").at("ev100").asNumber());
    const auto& key = rig.at("fixtures").asArray().at(0);
    result.keyFixtureId = key.at("id").asString();
    result.keyFixtureType = key.at("type").asString();
    const auto size = float2(key.at("emitter_size_m"), "fixtures[0].emitter_size_m");
    result.keyWidthMetres = size[0];
    result.keyDepthMetres = size[1];
    result.keyFluxLumens = static_cast<float>(key.at("luminous_flux_lm").asNumber());
    result.keyTemperatureKelvin = static_cast<float>(key.at("color_temperature_k").asNumber());
    result.keyPositionMetres =
        float3(key.at("position_m"), "fixtures[0].position_m");
    result.keyEmissionNormal = axisDirection(
        key.at("orientation").at("local_emission_axis").asString(),
        "fixtures[0].orientation.local_emission_axis");

    const int schemaVersion = rig.has("schema_version")
        ? rig.at("schema_version").asInt() : 1;
    const auto& environment = rig.at("environment");
    if (schemaVersion == 1) {
        if (environment.has("contract_version")
            && environment.at("contract_version").asInt() != 1)
            throw std::runtime_error(
                "schema v1 environment contract_version must equal one");
        result.environmentContract =
            EnvironmentRadianceContract::LegacyAbsoluteLinearRgbV1;
        result.keyFixtureOwnership = KeyFixtureOwnership::LegacyEnvironmentAndDirect;
        result.environmentUpper = float3(
            environment.at("upper_linear_rgb"), "environment.upper_linear_rgb");
        result.environmentLower = float3(
            environment.at("lower_linear_rgb"), "environment.lower_linear_rgb");
        // `environment.intensity` was authored in v1 but never had runtime
        // semantics.  It deliberately remains a no-op for pixel compatibility.
    } else if (schemaVersion == 2) {
        if (!environment.has("contract_version")
            || environment.at("contract_version").asInt() != 2
            || !environment.has("type")
            || environment.at("type").asString()
                != "analytic_normalized_tint_luminance_no_external_hdr"
            || !environment.has("radiance_encoding")
            || environment.at("radiance_encoding").asString()
                != "normalized_tint_luminance_v1"
            || !environment.has("tint_normalization")
            || environment.at("tint_normalization").asString()
                != "rec709_y_equals_one"
            || !environment.has("key_ownership")
            || environment.at("key_ownership").asString()
                != "direct_rect_area_only")
            throw std::runtime_error(
                "schema v2 environment must declare normalized tint/luminance and direct-only key ownership");
        if (environment.has("upper_linear_rgb")
            || environment.has("lower_linear_rgb")
            || environment.has("intensity"))
            throw std::runtime_error(
                "schema v2 environment cannot mix legacy RGB/intensity fields");
        result.environmentContract =
            EnvironmentRadianceContract::NormalizedTintLuminanceV2;
        result.keyFixtureOwnership = KeyFixtureOwnership::DirectRectAreaOnly;
        result.environmentUpperTint = float3(
            environment.at("upper_tint_linear_rgb"),
            "environment.upper_tint_linear_rgb");
        result.environmentLowerTint = float3(
            environment.at("lower_tint_linear_rgb"),
            "environment.lower_tint_linear_rgb");
        requireNormalizedTint(result.environmentUpperTint,
                              "environment.upper_tint_linear_rgb");
        requireNormalizedTint(result.environmentLowerTint,
                              "environment.lower_tint_linear_rgb");
        result.environmentUpperLuminance = nonNegativeFiniteNumber(
            environment.at("upper_luminance_scene_linear"),
            "environment.upper_luminance_scene_linear");
        result.environmentLowerLuminance = nonNegativeFiniteNumber(
            environment.at("lower_luminance_scene_linear"),
            "environment.lower_luminance_scene_linear");
        result.keyReferenceFluxLumens = nonNegativeFiniteNumber(
            environment.at("direct_key_reference_flux_lm"),
            "environment.direct_key_reference_flux_lm");
        if (result.keyReferenceFluxLumens <= 0.0f)
            throw std::runtime_error(
                "environment.direct_key_reference_flux_lm must be positive");
    } else {
        throw std::runtime_error("unsupported lighting rig schema_version");
    }

    const auto& tiers = rig.at("shadow_quality_tiers");
    result.shadowMapSizes = {{tiers.at("low").at("map_resolution").asInt(),
                              tiers.at("medium").at("map_resolution").asInt(),
                              tiers.at("high").at("map_resolution").asInt()}};
    result.shadowFilterTaps = {{tiers.at("low").at("filter_taps").asInt(),
                                tiers.at("medium").at("filter_taps").asInt(),
                                tiers.at("high").at("filter_taps").asInt()}};
    result.shadowSoftnessMetres = {{
        static_cast<float>(tiers.at("low").at("softness_world_radius_m").asNumber()),
        static_cast<float>(tiers.at("medium").at("softness_world_radius_m").asNumber()),
        static_cast<float>(tiers.at("high").at("softness_world_radius_m").asNumber())}};
    return result;
}

float directKeyIntensityScale(const LightingRigDefinition& lighting) noexcept
{
    if (lighting.environmentContract
        == EnvironmentRadianceContract::LegacyAbsoluteLinearRgbV1)
        return 1.0f;
    if (!std::isfinite(lighting.keyFluxLumens)
        || !std::isfinite(lighting.keyReferenceFluxLumens)
        || lighting.keyFluxLumens <= 0.0f
        || lighting.keyReferenceFluxLumens <= 0.0f)
        return 0.0f;
    return lighting.keyFluxLumens / lighting.keyReferenceFluxLumens;
}

NormalizedLightingScales normalizedLightingScales(
    const LightingRigDefinition& lighting) noexcept
{
    NormalizedLightingScales result;
    result.directKey = directKeyIntensityScale(lighting);
    return result;
}

PbrPackageDescription loadPbrPackageDescription(
    const std::string& parametersPath,
    const std::string& assetManifestPath,
    const std::string& lightingRigPath)
{
    const auto params = readJson(parametersPath, "PBR parameter manifest");
    const auto assets = readJson(assetManifestPath, "PBR asset manifest");
    PbrPackageDescription result;
    result.id = params.at("package_id").asString();

    const auto& management = params.at("color_management");
    result.baseColorSrgbNormalOrmLinear =
        management.at("basecolor").asString() == "srgb_iec61966_2_1"
        && management.at("normal").asString() == "linear_opengl_positive_y_rgba8"
        && management.at("orm").asString() == "linear_rgba8_r_ao_g_roughness_b_metallic";
    result.openGlPositiveYNormals = management.at("normal").asString().find("opengl_positive_y") != std::string::npos;
    result.ormChannelsAoRoughnessMetallic = management.at("orm").asString().find("r_ao_g_roughness_b_metallic") != std::string::npos;

    const std::filesystem::path paramsFile(parametersPath);
    for (const auto& entry : params.at("texture_sets").asObject()) {
        const auto& item = entry.second;
        const auto& dimensions = item.at("resolution_px").asArray();
        if (dimensions.size() != 2) throw std::runtime_error(entry.first + ".resolution_px must contain two values");
        const int width = dimensions[0].asInt();
        const int height = dimensions[1].asInt();
        const auto& files = item.at("files");
        PbrMaterialDefinition material;
        material.name = entry.first;
        material.baseColor = textureRef(paramsFile, files.at("basecolor").asString(), TextureSemantic::BaseColor, width, height);
        material.normal = textureRef(paramsFile, files.at("normal").asString(), TextureSemantic::NormalOpenGlPositiveY, width, height);
        material.orm = textureRef(paramsFile, files.at("orm").asString(), TextureSemantic::Orm, width, height);
        material.uvRepeat = float2(item.at("uv_repeat"), entry.first + ".uv_repeat");
        material.roughnessFallback = static_cast<float>(item.at("roughness_center").asNumber());
        material.metallicFallback = static_cast<float>(item.at("metallic").asNumber());
        material.clearcoatFallback = (entry.first == "walnut" || entry.first == "cue") ? 0.18f : 0.0f;
        result.materials.push_back(std::move(material));
    }

    const auto& budget = assets.at("budgets");
    result.expectedGpuBytes = static_cast<std::size_t>(budget.at("runtime_texture_gpu_bytes_rgba8_with_full_mips").asNumber());
    result.expectedTextureCount = budget.at("textures").asInt();
    const auto& constraints = assets.at("constraints");
    result.metallicRoughnessFallbackPresent = constraints.at("metallic_roughness_fallback_present").asBool();
    result.openGlPositiveYNormals = result.openGlPositiveYNormals
        && constraints.at("normal_convention").asString() == "opengl_positive_y";
    const auto& atlas = assets.at("files").at("ball_atlas_png");
    result.ballAtlas = textureRef(std::filesystem::path(assetManifestPath), atlas.at("path").asString(),
                                  TextureSemantic::BallAtlas,
                                  atlas.at("width_px").asInt(), atlas.at("height_px").asInt());

    result.lighting = loadLightingRigDefinition(lightingRigPath);
    return result;
}

PbrPackageValidation validatePbrPackage(const PbrPackageDescription& package)
{
    PbrPackageValidation result;
    if (!package.baseColorSrgbNormalOrmLinear) result.errors.push_back("baseColor must be sRGB and normal/ORM must be linear");
    if (!package.openGlPositiveYNormals) result.errors.push_back("normal maps must use OpenGL +Y convention");
    if (!package.ormChannelsAoRoughnessMetallic) result.errors.push_back("ORM must be R=AO G=roughness B=metallic");
    if (!package.metallicRoughnessFallbackPresent) result.errors.push_back("metallic-roughness fallback is required");
    if (package.lighting.environmentContract
            == EnvironmentRadianceContract::NormalizedTintLuminanceV2) {
        if (package.lighting.keyFixtureOwnership
            != KeyFixtureOwnership::DirectRectAreaOnly)
            result.errors.push_back("v2 environment key fixture must be direct-area-only");
        const auto validateTint = [&](const std::array<float, 3>& tint,
                                      const char* label) {
            bool finiteAndNonNegative = true;
            for (float component : tint)
                finiteAndNonNegative = finiteAndNonNegative
                    && std::isfinite(component) && component >= 0.0f;
            if (!finiteAndNonNegative
                || std::abs(rec709Luminance(tint) - 1.0f) > 1.0e-5f)
                result.errors.push_back(std::string(label)
                    + " must be finite, non-negative and Rec.709-normalized");
        };
        validateTint(package.lighting.environmentUpperTint,
                     "v2 upper environment tint");
        validateTint(package.lighting.environmentLowerTint,
                     "v2 lower environment tint");
        if (!std::isfinite(package.lighting.environmentUpperLuminance)
            || package.lighting.environmentUpperLuminance < 0.0f
            || !std::isfinite(package.lighting.environmentLowerLuminance)
            || package.lighting.environmentLowerLuminance < 0.0f)
            result.errors.push_back("v2 environment luminance must be finite and non-negative");
        if (!std::isfinite(package.lighting.keyFluxLumens)
            || package.lighting.keyFluxLumens < 0.0f
            || !std::isfinite(package.lighting.keyReferenceFluxLumens)
            || package.lighting.keyReferenceFluxLumens <= 0.0f)
            result.errors.push_back("v2 direct key flux and reference must be finite and non-negative/positive");
    }
    if (package.lighting.keyFixtureId.empty())
        result.errors.push_back("rectangular key fixture id is required");
    if (package.lighting.keyFixtureType != "rect_area_or_renderer_equivalent")
        result.errors.push_back("key fixture must declare rectangular area-light geometry");
    if (!std::isfinite(package.lighting.keyWidthMetres)
        || !std::isfinite(package.lighting.keyDepthMetres)
        || package.lighting.keyWidthMetres <= 0.0f
        || package.lighting.keyDepthMetres <= 0.0f)
        result.errors.push_back("rectangular key fixture dimensions must be finite and positive");
    bool keyPositionFinite = true;
    bool keyNormalFinite = true;
    float keyNormalLengthSquared = 0.0f;
    for (std::size_t component = 0; component < 3; ++component) {
        keyPositionFinite = keyPositionFinite
            && std::isfinite(package.lighting.keyPositionMetres[component]);
        keyNormalFinite = keyNormalFinite
            && std::isfinite(package.lighting.keyEmissionNormal[component]);
        keyNormalLengthSquared += package.lighting.keyEmissionNormal[component]
            * package.lighting.keyEmissionNormal[component];
    }
    if (!keyPositionFinite)
        result.errors.push_back("rectangular key fixture position must be finite");
    if (!keyNormalFinite || std::abs(keyNormalLengthSquared - 1.0f) > 1.0e-6f)
        result.errors.push_back("rectangular key fixture emission normal must be finite and unit length");
    for (std::size_t tier = 0; tier < package.lighting.shadowMapSizes.size(); ++tier) {
        if (package.lighting.shadowMapSizes[tier] <= 0
            || package.lighting.shadowFilterTaps[tier] <= 0
            || package.lighting.shadowFilterTaps[tier] > 24
            || !std::isfinite(package.lighting.shadowSoftnessMetres[tier])
            || package.lighting.shadowSoftnessMetres[tier] <= 0.0f)
            result.errors.push_back("shadow quality tier must have positive map, taps and softness");
    }
    if (package.materials.size() * 3u + 1u != static_cast<std::size_t>(package.expectedTextureCount)) result.errors.push_back("texture count differs from manifest budget");
    std::size_t estimated = rgba8FullMipBytes(package.ballAtlas.expectedWidth, package.ballAtlas.expectedHeight);
    const auto inspect = [&](const TextureReference& reference) {
        if (!std::filesystem::is_regular_file(reference.path)) result.missingFiles.push_back(reference.path);
    };
    inspect(package.ballAtlas);
    for (const auto& material : package.materials) {
        if (material.uvRepeat[0] <= 0.0f || material.uvRepeat[1] <= 0.0f) result.errors.push_back(material.name + " has invalid UV tiling");
        inspect(material.baseColor); inspect(material.normal); inspect(material.orm);
        estimated += rgba8FullMipBytes(material.baseColor.expectedWidth, material.baseColor.expectedHeight) * 3u;
    }
    if (estimated != package.expectedGpuBytes) result.errors.push_back("full-mip RGBA8 estimate differs from manifest budget");
    result.contractValid = result.errors.empty();
    return result;
}

}  // namespace billiardgl::render
