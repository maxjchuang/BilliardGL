#include "render/gltf/gltf_draw_package.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace billiardgl::render::gltf;

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

PackageImportResult loadPhase2()
{
    const std::string root = std::string(BILLIARDGL_SOURCE_ROOT) + "/assets/modern/phase2/";
    return importAssetPackage(root + "focused_practice_hall_modular_v1.gltf",
                              root + "focused_practice_hall_modular_v1.asset.json",
                              root + "focused_practice_hall_pbr_v1.params.json");
}

std::vector<LodScreenRadius> allRadii(float radius)
{
    return {{"ball", radius}, {"cue", radius}, {"lamp", radius}, {"table", radius}};
}

int selected(const DrawList& list, const std::string& group)
{
    for (const SelectedLod& lod : list.selectedLods)
        if (lod.group == group) return lod.level;
    return -1;
}

void testPackageAndDrawCounts(const ImportedGltfPackage& imported)
{
    DrawPackageResult built = buildCpuDrawPackage(imported);
    expect(built.ok, "CPU draw package builds: " + built.error);
    if (!built.ok) return;
    const CpuDrawPackage& package = built.package;
    expect(package.meshes.size() == 59,
           "all 59 promoted unique meshes are uploadable (actual "
               + std::to_string(package.meshes.size()) + ")");
    expect(package.nodes.size() == 92,
           "all 92 promoted node transforms cross the bridge (actual "
               + std::to_string(package.nodes.size()) + ")");
    expect(package.lodGroups.size() == 4, "four authored LOD groups are available");
    expect(package.staticRootNodes.size() == 1, "background is the sole static render root");
    expect(package.sourceTriangleCount == 8064,
           "promoted source triangle count crosses the bridge (actual "
               + std::to_string(package.sourceTriangleCount) + ")");
    expect(package.socketCount == 23, "23 sockets remain presentation metadata");

    LodPolicy policy;
    LodSelectionState highState;
    const DrawList high = buildDrawList(package, allRadii(200.0f), policy, &highState);
    expect(high.triangles == 4644 && high.items.size() == 24,
           "promoted LOD0 draw list has 4644 actual triangles in 24 draws (actual "
               + std::to_string(high.triangles) + "/"
               + std::to_string(high.items.size()) + ")");
    LodSelectionState mediumState;
    const DrawList medium = buildDrawList(package, allRadii(80.0f), policy, &mediumState);
    expect(medium.triangles == 2276 && medium.items.size() == 24,
           "promoted LOD1 draw list has 2276 actual triangles in 24 draws (actual "
               + std::to_string(medium.triangles) + "/"
               + std::to_string(medium.items.size()) + ")");
    LodSelectionState lowState;
    const DrawList low = buildDrawList(package, allRadii(20.0f), policy, &lowState);
    expect(low.triangles == 1264 && low.items.size() == 21,
           "promoted LOD2 draw list has 1264 actual triangles in 21 draws (actual "
               + std::to_string(low.triangles) + "/"
               + std::to_string(low.items.size()) + ")");

    for (const DrawItem& draw : high.items) {
        expect(draw.materialIndex >= 0 && static_cast<std::size_t>(draw.materialIndex) < imported.asset.materials.size(),
               "draw item carries a valid imported material index");
        expect(draw.indexCount % 3 == 0, "draw item remains indexed triangles");
        expect(!isDebugMarkerNodeName(package.nodes[static_cast<std::size_t>(draw.nodeIndex)].name),
               "SOCKET/debug marker nodes never enter draw lists");
    }
    expect(high.socketCount == 23 && medium.socketCount == 23 && low.socketCount == 23 &&
           high.socketInvariant == medium.socketInvariant && medium.socketInvariant == low.socketInvariant,
           "LOD switching leaves socket/physics alignment metadata invariant");
}

