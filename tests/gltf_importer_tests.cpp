#include "render/gltf/gltf_importer.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
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

bool close(float actual, float expected, float tolerance = 0.0005f)
{
    return std::fabs(actual - expected) <= tolerance;
}

template <typename T>
void append(std::vector<unsigned char>& bytes, const T& value)
{
    const unsigned char* first = reinterpret_cast<const unsigned char*>(&value);
    bytes.insert(bytes.end(), first, first + sizeof(value));
}

void writeBytes(const fs::path& path, const std::vector<unsigned char>& bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void writeText(const fs::path& path, const std::string& text)
{
    std::ofstream output(path);
    output << text;
}

std::vector<unsigned char> fixtureBuffer()
{
    std::vector<unsigned char> bytes;
    const float positions[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const float normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
    const float tangents[] = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};
    const float uv[] = {0, 0, 1, 0, 0, 1};
    const float colors[] = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
    const std::uint16_t indices[] = {0, 1, 2};
    for (float value : positions) append(bytes, value);
    for (float value : normals) append(bytes, value);
    for (float value : tangents) append(bytes, value);
    for (float value : uv) append(bytes, value);
    for (float value : colors) append(bytes, value);
    for (std::uint16_t value : indices) append(bytes, value);
    return bytes;
}

std::string fixtureJson(bool externalBuffer, int primitiveMode = 4,
                        bool badAccessor = false, bool unsupportedRequired = false)
{
    std::ostringstream json;
    json << "{\"asset\":{\"version\":\"2.0\"}";
    if (unsupportedRequired) json << ",\"extensionsRequired\":[\"EXT_not_supported\"]";
    json << ",\"buffers\":[{\"byteLength\":198";
    if (externalBuffer) json << ",\"uri\":\"mesh.bin\"";
    json << "}],\"bufferViews\":["
         << "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
         << "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36},"
         << "{\"buffer\":0,\"byteOffset\":72,\"byteLength\":48},"
         << "{\"buffer\":0,\"byteOffset\":120,\"byteLength\":24},"
         << "{\"buffer\":0,\"byteOffset\":144,\"byteLength\":48},"
         << "{\"buffer\":0,\"byteOffset\":192,\"byteLength\":6}]"
         << ",\"accessors\":["
         << "{\"bufferView\":0,\"byteOffset\":" << (badAccessor ? 28 : 0) << ",\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
         << "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
         << "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC4\"},"
         << "{\"bufferView\":3,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
         << "{\"bufferView\":4,\"componentType\":5126,\"count\":3,\"type\":\"VEC4\"},"
         << "{\"bufferView\":5,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]"
         << ",\"materials\":[{\"name\":\"FixtureMaterial\",\"pbrMetallicRoughness\":{\"metallicFactor\":0.25,\"roughnessFactor\":0.75}}]"
         << ",\"meshes\":[{\"name\":\"FixtureMesh\",\"primitives\":[{\"mode\":" << primitiveMode
         << ",\"indices\":5,\"material\":0,\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TANGENT\":2,\"TEXCOORD_0\":3,\"COLOR_0\":4}}]}]"
         << ",\"nodes\":[{\"name\":\"FixtureNode\",\"mesh\":0,\"translation\":[1,2,3]}]"
         << ",\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
    return json.str();
}

void writeGlb(const fs::path& path)
{
    std::string json = fixtureJson(false);
    while (json.size() % 4) json.push_back(' ');
    std::vector<unsigned char> bin = fixtureBuffer();
    while (bin.size() % 4) bin.push_back(0);
    std::vector<unsigned char> glb;
    const std::uint32_t totalLength = static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bin.size());
    append(glb, std::uint32_t{0x46546c67});
    append(glb, std::uint32_t{2});
    append(glb, totalLength);
    append(glb, static_cast<std::uint32_t>(json.size()));
    append(glb, std::uint32_t{0x4e4f534a});
    glb.insert(glb.end(), json.begin(), json.end());
    append(glb, static_cast<std::uint32_t>(bin.size()));
    append(glb, std::uint32_t{0x004e4942});
    glb.insert(glb.end(), bin.begin(), bin.end());
    writeBytes(path, glb);
}

