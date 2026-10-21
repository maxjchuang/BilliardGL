#include "render/gltf/gltf_draw_package.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace billiardgl { namespace render { namespace gltf {
namespace {

void hashBytes(std::uint64_t& hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
}

std::uint64_t socketInvariant(const std::vector<Socket>& sockets)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (const Socket& socket : sockets) {
        hashBytes(hash, socket.name.data(), socket.name.size());
        hashBytes(hash, &socket.positionCm.x, sizeof(socket.positionCm.x));
        hashBytes(hash, &socket.positionCm.y, sizeof(socket.positionCm.y));
        hashBytes(hash, &socket.positionCm.z, sizeof(socket.positionCm.z));
    }
    return hash;
}

int desiredLod(float radius, const LodPolicy& policy)
{
    if (radius >= policy.lod0MinimumRadiusPixels) return 0;
    if (radius >= policy.lod1MinimumRadiusPixels) return 1;
    return 2;
}

int selectedLod(float radius, int previous, const LodPolicy& policy)
{
    const int desired = desiredLod(radius, policy);
    if (previous < 0 || previous > 2 || desired == previous) return desired;
    const float h = std::clamp(policy.hysteresisFraction, 0.0f, 0.49f);
    if (previous == 0 && desired > 0 && radius >= policy.lod0MinimumRadiusPixels * (1.0f - h)) return 0;
    if (previous == 1 && desired == 0 && radius <= policy.lod0MinimumRadiusPixels * (1.0f + h)) return 1;
    if (previous == 1 && desired == 2 && radius >= policy.lod1MinimumRadiusPixels * (1.0f - h)) return 1;
    if (previous == 2 && desired < 2 && radius <= policy.lod1MinimumRadiusPixels * (1.0f + h)) return 2;
    return desired;
}

float radiusFor(const std::vector<LodScreenRadius>& radii, const std::string& group)
{
    for (const LodScreenRadius& value : radii)
        if (value.group == group) return std::max(value.radiusPixels, 0.0f);
    return 0.0f;
}

const DrawLodLevel* findLevel(const DrawLodGroup& group, int level)
{
    for (const DrawLodLevel& candidate : group.levels)
        if (candidate.level == level) return &candidate;
    return group.levels.empty() ? nullptr : &group.levels.back();
}

void appendNode(const CpuDrawPackage& package, int nodeIndex, DrawList& list,
                std::set<int>& active)
{
    if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= package.nodes.size()) return;
    if (!active.insert(nodeIndex).second) return;
    const DrawNode& node = package.nodes[static_cast<std::size_t>(nodeIndex)];
    if (isDebugMarkerNodeName(node.name)) {
        active.erase(nodeIndex);
        return;
    }
    if (node.meshIndex >= 0 && static_cast<std::size_t>(node.meshIndex) < package.meshes.size()) {
        const CpuMesh& mesh = package.meshes[static_cast<std::size_t>(node.meshIndex)];
        for (std::size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex) {
            const CpuPrimitiveRange& primitive = mesh.primitives[primitiveIndex];
            list.items.push_back({nodeIndex, node.meshIndex, static_cast<int>(primitiveIndex),
                                  primitive.materialIndex, primitive.firstIndex,
                                  primitive.indexCount, node.worldTransformCm});
            list.triangles += primitive.indexCount / 3;
        }
    }
    for (int child : node.children) appendNode(package, child, list, active);
    active.erase(nodeIndex);
}

}  // namespace

bool isDebugMarkerNodeName(const std::string& name) noexcept
{
    return name.rfind("SOCKET_", 0) == 0 || name.rfind("DEBUG_", 0) == 0 ||
           name.rfind("MARKER_", 0) == 0;
}