void testHysteresis(const ImportedGltfPackage& imported)
{
    DrawPackageResult built = buildCpuDrawPackage(imported);
    if (!built.ok) return;
    LodPolicy policy;
    LodSelectionState state;
    expect(selected(buildDrawList(built.package, allRadii(160.0f), policy, &state), "table") == 0,
           "large projected radius selects LOD0");
    expect(selected(buildDrawList(built.package, allRadii(132.0f), policy, &state), "table") == 0,
           "LOD0 holds inside downward hysteresis band");
    expect(selected(buildDrawList(built.package, allRadii(120.0f), policy, &state), "table") == 1,
           "LOD0 releases below downward hysteresis band");
    expect(selected(buildDrawList(built.package, allRadii(150.0f), policy, &state), "table") == 1,
           "LOD1 holds inside upward hysteresis band");
    expect(selected(buildDrawList(built.package, allRadii(160.0f), policy, &state), "table") == 0,
           "LOD1 promotes above upward hysteresis band");

    LodSelectionState distant;
    expect(selected(buildDrawList(built.package, allRadii(50.0f), policy, &distant), "ball") == 1,
           "medium projected radius selects LOD1");
    expect(selected(buildDrawList(built.package, allRadii(43.0f), policy, &distant), "ball") == 1,
           "LOD1 holds near the LOD2 boundary");
    expect(selected(buildDrawList(built.package, allRadii(39.0f), policy, &distant), "ball") == 2,
           "LOD1 releases below the LOD2 boundary");
    expect(selected(buildDrawList(built.package, allRadii(48.0f), policy, &distant), "ball") == 2,
           "LOD2 holds inside upward hysteresis band");
    expect(selected(buildDrawList(built.package, allRadii(51.0f), policy, &distant), "ball") == 1,
           "LOD2 promotes after clearing upward hysteresis band");
}

void testMarkerCullingAndErrors(ImportedGltfPackage imported)
{
    const DrawPackageResult baselinePackage = buildCpuDrawPackage(imported);
    expect(baselinePackage.ok, "marker-culling baseline package builds");
    LodSelectionState baselineState;
    const DrawList baseline = baselinePackage.ok
        ? buildDrawList(baselinePackage.package, allRadii(200.0f), {},
                        &baselineState)
        : DrawList{};
    const int socketNode = imported.asset.sockets.front().nodeIndex;
    imported.asset.nodes[static_cast<std::size_t>(socketNode)].meshIndex = 0;
    DrawPackageResult markerPackage = buildCpuDrawPackage(imported);
    expect(markerPackage.ok, "debug-marker fixture builds");
    if (markerPackage.ok) {
        LodSelectionState state;
        const DrawList list = buildDrawList(markerPackage.package, allRadii(200.0f), {}, &state);
        expect(list.triangles == 4644 && list.items.size() == 24,
               "mesh-bearing SOCKET keeps the promoted draw budget unchanged");
        expect(list.triangles == baseline.triangles
               && list.items.size() == baseline.items.size(),
               "mesh-bearing SOCKET culling is semantic, not a bumped count");
        for (const DrawItem& draw : list.items)
            expect(draw.nodeIndex != socketNode
                   && !isDebugMarkerNodeName(
                       markerPackage.package.nodes[
                           static_cast<std::size_t>(draw.nodeIndex)].name),
                   "mesh-bearing SOCKET node itself never enters the draw list");
    }

    imported.asset.meshes.front().primitives.front().indices.front() =
        static_cast<std::uint32_t>(imported.asset.meshes.front().primitives.front().vertices.size());
    DrawPackageResult invalid = buildCpuDrawPackage(imported);
    expect(!invalid.ok && invalid.error.find("mesh '") != std::string::npos &&
           invalid.error.find("accessor") != std::string::npos,
           "bridge validation failure names mesh and accessor/index data");
}

}  // namespace

int main()
{
    PackageImportResult imported = loadPhase2();
    expect(imported.ok, "Phase2 import prerequisite: " + imported.error);
    if (imported.ok) {
        testPackageAndDrawCounts(imported.package);
        testHysteresis(imported.package);
        testMarkerCullingAndErrors(imported.package);
    }
    if (failures) {
        std::cerr << failures << " glTF draw package assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "glTF CPU draw package/LOD tests passed\n";
    return EXIT_SUCCESS;
}