void testCurrentPhase2Package(const fs::path& root)
{
    const fs::path phase2 = root / "assets/modern/phase2";
    PackageImportResult result = importAssetPackage(
        (phase2 / "focused_practice_hall_modular_v1.gltf").string(),
        (phase2 / "focused_practice_hall_modular_v1.asset.json").string(),
        (phase2 / "focused_practice_hall_pbr_v1.params.json").string());
    expect(result.ok, "Phase2 package imports: " + result.error);
    if (!result.ok) return;
    const AssetPackage& package = result.package;
    expect(package.asset.triangleCount == 8064,
           "promoted Phase2 package triangle count is 8064 (actual "
               + std::to_string(package.asset.triangleCount) + ")");
    expect(package.declaredTriangles == package.asset.triangleCount,
           "promoted geometry count remains bound to the package manifest");
    expect(package.asset.sockets.size() == 23, "Phase2 exposes 23 SOCKET markers");
    expect(package.asset.textures.size() == 22, "Phase2 exposes 22 runtime textures");
    expect(package.asset.materials.size() == 29, "Phase2 exposes 29 materials");
    expect(package.lodRoots.size() == 12, "Phase2 resolves four three-level LOD sets");
    expect(package.pbrBindings.size() == 9, "Phase2 resolves all PBR material bindings");
    expect(package.metallicRoughnessFallbackPresent && !package.clearcoatExtensionsRequired,
           "Phase2 declares optional clearcoat with metallic-roughness fallback");
    std::size_t clearcoatCount = 0;
    for (const Material& material : package.asset.materials) {
        if (!material.hasClearcoat) continue;
        ++clearcoatCount;
        expect(material.hasMetallicRoughnessFallback,
               "clearcoat material keeps core fallback: " + material.name);
    }
    expect(clearcoatCount == 17, "Phase2 imports clearcoat on walnut and 16 ball materials");
    bool foundSurface = false;
    for (const Socket& socket : package.asset.sockets) {
        if (socket.name == "SOCKET_play_surface_origin") {
            foundSurface = true;
            expect(close(socket.positionCm.y, 86.483976f), "metres are converted to centimetres at the importer boundary");
        }
    }
    expect(foundSurface, "play surface socket is present");
    expect(package.asset.worldBounds.valid, "Phase2 world bounds are available");
    bool foundBallLod0 = false;
    for (const Mesh& mesh : package.asset.meshes) {
        if (mesh.name != "MESH_ball_shared_LOD0") continue;
        foundBallLod0 = true;
        expect(mesh.primitives.size() == 1,
               "authored LOD0 ball remains one indexed primitive");
        for (const Primitive& primitive : mesh.primitives)
            for (const Vertex& vertex : primitive.vertices) {
                const float radius = std::sqrt(
                    vertex.positionCm.x * vertex.positionCm.x
                    + vertex.positionCm.y * vertex.positionCm.y
                    + vertex.positionCm.z * vertex.positionCm.z);
                const float normalLength = std::sqrt(
                    vertex.normal.x * vertex.normal.x
                    + vertex.normal.y * vertex.normal.y
                    + vertex.normal.z * vertex.normal.z);
                expect(close(radius, 2.8575f, 0.00001f),
                       "every actual LOD0 ball vertex lies on the 2.8575 cm sphere");
                expect(close(normalLength, 1.0f, 0.00001f),
                       "every actual LOD0 ball normal remains unit length");
                const float radialAlignment =
                    (vertex.positionCm.x * vertex.normal.x
                     + vertex.positionCm.y * vertex.normal.y
                     + vertex.positionCm.z * vertex.normal.z)
                    / (radius * normalLength);
                expect(radialAlignment >= 0.99999f,
                       "every actual LOD0 ball normal points along its outward "
                       "geometric sphere radius");
                expect(!vertex.hasColor0 && close(vertex.color0.x, 1.0f)
                       && close(vertex.color0.y, 1.0f)
                       && close(vertex.color0.z, 1.0f)
                       && close(vertex.color0.w, 1.0f),
                       "absent ball COLOR_0 resolves to neutral white vertex color");
            }
    }
    expect(foundBallLod0, "promoted package contains the authored LOD0 ball mesh");
}

