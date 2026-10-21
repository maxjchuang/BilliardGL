#pragma once

#include "render/gltf/gltf_importer.h"
#include "render/modern/render_data.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace billiardgl { namespace render { namespace gltf {

struct CpuPrimitiveRange {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    int materialIndex = -1;
};

struct CpuMesh {
    std::string sourceMeshName;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<CpuPrimitiveRange> primitives;
    std::size_t triangleCount = 0;
};

struct DrawNode {
    std::string name;
    int meshIndex = -1;
    std::vector<int> children;
    Matrix4 worldTransformCm{};
};

struct DrawLodLevel {
    int level = -1;
    int rootNodeIndex = -1;
    std::size_t triangleCount = 0;
};

struct DrawLodGroup {
    std::string name;
    std::vector<DrawLodLevel> levels;
};

struct CpuDrawPackage {
    std::vector<CpuMesh> meshes;
    std::vector<DrawNode> nodes;
    std::vector<DrawLodGroup> lodGroups;
    std::vector<int> staticRootNodes;
    std::size_t sourceTriangleCount = 0;
    std::size_t socketCount = 0;
    std::uint64_t socketInvariant = 0;
};

struct DrawPackageResult {
    bool ok = false;
    CpuDrawPackage package;
    std::string error;
};

struct LodPolicy {
    float lod0MinimumRadiusPixels = 140.0f;
    float lod1MinimumRadiusPixels = 45.0f;
    float hysteresisFraction = 0.10f;
};

struct LodSelectionState {
    std::map<std::string, int> selectedLevels;
};

struct LodScreenRadius {
    std::string group;
    float radiusPixels = 0.0f;
};

struct DrawItem {
    int nodeIndex = -1;
    int meshIndex = -1;
    int primitiveIndex = -1;
    int materialIndex = -1;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    Matrix4 worldTransformCm{};
};

struct SelectedLod {
    std::string group;
    int level = -1;
    float radiusPixels = 0.0f;
};

struct DrawList {
    std::vector<DrawItem> items;
    std::vector<SelectedLod> selectedLods;
    std::size_t triangles = 0;
    std::size_t socketCount = 0;
    std::uint64_t socketInvariant = 0;
};

DrawPackageResult buildCpuDrawPackage(const ImportedGltfPackage& imported);

DrawList buildDrawList(const CpuDrawPackage& package,
                       const std::vector<LodScreenRadius>& screenRadii,
                       const LodPolicy& policy,
                       LodSelectionState* state);

bool isDebugMarkerNodeName(const std::string& name) noexcept;

} } }  // namespace billiardgl::render::gltf