DrawPackageResult buildCpuDrawPackage(const ImportedGltfPackage& imported)
{
    DrawPackageResult result;
    CpuDrawPackage package;
    package.sourceTriangleCount = imported.asset.triangleCount;
    package.socketCount = imported.asset.sockets.size();
    package.socketInvariant = socketInvariant(imported.asset.sockets);
    package.meshes.reserve(imported.asset.meshes.size());

    for (const Mesh& sourceMesh : imported.asset.meshes) {
        CpuMesh mesh;
        mesh.sourceMeshName = sourceMesh.name;
        for (const Primitive& sourcePrimitive : sourceMesh.primitives) {
            if (sourcePrimitive.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
                mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() - sourcePrimitive.vertices.size() ||
                mesh.indices.size() > std::numeric_limits<std::uint32_t>::max() - sourcePrimitive.indices.size()) {
                result.error = "mesh '" + sourceMesh.name + "' accessor/index data exceeds 32-bit GPU limits";
                return result;
            }
            const std::uint32_t baseVertex = static_cast<std::uint32_t>(mesh.vertices.size());
            CpuPrimitiveRange range;
            range.firstIndex = static_cast<std::uint32_t>(mesh.indices.size());
            range.indexCount = static_cast<std::uint32_t>(sourcePrimitive.indices.size());
            range.materialIndex = sourcePrimitive.materialIndex;
            for (const Vertex& source : sourcePrimitive.vertices) {
                MeshVertex vertex;
                vertex.position = {{source.positionCm.x, source.positionCm.y, source.positionCm.z}};
                vertex.normal = {{source.normal.x, source.normal.y, source.normal.z}};
                vertex.tangent = {{source.tangent.x, source.tangent.y, source.tangent.z, source.tangent.w}};
                vertex.texCoord0 = {{source.uv0.x, source.uv0.y}};
                vertex.color = {{source.color0.x, source.color0.y, source.color0.z, source.color0.w}};
                mesh.vertices.push_back(vertex);
            }
            for (std::uint32_t index : sourcePrimitive.indices) {
                if (index >= sourcePrimitive.vertices.size()) {
                    result.error = "mesh '" + sourceMesh.name + "' accessor index exceeds vertex range";
                    return result;
                }
                mesh.indices.push_back(baseVertex + index);
            }
            mesh.triangleCount += range.indexCount / 3;
            mesh.primitives.push_back(range);
        }
        package.meshes.push_back(std::move(mesh));
    }

    package.nodes.reserve(imported.asset.nodes.size());
    for (const Node& source : imported.asset.nodes) {
        DrawNode node;
        node.name = source.name;
        node.meshIndex = source.meshIndex;
        node.children = source.children;
        std::copy(source.worldTransformCm.begin(), source.worldTransformCm.end(), node.worldTransformCm.begin());
        package.nodes.push_back(std::move(node));
    }

    std::map<std::string, DrawLodGroup> groups;
    std::set<int> lodRoots;
    for (const LodRoot& source : imported.lodRoots) {
        DrawLodGroup& group = groups[source.group];
        group.name = source.group;
        group.levels.push_back({source.level, source.nodeIndex, source.expectedTriangles});
        lodRoots.insert(source.nodeIndex);
    }
    for (auto& entry : groups) {
        std::sort(entry.second.levels.begin(), entry.second.levels.end(),
                  [](const DrawLodLevel& a, const DrawLodLevel& b) { return a.level < b.level; });
        package.lodGroups.push_back(std::move(entry.second));
    }
    for (std::size_t i = 0; i < imported.asset.nodes.size(); ++i) {
        const Node& node = imported.asset.nodes[i];
        if (node.parentIndex < 0 && lodRoots.count(static_cast<int>(i)) == 0 &&
            !isDebugMarkerNodeName(node.name))
            package.staticRootNodes.push_back(static_cast<int>(i));
    }

    result.ok = true;
    result.package = std::move(package);
    return result;
}

DrawList buildDrawList(const CpuDrawPackage& package,
                       const std::vector<LodScreenRadius>& screenRadii,
                       const LodPolicy& policy,
                       LodSelectionState* state)
{
    DrawList list;
    list.socketCount = package.socketCount;
    list.socketInvariant = package.socketInvariant;
    std::set<int> active;
    for (int root : package.staticRootNodes) appendNode(package, root, list, active);
    for (const DrawLodGroup& group : package.lodGroups) {
        const float radius = radiusFor(screenRadii, group.name);
        int previous = -1;
        if (state) {
            const auto found = state->selectedLevels.find(group.name);
            if (found != state->selectedLevels.end()) previous = found->second;
        }
        const int level = selectedLod(radius, previous, policy);
        if (state) state->selectedLevels[group.name] = level;
        list.selectedLods.push_back({group.name, level, radius});
        const DrawLodLevel* selected = findLevel(group, level);
        if (selected) appendNode(package, selected->rootNodeIndex, list, active);
    }
    return list;
}

} } }  // namespace billiardgl::render::gltf