void testCurrentM1Graybox(const fs::path& root)
{
    ImportResult result = importAsset((root / "assets/modern/graybox/focused_practice_hall_m1_graybox.gltf").string());
    expect(result.ok, "M1 graybox imports: " + result.error);
    if (!result.ok) return;
    expect(result.asset.triangleCount == 4784, "M1 unique triangle count is 4784");
    expect(result.asset.sockets.size() == 23, "M1 exposes 23 SOCKET markers");
    expect(result.asset.materials.size() == 9, "M1 exposes nine materials");
    expect(result.asset.textures.empty(), "M1 does not require textures");
}

void testExternalBufferAndAttributes(const fs::path& directory)
{
    writeBytes(directory / "mesh.bin", fixtureBuffer());
    writeText(directory / "external.gltf", fixtureJson(true));
    ImportResult result = importAsset((directory / "external.gltf").string());
    expect(result.ok, "relative external buffer imports: " + result.error);
    if (!result.ok) return;
    const Vertex& vertex = result.asset.meshes[0].primitives[0].vertices[1];
    expect(vertex.hasNormal && vertex.hasTangent && vertex.hasUv0 && vertex.hasColor0,
           "NORMAL/TANGENT/UV0/COLOR0 attributes are imported");
    expect(close(vertex.positionCm.x, 100.0f), "local POSITION is converted from metres to centimetres");
    expect(close(result.asset.nodes[0].worldTransformCm[12], 100.0f) &&
           close(result.asset.nodes[0].worldTransformCm[13], 200.0f) &&
           close(result.asset.nodes[0].worldTransformCm[14], 300.0f),
           "node transform translation is converted to centimetres");
    expect(close(result.asset.worldBounds.maxCm.x, 200.0f) && close(result.asset.worldBounds.maxCm.y, 300.0f),
           "world bounds apply node transforms");
}

void testGlb(const fs::path& directory)
{
    writeGlb(directory / "fixture.glb");
    ImportResult result = importAsset((directory / "fixture.glb").string());
    expect(result.ok, "GLB imports: " + result.error);
    if (result.ok) {
        expect(result.asset.sourceWasGlb, "GLB source format is reported");
        expect(result.asset.triangleCount == 1, "GLB triangle is imported");
    }
}

void testErrorPaths(const fs::path& directory)
{
    writeBytes(directory / "mesh.bin", fixtureBuffer());

    writeText(directory / "required.gltf", fixtureJson(true, 4, false, true));
    ImportResult required = importAsset((directory / "required.gltf").string());
    expect(!required.ok && required.error.find("EXT_not_supported") != std::string::npos,
           "unsupported required extension is actionable");

    writeText(directory / "lines.gltf", fixtureJson(true, 1));
    ImportResult lines = importAsset((directory / "lines.gltf").string());
    expect(!lines.ok && lines.error.find("non-triangle") != std::string::npos &&
           lines.error.find("FixtureNode") != std::string::npos &&
           lines.error.find("FixtureMesh") != std::string::npos &&
           lines.error.find("FixtureMaterial") != std::string::npos,
           "non-triangle error includes node/mesh/material context");

    writeText(directory / "bounds.gltf", fixtureJson(true, 4, true));
    ImportResult bounds = importAsset((directory / "bounds.gltf").string());
    expect(!bounds.ok && bounds.error.find("out of bufferView bounds") != std::string::npos &&
           bounds.error.find("FixtureNode") != std::string::npos &&
           bounds.error.find("FixtureMesh") != std::string::npos &&
           bounds.error.find("FixtureMaterial") != std::string::npos,
           "out-of-bounds accessor is rejected with node/mesh/material context");
}

}  // namespace

int main()
{
    const fs::path root = BILLIARDGL_SOURCE_ROOT;
    const fs::path temp = fs::temp_directory_path() / "billiardgl-gltf-importer-tests";
    std::error_code ignored;
    fs::remove_all(temp, ignored);
    fs::create_directories(temp);

    testCurrentPhase2Package(root);
    testCurrentM1Graybox(root);
    testExternalBufferAndAttributes(temp);
    testGlb(temp);
    testErrorPaths(temp);

    fs::remove_all(temp, ignored);
    if (failures != 0) {
        std::cerr << failures << " glTF importer assertion(s) failed\n";
        return 1;
    }
    std::cout << "glTF importer CPU tests passed\n";
    return 0;
}
