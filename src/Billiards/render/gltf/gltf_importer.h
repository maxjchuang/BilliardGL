#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl { namespace render { namespace gltf {

constexpr float kMetresToCentimetres = 100.0f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct TextureBinding {
    int textureIndex = -1;
    int texcoord = 0;
    float scale = 1.0f;
    Vec2 offset;
    Vec2 uvScale = {1.0f, 1.0f};
    float rotation = 0.0f;
};

struct Texture {
    std::string name;
    std::string imageName;
    std::string uri;
    std::string mimeType;
    bool embedded = false;
};

struct Material {
    std::string name;
    Vec4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    TextureBinding baseColorTexture;
    TextureBinding metallicRoughnessTexture;
    TextureBinding normalTexture;
    TextureBinding occlusionTexture;
    bool hasMetallicRoughnessFallback = false;
    bool hasClearcoat = false;
    float clearcoatFactor = 0.0f;
    float clearcoatRoughnessFactor = 0.0f;
    TextureBinding clearcoatTexture;
    TextureBinding clearcoatRoughnessTexture;
    TextureBinding clearcoatNormalTexture;
};

struct Vertex {
    Vec3 positionCm;
    Vec3 normal;
    Vec4 tangent;
    Vec2 uv0;
    Vec4 color0 = {1.0f, 1.0f, 1.0f, 1.0f};
    bool hasNormal = false;
    bool hasTangent = false;
    bool hasUv0 = false;
    bool hasColor0 = false;
};

struct Primitive {
    int materialIndex = -1;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct Mesh {
    std::string name;
    std::vector<Primitive> primitives;
    std::size_t triangleCount = 0;
};

struct Node {
    std::string name;
    int meshIndex = -1;
    int parentIndex = -1;
    std::vector<int> children;
    // glTF column-major world transform. Translation is converted to centimetres.
    std::array<float, 16> worldTransformCm{};
};

struct Socket {
    std::string name;
    int nodeIndex = -1;
    Vec3 positionCm;
};

struct Bounds {
    Vec3 minCm;
    Vec3 maxCm;
    bool valid = false;
};

struct Asset {
    std::string sourcePath;
    bool sourceWasGlb = false;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
    std::vector<Socket> sockets;
    std::size_t triangleCount = 0;
    Bounds worldBounds;
};

struct ImportResult {
    bool ok = false;
    Asset asset;
    std::string error;
};

struct LodRoot {
    std::string group;
    int level = -1;
    int nodeIndex = -1;
    std::string nodeName;
    std::size_t expectedTriangles = 0;
};

struct PbrMaterialBinding {
    std::string materialName;
    int materialIndex = -1;
    std::string textureSet;
    Vec2 uvRepeat = {1.0f, 1.0f};
};

struct AssetPackage {
    Asset asset;
    std::vector<LodRoot> lodRoots;
    std::vector<PbrMaterialBinding> pbrBindings;
    std::size_t declaredTriangles = 0;
    std::size_t declaredTextures = 0;
    bool clearcoatExtensionsRequired = false;
    bool metallicRoughnessFallbackPresent = false;
};

using ImportedGltfPackage = AssetPackage;

struct PackageImportResult {
    bool ok = false;
    AssetPackage package;
    std::string error;
};

ImportResult importAsset(const std::string& path);

PackageImportResult importAssetPackage(
    const std::string& gltfPath,
    const std::string& manifestPath,
    const std::string& pbrParametersPath);

} } }  // namespace billiardgl::render::gltf
