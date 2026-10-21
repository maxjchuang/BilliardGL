#include "render/modern/gpu_resources.h"
#include "render/modern/formal_asset_attestation.h"
#include "render/modern/lookdev_settings.h"
#include "render/modern/modern_renderer.h"
#include "render/modern/render_data.h"
#include "render/modern/renderer_backend.h"
#include "render/modern/shader_diagnostics.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

struct FakeGpuTag {
    static billiardgl::render::GpuHandle destroyedHandle;
    static void destroy(billiardgl::render::GpuHandle handle) noexcept
    {
        destroyedHandle = handle;
    }
};

billiardgl::render::GpuHandle FakeGpuTag::destroyedHandle = 0;

void expect(bool condition, const char* expression)
{
    if (!condition) {
        std::cerr << "Expectation failed: " << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void expectContains(const std::string& text, const std::string& expected)
{
    expect(text.find(expected) != std::string::npos, expected.c_str());
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    expect(static_cast<bool>(input), "test fixture source opens");
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

void writeText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    expect(static_cast<bool>(output), "test fixture destination opens");
    output << text;
    expect(static_cast<bool>(output), "test fixture write completes");
}

std::string replacedOnce(std::string text, const std::string& from,
                         const std::string& to)
{
    const auto offset = text.find(from);
    expect(offset != std::string::npos, "fixture replacement source exists");
    text.replace(offset, from.size(), to);
    expect(text.find(from, offset + to.size()) == std::string::npos,
           "fixture replacement source is unique");
    return text;
}

void expectRigLoadFailure(const std::filesystem::path& path,
                          const std::string& text,
                          const std::string& expected)
{
    writeText(path, text);
    try {
        (void)billiardgl::render::loadLightingRigDefinition(path.string());
        expect(false, "invalid lighting rig must fail closed");
    } catch (const std::runtime_error& error) {
        expectContains(error.what(), expected);
    }
}

template <typename Resource>
constexpr bool isMoveOnlyResource()
{
    return !std::is_copy_constructible<Resource>::value
        && !std::is_copy_assignable<Resource>::value
        && std::is_nothrow_move_constructible<Resource>::value
        && std::is_nothrow_move_assignable<Resource>::value;
}

}  // namespace

int main()
{
    using namespace billiardgl::render;

    static_assert(isMoveOnlyResource<Shader>(), "Shader ownership must be unique");
    static_assert(isMoveOnlyResource<Program>(), "Program ownership must be unique");
    static_assert(isMoveOnlyResource<Buffer>(), "Buffer ownership must be unique");
    static_assert(isMoveOnlyResource<VertexArray>(), "VAO ownership must be unique");
    static_assert(isMoveOnlyResource<Texture2D>(), "Texture ownership must be unique");
    static_assert(isMoveOnlyResource<Sampler>(), "Sampler ownership must be unique");
    static_assert(isMoveOnlyResource<Framebuffer>(), "Framebuffer ownership must be unique");

    {
        UniqueGpuResource<FakeGpuTag> original(71);
        UniqueGpuResource<FakeGpuTag> moved(std::move(original));
        expect(!original, "move clears source ownership");
        expect(moved.id() == 71, "move transfers handle ownership");
        moved.reset(83);
        expect(FakeGpuTag::destroyedHandle == 71, "reset destroys replaced handle");
        expect(moved.release() == 83, "release returns ownership without destroying it");
    }
    expect(FakeGpuTag::destroyedHandle == 71, "released handle is not destroyed");

    const std::string source = "#version 410 core\nvoid main() {\n  broken;\n}\n";
    const std::string compilation = formatShaderCompilationError(
        ShaderStage::Vertex, "table.vert", "ERROR: 0:3: unknown identifier", source);
    expectContains(compilation, "stage: vertex");
    expectContains(compilation, "resource: table.vert");
    expectContains(compilation, "ERROR: 0:3: unknown identifier");
    expectContains(compilation, "1 | #version 410 core");
    expectContains(compilation, "3 |   broken;");

    const std::string linking = formatProgramLinkError("forward-pbr", "varying mismatch");
    expectContains(linking, "Shader program link failed");
    expectContains(linking, "resource: forward-pbr");
    expectContains(linking, "varying mismatch");

    const std::string missingPath = "/path/that/does/not/exist/billiardgl.vert";
    try {
        (void)readShaderSourceFile(missingPath);
        expect(false, "readShaderSourceFile should throw");
    } catch (const std::runtime_error& error) {
        expectContains(error.what(), "Shader source file error");
        expectContains(error.what(), missingPath);
        expectContains(error.what(), "could not open for reading");
    }

    expect(static_cast<unsigned int>(MeshAttribute::Position) == 0,
           "Position attribute location");
    expect(static_cast<unsigned int>(MeshAttribute::Color0) == 4,
           "Color attribute location");

    const LookdevLoadResult fallback = loadLookdevSettings(
        "/path/that/does/not/exist/lookdev.json");
    expect(!fallback.loadedManifest, "missing lookdev uses fallback");
    expect(!fallback.warnings.empty(), "lookdev fallback is never silent");
    const LookdevLoadResult approved = loadLookdevSettings(
        std::string(BILLIARDGL_SOURCE_ROOT)
        + "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json");
    expect(approved.loadedManifest, "approved lookdev manifest loads");
    expect(approved.settings.manifestId == "focused_practice_hall_v1",
           "manifest identity crosses loader boundary");
    expect(approved.settings.ballClearcoat == 0.9f,
           "ball clearcoat target crosses loader boundary");
    expect(approved.settings.lampWidthMetres == 1.85f,
           "lamp dimensions cross loader boundary");
    const auto low = qualitySettings(RenderQuality::Low);
    const auto medium = qualitySettings(RenderQuality::Medium);
    const auto high = qualitySettings(RenderQuality::High);
    for (auto quality : {RenderQuality::Low, RenderQuality::Medium, RenderQuality::High}) {
        expect(effectiveDepthOfFieldRadius(quality, false, true) == 0.0f &&
                   effectiveDepthOfFieldRadius(quality, true, false) == 0.0f,
               "DOF is opt-in and restricted to replay");
        expect(effectiveDepthOfFieldRadius(quality, true, true) ==
                   (quality == RenderQuality::High ? 2.0f : 0.0f),
               "only High permits replay depth of field");
    }
    expect(effectiveBloomStrength(RenderQuality::Low, true) == 0.0f &&
               effectiveBloomStrength(RenderQuality::Medium, true) == 0.06f &&
               effectiveBloomStrength(RenderQuality::High, true) == 0.06f,
           "bloom follows the authored quality-tier contract");
    for (auto quality : {RenderQuality::Low, RenderQuality::Medium, RenderQuality::High})
        expect(effectiveBloomStrength(quality, false) == 0.0f,
               "disabling bloom overrides every quality tier");
    expect(low.shadowMapSize == 1024 && low.pcfKernelWidth == 1,
           "low shadow quality contract");
    expect(medium.shadowMapSize == 2048 && medium.pcfKernelWidth == 2,
           "medium shadow quality contract");
    expect(high.shadowMapSize == 4096 && high.pcfKernelWidth == 3,
           "high shadow quality contract");
    expect(low.rectangularLightSamples < medium.rectangularLightSamples
           && medium.rectangularLightSamples == high.rectangularLightSamples,
           "low keeps its explicit budget while medium/high use the converged rectangle integral");
    expect(effectiveBallAreaLightSamples(RenderQuality::Low) == 1
               && effectiveBallAreaLightSamples(RenderQuality::Medium) == 16
               && effectiveBallAreaLightSamples(RenderQuality::High) == 16,
           "ball area light defaults to the selected quality tier");
    expect(effectiveBallAreaLightSamples(RenderQuality::Low, 0) == 0
               && effectiveBallAreaLightSamples(RenderQuality::Medium, 0) == 0
               && effectiveBallAreaLightSamples(RenderQuality::High, 0) == 0,
           "explicit zero preserves the point-light diagnostic rollback");
    expect(effectiveBallAreaLightSamples(RenderQuality::Low, 4) == 4
               && effectiveBallAreaLightSamples(RenderQuality::High, 1) == 1,
           "explicit diagnostic sample counts override quality selection");
    expect(effectiveBallShadowReceiverBiasScale() == 4.0f
               && effectiveBallShadowReceiverBiasScale(1.0f) == 1.0f,
           "ball-only receiver bias defaults to the converged scale with an exact rollback");

    const std::string phase2 = std::string(BILLIARDGL_SOURCE_ROOT) + "/assets/modern/phase2/";
    const auto pbr = loadPbrPackageDescription(
        phase2 + "focused_practice_hall_pbr_v1.params.json",
        phase2 + "focused_practice_hall_modular_v1.asset.json",
        phase2 + "focused_practice_hall_lighting_rig_v1.json");
    const auto validation = validatePbrPackage(pbr);
    expect(validation.contractValid, "PBR color/normal/ORM/fallback contract");
    expect(validation.missingFiles.empty(), "all PBR package files exist");
    expect(pbr.materials.size() == 7 && pbr.expectedTextureCount == 22,
           "seven PBR sets plus atlas");
    expect(pbr.expectedGpuBytes == 27962016u,
           "PBR full-mip GPU budget");
    expect(pbr.lighting.shadowMapSizes[0] == 1024
           && pbr.lighting.shadowMapSizes[1] == 2048
           && pbr.lighting.shadowMapSizes[2] == 4096,
           "lighting rig shadow tiers cross loader boundary");
    expect(pbr.lighting.shadowFilterTaps
               == std::array<int, 3>{{4, 12, 24}}
           && pbr.lighting.shadowSoftnessMetres
               == std::array<float, 3>{{0.035f, 0.08f, 0.14f}},
           "authored shadow filter taps and world softness cross loader boundary");
    expect(pbr.lighting.keyFixtureId == "key_rect_table_lamp"
           && pbr.lighting.keyFixtureType == "rect_area_or_renderer_equivalent",
           "authored rectangular key identity crosses loader boundary");
    expect(pbr.lighting.keyWidthMetres == 0.61f
           && pbr.lighting.keyDepthMetres == 1.81f
           && pbr.lighting.keyPositionMetres
               == std::array<float, 3>{{0.0f, 2.21483976f, 0.0f}}
           && pbr.lighting.keyEmissionNormal
               == std::array<float, 3>{{0.0f, -1.0f, 0.0f}},
           "authored rectangular key transform crosses loader boundary");
    expect(pbr.lighting.environmentContract
               == EnvironmentRadianceContract::LegacyAbsoluteLinearRgbV1
           && pbr.lighting.keyFixtureOwnership
               == KeyFixtureOwnership::LegacyEnvironmentAndDirect,
           "schema v1 keeps absolute RGB and legacy key ownership");

    const auto lightingFixtureDirectory =
        std::filesystem::temp_directory_path()
        / "billiardgl-lighting-contract-v2-tests";
    std::filesystem::create_directories(lightingFixtureDirectory);
    const auto lightingFixture = lightingFixtureDirectory / "rig.json";
    const auto sourceRigPath = std::filesystem::path(phase2)
        / "focused_practice_hall_lighting_rig_v1.json";
    const std::string sourceRig = readText(sourceRigPath);
    writeText(lightingFixture, replacedOnce(
        sourceRig, "\"intensity\": 0.18", "\"intensity\": 987.0"));
    const auto v1DifferentUnusedIntensity =
        loadLightingRigDefinition(lightingFixture.string());
    expect(v1DifferentUnusedIntensity.environmentContract
               == pbr.lighting.environmentContract
           && v1DifferentUnusedIntensity.keyFixtureOwnership
               == pbr.lighting.keyFixtureOwnership
           && v1DifferentUnusedIntensity.environmentUpper
               == pbr.lighting.environmentUpper
           && v1DifferentUnusedIntensity.environmentLower
               == pbr.lighting.environmentLower,
           "schema v1 intensity remains a legacy no-op");
    writeText(lightingFixture, replacedOnce(
        sourceRig, "  \"schema_version\": 1,\n", ""));
    const auto v1MissingSchema =
        loadLightingRigDefinition(lightingFixture.string());
    expect(v1MissingSchema.environmentContract
               == EnvironmentRadianceContract::LegacyAbsoluteLinearRgbV1
           && v1MissingSchema.environmentUpper == pbr.lighting.environmentUpper
           && v1MissingSchema.environmentLower == pbr.lighting.environmentLower,
           "missing schema_version remains compatible with legacy v1");
    writeText(lightingFixture, replacedOnce(
        sourceRig, "\"background_relative_ev\": -2.5,",
        "\"background_relative_ev\": -2.5,\n    \"contract_version\": 1,"));
    const auto v1ExplicitContract =
        loadLightingRigDefinition(lightingFixture.string());
    expect(v1ExplicitContract.environmentContract
               == EnvironmentRadianceContract::LegacyAbsoluteLinearRgbV1,
           "explicit integer contract_version one remains legacy v1");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(sourceRig, "\"background_relative_ev\": -2.5,",
            "\"background_relative_ev\": -2.5,\n    \"contract_version\": 1.5,"),
        "not int");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(sourceRig, "\"background_relative_ev\": -2.5,",
            "\"background_relative_ev\": -2.5,\n    \"contract_version\": \"1\","),
        "not int");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(sourceRig, "\"background_relative_ev\": -2.5,",
            "\"background_relative_ev\": -2.5,\n    \"contract_version\": true,"),
        "not int");

    const std::string v2Rig = R"JSON({
  "rig_id":"normalized_candidate_v2",
  "schema_version":2,
  "exposure":{"ev100":7.2},
  "fixtures":[{"id":"key_rect_table_lamp","type":"rect_area_or_renderer_equivalent",
    "emitter_size_m":[0.61,1.81],"luminous_flux_lm":6200,
    "color_temperature_k":3900,"position_m":[0,2.21483976,0],
    "orientation":{"local_emission_axis":"-Y"}}],
  "environment":{"contract_version":2,
    "type":"analytic_normalized_tint_luminance_no_external_hdr",
    "radiance_encoding":"normalized_tint_luminance_v1",
    "tint_normalization":"rec709_y_equals_one",
    "key_ownership":"direct_rect_area_only",
    "direct_key_reference_flux_lm":6200,
    "upper_tint_linear_rgb":[1,1,1],
    "lower_tint_linear_rgb":[1,1,1],
    "upper_luminance_scene_linear":0.18,
    "lower_luminance_scene_linear":0.06},
  "shadow_quality_tiers":{
    "low":{"map_resolution":1024,"filter_taps":4,"softness_world_radius_m":0.035},
    "medium":{"map_resolution":2048,"filter_taps":12,"softness_world_radius_m":0.08},
    "high":{"map_resolution":4096,"filter_taps":24,"softness_world_radius_m":0.14}}
})JSON";
    writeText(lightingFixture, v2Rig);
    const auto v2 = loadLightingRigDefinition(lightingFixture.string());
    expect(v2.environmentContract
               == EnvironmentRadianceContract::NormalizedTintLuminanceV2
           && v2.keyFixtureOwnership
               == KeyFixtureOwnership::DirectRectAreaOnly
           && v2.environmentUpperTint
               == std::array<float, 3>{{1.0f, 1.0f, 1.0f}}
           && v2.environmentLowerTint
               == std::array<float, 3>{{1.0f, 1.0f, 1.0f}}
           && v2.environmentUpperLuminance == 0.18f
           && v2.environmentLowerLuminance == 0.06f
           && v2.keyReferenceFluxLumens == 6200.0f
           && directKeyIntensityScale(v2) == 1.0f,
           "schema v2 normalized tint/luminance crosses loader boundary");
    const auto v2Scales = normalizedLightingScales(v2);
    expect(v2Scales.ambientFloor == 0.0f
           && v2Scales.irradiance == 1.0f
           && v2Scales.reflected == 1.0f
           && v2Scales.directKey == 1.0f,
           "schema v2 removes ambient floors and uses unit IBL energy");
    auto zeroKey = v2;
    zeroKey.keyFluxLumens = 0.0f;
    auto doubleKey = v2;
    doubleKey.keyFluxLumens *= 2.0f;
    expect(normalizedLightingScales(zeroKey).directKey == 0.0f
           && normalizedLightingScales(doubleKey).directKey == 2.0f
           && normalizedLightingScales(zeroKey).irradiance == 1.0f
           && normalizedLightingScales(doubleKey).reflected == 1.0f,
           "schema v2 direct key has exact zero and two-times scaling");
    const std::string rendererSource = readText(
        std::filesystem::path(BILLIARDGL_SOURCE_ROOT)
        / "src/Billiards/render/modern/modern_renderer.cpp");
    expectContains(rendererSource,
        "vec3(uV2AmbientFloor+ndl*uDirectKeyIntensityScale)+irradiance*uV2IrradianceScale");
    expectContains(rendererSource,
        "spec*uDirectKeyIntensityScale+reflected*uV2ReflectedScale");
    expectContains(rendererSource,
        "diffuseLight=uPbrMode==2?vec3(0.12+0.85*ndl)+irradiance*0.55:vec3(0.55+1.35*ndl)+irradiance*2.0");
    expectContains(rendererSource,
        "reflection=f0*(spec*1.7+reflected*2.2)+vec3(spec*uClearcoat*0.25)");
    auto v2Package = pbr;
    v2Package.lighting = v2;
    expect(validatePbrPackage(v2Package).contractValid,
           "schema v2 PBR package validates");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig,
            "\"upper_luminance_scene_linear\":0.18",
            "\"intensity\":0.18,\"upper_luminance_scene_linear\":0.18"),
        "cannot mix legacy RGB/intensity fields");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig,
            "\"upper_tint_linear_rgb\":[1,1,1]",
            "\"upper_tint_linear_rgb\":[2,1,1]"),
        "Rec.709 luminance equal to one");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig,
            "\"key_ownership\":\"direct_rect_area_only\"",
            "\"key_ownership\":\"environment_and_direct\""),
        "direct-only key ownership");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig,
            "\"type\":\"analytic_normalized_tint_luminance_no_external_hdr\"",
            "\"type\":\"analytic_constant_plus_ground_bounce_no_external_hdr\""),
        "direct-only key ownership");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig, "\"schema_version\":2", "\"schema_version\":3"),
        "unsupported lighting rig schema_version");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig, "\"schema_version\":2", "\"schema_version\":2.5"),
        "not int");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig, "\"schema_version\":2", "\"schema_version\":\"2\""),
        "not int");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig, "\"schema_version\":2", "\"schema_version\":true"),
        "not int");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig, "\"contract_version\":2", "\"contract_version\":2.5"),
        "not int");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig, "\"contract_version\":2", "\"contract_version\":\"2\""),
        "not int");
    expectRigLoadFailure(lightingFixture,
        replacedOnce(v2Rig, "\"contract_version\":2", "\"contract_version\":false"),
        "not int");
    std::filesystem::remove(lightingFixture);
    std::filesystem::remove(lightingFixtureDirectory);
    auto badRig = pbr;
    badRig.lighting.keyWidthMetres = 0.0f;
    expect(!validatePbrPackage(badRig).contractValid,
           "zero-width rectangular key geometry fails closed");
    badRig = pbr;
    badRig.lighting.keyPositionMetres[1] =
        std::numeric_limits<float>::quiet_NaN();
    expect(!validatePbrPackage(badRig).contractValid,
           "non-finite rectangular key position fails closed");
    badRig = pbr;
    badRig.lighting.shadowFilterTaps[1] = 25;
    expect(!validatePbrPackage(badRig).contractValid,
           "unsupported authored shadow tap count fails closed");
    badRig = pbr;
    badRig.lighting.shadowSoftnessMetres[1] = 0.0f;
    expect(!validatePbrPackage(badRig).contractValid,
           "non-positive authored shadow softness fails closed");
    badRig = pbr;
    badRig.lighting.keyEmissionNormal = {{0.0f, -0.5f, 0.0f}};
    expect(!validatePbrPackage(badRig).contractValid,
           "non-unit rectangular key emission axis fails closed");
    badRig = pbr;
    badRig.lighting.keyFixtureType = "point";
    expect(!validatePbrPackage(badRig).contractValid,
           "non-rectangular key fixture fails closed");
    expect(pbr.ballAtlas.encoding == TextureEncoding::SrgbRgba8,
           "ball atlas uploads as sRGB");
    expect(pbr.materials.front().normal.encoding == TextureEncoding::LinearRgba8
           && pbr.materials.front().orm.encoding == TextureEncoding::LinearRgba8,
           "normal and ORM upload as linear");
    expect(pbr.openGlPositiveYNormals && pbr.ormChannelsAoRoughnessMetallic,
           "OpenGL +Y and ORM channel declarations");
    expect(rgba8FullMipBytes(1024, 512) == 2796204u,
           "atlas mip memory calculation");
    for (const auto& material : pbr.materials) {
        expect(material.uvRepeat[0] > 0.0f && material.uvRepeat[1] > 0.0f,
               "positive UV tiling");
    }
    const auto formal = preflightFormalAssetPackage(
        std::string(BILLIARDGL_SOURCE_ROOT)
        + "/assets/modern/phase2-v10i-formal");
    expect(formal.ok, "self-contained v10i formal package preflight");
    expect(!formal.silentMaterialFallbackAllowed,
           "formal package forbids silent white/plain material fallback");
    expect(formal.requiredAssets.size() == 6,
           "formal package attests exact six contract assets");
    expect(formal.runtimeDependencies.size() == 19,
           "formal package attests all glTF runtime textures");
    for (const auto& binding : formal.requiredAssets) {
        expect(binding.canonicalPath.front() == '/',
               "formal attestation paths are canonical absolute paths");
        expect(binding.sha256.size() == 64,
               "formal attestation records exact raw SHA-256");
    }
    const auto absentFormal = preflightFormalAssetPackage(
        "/path/that/does/not/exist/phase2-v10i-formal");
    expect(!absentFormal.ok && !absentFormal.errors.empty(),
           "missing formal package fails closed with structured errors");
    expect(!absentFormal.errors.front().file.empty() &&
           !absentFormal.errors.front().field.empty(),
           "formal load errors name exact file and field");
    expectContains(formatFormalAssetLoadError(absentFormal.errors.front()),
                   "file=/path/that/does/not/exist/phase2-v10i-formal");
    auto missing = pbr;
    missing.materials.front().normal.path += ".missing";
    const auto missingValidation = validatePbrPackage(missing);
    expect(missingValidation.contractValid && missingValidation.missingFiles.size() == 1,
           "missing image is routed to explicit runtime error material");
    const auto cueBallUv = ballAtlasUvBounds(0);
    const auto fifteenUv = ballAtlasUvBounds(15);
    expect(cueBallUv[0] == 0.0f && cueBallUv[1] == 0.0f
           && cueBallUv[2] > cueBallUv[0] && cueBallUv[3] > cueBallUv[1],
           "atlas UVs are ordered and not mirrored");
    expect(fifteenUv[0] == 0.875f && fifteenUv[1] == 0.5f
           && fifteenUv[2] == 1.0f && fifteenUv[3] == 1.0f,
           "ball 15 maps to final 8x2 atlas tile without crossing seam");
    return 0;
}
