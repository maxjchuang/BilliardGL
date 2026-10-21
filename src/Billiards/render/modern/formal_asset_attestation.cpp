#include "render/modern/formal_asset_attestation.h"

#include "automation_json.h"
#include "deterministic_hash.h"
#include "render/modern/pbr_materials.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace billiardgl::render {
namespace {

namespace fs = std::filesystem;

struct RequiredFile {
    const char* id;
    const char* filename;
};

constexpr std::array<RequiredFile, 6> kRequiredFiles{{
    {"modular_gltf", "focused_practice_hall_modular_v1.gltf"},
    {"asset_manifest", "focused_practice_hall_modular_v1.asset.json"},
    {"pbr_params", "focused_practice_hall_pbr_v1.params.json"},
    {"lighting_rig", "focused_practice_hall_lighting_rig_v1.json"},
    {"venue_gltf", "focused_practice_hall_venue_v1.gltf"},
    {"ball_atlas", "focused_practice_hall_ball_identity_atlas_v1.png"},
}};

std::string readBytes(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("could not open file");
    std::ostringstream bytes;
    bytes << stream.rdbuf();
    if (!stream.good() && !stream.eof())
        throw std::runtime_error("could not read file");
    return bytes.str();
}

fs::path canonicalOrAbsolute(const fs::path& path)
{
    std::error_code error;
    const fs::path canonical = fs::weakly_canonical(path, error);
    if (!error) return canonical;
    return fs::absolute(path).lexically_normal();
}

bool containedBy(const fs::path& root, const fs::path& candidate)
{
    const auto relative = candidate.lexically_relative(root);
    return !relative.empty() && *relative.begin() != "..";
}

void addError(FormalAssetPreflightResult& result, const std::string& code,
              const fs::path& file, const std::string& field,
              const std::string& message)
{
    result.errors.push_back({code, canonicalOrAbsolute(file).string(), field,
                             message});
}

json::Value parseJson(FormalAssetPreflightResult& result,
                      const fs::path& path, const std::string& field)
{
    try {
        const auto parsed = json::parse(readBytes(path));
        if (!parsed.ok) {
            addError(result, "formal_asset_json_invalid", path, field,
                     parsed.error);
            return json::Value();
        }
        if (!parsed.value.isObject()) {
            addError(result, "formal_asset_field_invalid", path, field,
                     "JSON root must be an object");
            return json::Value();
        }
        return parsed.value;
    } catch (const std::exception& error) {
        addError(result, "formal_asset_file_unavailable", path, field,
                 error.what());
        return json::Value();
    }
}

bool requireString(FormalAssetPreflightResult& result,
                   const json::Value& object, const std::string& key,
                   const fs::path& file, const std::string& field,
                   const std::string& expected = {})
{
    try {
        if (!object.has(key) || !object.at(key).isString() ||
            (!expected.empty() && object.at(key).asString() != expected)) {
            addError(result, "formal_asset_field_invalid", file,
                     field + "." + key,
                     expected.empty() ? "required string missing" :
                         "expected '" + expected + "'");
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        addError(result, "formal_asset_field_invalid", file,
                 field + "." + key, error.what());
        return false;
    }
}

void inspectGltfImages(FormalAssetPreflightResult& result,
                       const json::Value& gltf, const fs::path& gltfPath,
                       const fs::path& root, std::set<std::string>& dependencyIds)
{
    try {
        const auto& asset = gltf.at("asset");
        requireString(result, asset, "version", gltfPath, "$.asset", "2.0");
        const auto& images = gltf.at("images").asArray();
        for (std::size_t index = 0; index < images.size(); ++index) {
            const std::string field = "$.images[" + std::to_string(index) + "].uri";
            if (!images[index].has("uri") || !images[index].at("uri").isString()) {
                addError(result, "formal_asset_field_invalid", gltfPath,
                         field, "required local image URI missing");
                continue;
            }
            const std::string uri = images[index].at("uri").asString();
            if (uri.rfind("data:", 0) == 0) continue;
            const fs::path resolved = canonicalOrAbsolute(gltfPath.parent_path() / uri);
            if (!containedBy(root, resolved)) {
                addError(result, "formal_asset_path_escape", gltfPath,
                         field, "image URI leaves formal asset root");
                continue;
            }
            if (!fs::is_regular_file(resolved)) {
                addError(result, "formal_asset_runtime_dependency_missing",
                         resolved, field,
                         "missing texture; silent white/plain fallback is forbidden");
                continue;
            }
            const std::string id = resolved.filename().string();
            if (dependencyIds.insert(id).second) {
                const std::string bytes = readBytes(resolved);
                result.runtimeDependencies.push_back(
                    {id, resolved.string(), sha256Hex(bytes)});
            }
        }
    } catch (const std::exception& error) {
        addError(result, "formal_asset_field_invalid", gltfPath, "$",
                 error.what());
    }
}

void verifyManifestBinding(FormalAssetPreflightResult& result,
                           const json::Value& manifest,
                           const fs::path& manifestPath,
                           const std::string& manifestKey,
                           const FormalAssetBinding& binding)
{
    const std::string field = "$.files." + manifestKey;
    try {
        const auto& entry = manifest.at("files").at(manifestKey);
        const std::string expected = entry.at("sha256").asString();
        if (expected != "sha256:" + binding.sha256) {
            addError(result, "formal_asset_manifest_sha_mismatch",
                     manifestPath, field + ".sha256",
                     "manifest SHA does not match loaded bytes");
        }
    } catch (const std::exception& error) {
        addError(result, "formal_asset_field_invalid", manifestPath, field,
                 error.what());
    }
}

}  // namespace

FormalAssetPreflightResult preflightFormalAssetPackage(
    const std::string& assetRoot)
{
    FormalAssetPreflightResult result;
    const fs::path root = canonicalOrAbsolute(assetRoot);
    result.canonicalAssetRoot = root.string();
    if (!fs::is_directory(root)) {
        addError(result, "formal_asset_root_unavailable", root, "$",
                 "formal asset root must be an existing directory");
        return result;
    }

    for (const RequiredFile& required : kRequiredFiles) {
        const fs::path path = canonicalOrAbsolute(root / required.filename);
        if (!containedBy(root, path)) {
            addError(result, "formal_asset_path_escape", path, "$",
                     "required asset leaves formal asset root");
            continue;
        }
        try {
            const std::string bytes = readBytes(path);
            result.requiredAssets.push_back(
                {required.id, path.string(), sha256Hex(bytes)});
        } catch (const std::exception& error) {
            addError(result, "formal_asset_file_unavailable", path, "$",
                     error.what());
        }
    }
    if (result.requiredAssets.size() != kRequiredFiles.size()) return result;

    const auto find = [&](const std::string& id) -> const FormalAssetBinding& {
        const auto found = std::find_if(result.requiredAssets.begin(),
            result.requiredAssets.end(), [&](const FormalAssetBinding& item) {
                return item.id == id;
            });
        if (found == result.requiredAssets.end())
            throw std::runtime_error("required binding missing: " + id);
        return *found;
    };
    const fs::path modular(find("modular_gltf").canonicalPath);
    const fs::path venue(find("venue_gltf").canonicalPath);
    const fs::path manifestPath(find("asset_manifest").canonicalPath);
    const fs::path paramsPath(find("pbr_params").canonicalPath);
    const fs::path rigPath(find("lighting_rig").canonicalPath);

    const json::Value modularJson = parseJson(result, modular, "$.");
    const json::Value venueJson = parseJson(result, venue, "$.");
    const json::Value manifest = parseJson(result, manifestPath, "$.");
    const json::Value params = parseJson(result, paramsPath, "$.");
    const json::Value rig = parseJson(result, rigPath, "$.");
    std::set<std::string> dependencies;
    if (modularJson.isObject())
        inspectGltfImages(result, modularJson, modular, root, dependencies);
    if (venueJson.isObject())
        inspectGltfImages(result, venueJson, venue, root, dependencies);
    if (params.isObject()) {
        requireString(result, params, "package_id", paramsPath, "$",
                      "focused_practice_hall_pbr_v1");
    }
    if (rig.isObject()) {
        requireString(result, rig, "rig_id", rigPath, "$",
                      "focused_practice_hall_lighting_rig_v1");
    }
    if (manifest.isObject()) {
        verifyManifestBinding(result, manifest, manifestPath, "gltf",
                              find("modular_gltf"));
        verifyManifestBinding(result, manifest, manifestPath, "pbr_parameters",
                              find("pbr_params"));
        verifyManifestBinding(result, manifest, manifestPath, "lighting_rig",
                              find("lighting_rig"));
        verifyManifestBinding(result, manifest, manifestPath, "ball_atlas_png",
                              find("ball_atlas"));
    }

    try {
        const PbrPackageDescription package = loadPbrPackageDescription(
            paramsPath.string(), manifestPath.string(), rigPath.string());
        const PbrPackageValidation validation = validatePbrPackage(package);
        for (const std::string& missing : validation.missingFiles) {
            addError(result, "formal_asset_runtime_dependency_missing",
                     missing, "$.texture_sets[*].files",
                     "missing PBR texture; silent white/plain fallback is forbidden");
        }
        for (const std::string& error : validation.errors) {
            addError(result, "formal_asset_pbr_contract_invalid", paramsPath,
                     "$.texture_sets", error);
        }
    } catch (const std::exception& error) {
        addError(result, "formal_asset_pbr_contract_invalid", paramsPath,
                 "$.texture_sets", error.what());
    }

    result.ok = result.errors.empty();
    return result;
}

std::string formatFormalAssetLoadError(const FormalAssetLoadError& error)
{
    return "formal_asset_load_error code=" + error.code + " file=" +
        error.file + " field=" + error.field + " message=" + error.message;
}

}  // namespace billiardgl::render
