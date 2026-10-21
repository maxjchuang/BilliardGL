#include "render/gltf/gltf_importer.h"

#include "automation_json.h"

#define CGLTF_IMPLEMENTATION
#include "dependencies/cgltf/cgltf.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace billiardgl { namespace render { namespace gltf {
namespace {

std::string safeName(const char* name, const char* fallback)
{
    return name && *name ? name : fallback;
}

const char* resultName(cgltf_result result)
{
    switch (result) {
    case cgltf_result_success: return "success";
    case cgltf_result_data_too_short: return "data too short";
    case cgltf_result_unknown_format: return "unknown format";
    case cgltf_result_invalid_json: return "invalid JSON";
    case cgltf_result_invalid_gltf: return "invalid glTF";
    case cgltf_result_invalid_options: return "invalid options";
    case cgltf_result_file_not_found: return "file not found";
    case cgltf_result_io_error: return "I/O error";
    case cgltf_result_out_of_memory: return "out of memory";
    case cgltf_result_legacy_gltf: return "legacy glTF";
    default: return "unknown error";
    }
}

bool checkedRange(std::size_t offset, std::size_t count, std::size_t stride,
                  std::size_t elementSize, std::size_t limit)
{
    if (elementSize == 0 || offset > limit) return false;
    if (count == 0) return true;
    if (stride < elementSize) return false;
    const std::size_t steps = count - 1;
    if (steps > (std::numeric_limits<std::size_t>::max() - elementSize) / stride) return false;
    const std::size_t bytes = steps * stride + elementSize;
    return bytes <= limit - offset;
}

bool validateView(const cgltf_buffer_view* view)
{
    if (!view || !view->buffer) return false;
    if (view->offset > view->buffer->size) return false;
    return view->size <= view->buffer->size - view->offset;
}

bool validateAccessorBounds(const cgltf_accessor* accessor, std::string& reason)
{
    const std::size_t elementSize = cgltf_calc_size(accessor->type, accessor->component_type);
    if (elementSize == 0) { reason = "invalid accessor component/type"; return false; }
    if (accessor->buffer_view) {
        if (!validateView(accessor->buffer_view)) { reason = "bufferView is out of buffer bounds"; return false; }
        const std::size_t stride = accessor->stride ? accessor->stride : elementSize;
        if (!checkedRange(accessor->offset, accessor->count, stride, elementSize,
                          accessor->buffer_view->size)) {
            reason = "accessor is out of bufferView bounds";
            return false;
        }
    } else if (!accessor->is_sparse) {
        reason = "accessor has no bufferView";
        return false;
    }
    if (accessor->is_sparse) {
        const cgltf_accessor_sparse& sparse = accessor->sparse;
        const std::size_t indexSize = cgltf_component_size(sparse.indices_component_type);
        if (sparse.count > accessor->count || !validateView(sparse.indices_buffer_view) ||
            !checkedRange(sparse.indices_byte_offset, sparse.count, indexSize, indexSize,
                          sparse.indices_buffer_view->size)) {
            reason = "sparse indices are out of bounds";
            return false;
        }
        if (!validateView(sparse.values_buffer_view) ||
            !checkedRange(sparse.values_byte_offset, sparse.count, elementSize, elementSize,
                          sparse.values_buffer_view->size)) {
            reason = "sparse values are out of bounds";
            return false;
        }
    }
    return true;
}

template <typename T>
int pointerIndex(const T* pointer, const T* base, std::size_t count)
{
    if (!pointer || !base || pointer < base || pointer >= base + count) return -1;
    return static_cast<int>(pointer - base);
}

TextureBinding textureBinding(const cgltf_texture_view& view, const cgltf_data* data)
{
    TextureBinding result;
    result.textureIndex = pointerIndex(view.texture, data->textures, data->textures_count);
    result.texcoord = view.texcoord;
    result.scale = view.scale;
    if (view.has_transform) {
        result.offset = {view.transform.offset[0], view.transform.offset[1]};
        result.uvScale = {view.transform.scale[0], view.transform.scale[1]};
        result.rotation = view.transform.rotation;
        if (view.transform.has_texcoord) result.texcoord = view.transform.texcoord;
    }
    return result;
}

std::string primitiveContext(const cgltf_data* data, const cgltf_mesh* mesh,
                             const cgltf_material* material)
{
    std::string nodeName = "<unreferenced>";
    for (std::size_t i = 0; i < data->nodes_count; ++i) {
        if (data->nodes[i].mesh == mesh) {
            nodeName = safeName(data->nodes[i].name, "<unnamed>");
            break;
        }
    }
    std::ostringstream out;
    out << "node='" << nodeName << "', mesh='" << safeName(mesh->name, "<unnamed>")
        << "', material='" << (material ? safeName(material->name, "<unnamed>") : "<none>") << "'";
    return out.str();
}

std::string accessorContext(const cgltf_data* data, const cgltf_accessor* accessor)
{
    for (std::size_t meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex) {
        const cgltf_mesh& mesh = data->meshes[meshIndex];
        for (std::size_t primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex) {
            const cgltf_primitive& primitive = mesh.primitives[primitiveIndex];
            if (primitive.indices == accessor) return primitiveContext(data, &mesh, primitive.material);
            for (std::size_t attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
                if (primitive.attributes[attributeIndex].data == accessor)
                    return primitiveContext(data, &mesh, primitive.material);
        }
    }
    return "node='<unreferenced>', mesh='<unreferenced>', material='<none>'";
}

bool readAttribute(const cgltf_accessor* accessor, std::size_t index, float* values,
                   std::size_t components)
{
    return accessor && cgltf_accessor_read_float(accessor, index, values, components);
}

Vec3 transformPoint(const std::array<float, 16>& m, const Vec3& p)
{
    return {
        m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
        m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
        m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]
    };
}

void addBounds(Bounds& bounds, const Vec3& p)
{
    if (!bounds.valid) {
        bounds.minCm = bounds.maxCm = p;
        bounds.valid = true;
        return;
    }
    bounds.minCm.x = std::min(bounds.minCm.x, p.x);
    bounds.minCm.y = std::min(bounds.minCm.y, p.y);
    bounds.minCm.z = std::min(bounds.minCm.z, p.z);
    bounds.maxCm.x = std::max(bounds.maxCm.x, p.x);
    bounds.maxCm.y = std::max(bounds.maxCm.y, p.y);
    bounds.maxCm.z = std::max(bounds.maxCm.z, p.z);
}

bool startsWith(const std::string& value, const char* prefix)
{
    return value.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

std::string readTextFile(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream) throw std::runtime_error("cannot open '" + path + "'");
    std::ostringstream text;
    text << stream.rdbuf();
    if (!stream.good() && !stream.eof()) throw std::runtime_error("cannot read '" + path + "'");
    return text.str();
}

json::Value parseJsonFile(const std::string& path)
{
    const json::ParseResult parsed = json::parse(readTextFile(path));
    if (!parsed.ok) throw std::runtime_error("invalid JSON in '" + path + "': " + parsed.error);
    return parsed.value;
}

int findMaterial(const Asset& asset, const std::string& name)
{
    for (std::size_t i = 0; i < asset.materials.size(); ++i)
        if (asset.materials[i].name == name) return static_cast<int>(i);
    return -1;
}

bool hasTextureUri(const Asset& asset, const std::string& filename)
{
    for (const Texture& texture : asset.textures) {
        if (texture.uri == filename) return true;
        if (texture.uri.size() > filename.size() &&
            texture.uri.compare(texture.uri.size() - filename.size(), filename.size(), filename) == 0)
            return true;
    }
    return false;
}

bool nodeTreeTriangles(const Asset& asset, int nodeIndex, std::size_t& total,
                       std::unordered_set<int>& active)
{
    if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= asset.nodes.size()) return false;
    if (!active.insert(nodeIndex).second) return false;
    const Node& node = asset.nodes[nodeIndex];
    if (node.meshIndex >= 0) total += asset.meshes[static_cast<std::size_t>(node.meshIndex)].triangleCount;
    for (int child : node.children) {
        if (!nodeTreeTriangles(asset, child, total, active)) return false;
    }
    active.erase(nodeIndex);
    return true;
}

}  // namespace

ImportResult importAsset(const std::string& path)
{
    ImportResult result;
    cgltf_options options{};
    cgltf_data* raw = nullptr;
    cgltf_result status = cgltf_parse_file(&options, path.c_str(), &raw);
    if (status != cgltf_result_success) {
        result.error = "glTF parse failed for '" + path + "': " + resultName(status);
        return result;
    }
    struct DataGuard {
        cgltf_data* data;
        ~DataGuard() { cgltf_free(data); }
    } guard{raw};

    static const std::unordered_set<std::string> supportedRequiredExtensions = {
        "KHR_materials_clearcoat", "KHR_texture_transform"
    };
    for (std::size_t i = 0; i < raw->extensions_required_count; ++i) {
        const std::string extension = raw->extensions_required[i] ? raw->extensions_required[i] : "";
        if (supportedRequiredExtensions.count(extension) == 0) {
            result.error = "unsupported required glTF extension '" + extension + "'";
            return result;
        }
    }

    status = cgltf_load_buffers(&options, raw, path.c_str());
    if (status != cgltf_result_success) {
        result.error = "glTF buffer load failed for '" + path + "': " + resultName(status);
        return result;
    }
    for (std::size_t i = 0; i < raw->accessors_count; ++i) {
        std::string reason;
        if (!validateAccessorBounds(&raw->accessors[i], reason)) {
            std::ostringstream error;
            error << "accessor " << i << " is invalid: " << reason << " ("
                  << accessorContext(raw, &raw->accessors[i]) << ")";
            result.error = error.str();
            return result;
        }
    }
    status = cgltf_validate(raw);
    if (status != cgltf_result_success) {
        result.error = std::string("glTF validation failed: ") + resultName(status);
        return result;
    }

    Asset asset;
    asset.sourcePath = path;
    asset.sourceWasGlb = raw->file_type == cgltf_file_type_glb;

    asset.textures.reserve(raw->textures_count);
    for (std::size_t i = 0; i < raw->textures_count; ++i) {
        const cgltf_texture& source = raw->textures[i];
        const cgltf_image* image = source.image;
        Texture texture;
        texture.name = safeName(source.name, "");
        if (image) {
            texture.imageName = safeName(image->name, "");
            texture.uri = safeName(image->uri, "");
            texture.mimeType = safeName(image->mime_type, "");
            texture.embedded = image->buffer_view || startsWith(texture.uri, "data:");
        }
        asset.textures.push_back(texture);
    }

    asset.materials.reserve(raw->materials_count);
    for (std::size_t i = 0; i < raw->materials_count; ++i) {
        const cgltf_material& source = raw->materials[i];
        Material material;
        material.name = safeName(source.name, "");
        material.hasMetallicRoughnessFallback = source.has_pbr_metallic_roughness;
        if (source.has_pbr_metallic_roughness) {
            const cgltf_pbr_metallic_roughness& pbr = source.pbr_metallic_roughness;
            material.baseColorFactor = {pbr.base_color_factor[0], pbr.base_color_factor[1],
                                        pbr.base_color_factor[2], pbr.base_color_factor[3]};
            material.metallicFactor = pbr.metallic_factor;
            material.roughnessFactor = pbr.roughness_factor;
            material.baseColorTexture = textureBinding(pbr.base_color_texture, raw);
            material.metallicRoughnessTexture = textureBinding(pbr.metallic_roughness_texture, raw);
        }
        material.normalTexture = textureBinding(source.normal_texture, raw);
        material.occlusionTexture = textureBinding(source.occlusion_texture, raw);
        material.hasClearcoat = source.has_clearcoat;
        if (source.has_clearcoat) {
            material.clearcoatFactor = source.clearcoat.clearcoat_factor;
            material.clearcoatRoughnessFactor = source.clearcoat.clearcoat_roughness_factor;
            material.clearcoatTexture = textureBinding(source.clearcoat.clearcoat_texture, raw);
            material.clearcoatRoughnessTexture = textureBinding(source.clearcoat.clearcoat_roughness_texture, raw);
            material.clearcoatNormalTexture = textureBinding(source.clearcoat.clearcoat_normal_texture, raw);
        }
        asset.materials.push_back(material);
    }

    asset.meshes.reserve(raw->meshes_count);
    for (std::size_t meshIndex = 0; meshIndex < raw->meshes_count; ++meshIndex) {
        const cgltf_mesh& sourceMesh = raw->meshes[meshIndex];
        Mesh mesh;
        mesh.name = safeName(sourceMesh.name, "");
        mesh.primitives.reserve(sourceMesh.primitives_count);
        for (std::size_t primitiveIndex = 0; primitiveIndex < sourceMesh.primitives_count; ++primitiveIndex) {
            const cgltf_primitive& source = sourceMesh.primitives[primitiveIndex];
            const std::string context = primitiveContext(raw, &sourceMesh, source.material);
            if (source.type != cgltf_primitive_type_triangles) {
                result.error = "non-triangle primitive rejected (" + context + ")";
                return result;
            }
            if (!source.indices) {
                result.error = "non-indexed primitive rejected (" + context + ")";
                return result;
            }
            const cgltf_accessor* position = cgltf_find_accessor(&source, cgltf_attribute_type_position, 0);
            const cgltf_accessor* normal = cgltf_find_accessor(&source, cgltf_attribute_type_normal, 0);
            const cgltf_accessor* tangent = cgltf_find_accessor(&source, cgltf_attribute_type_tangent, 0);
            const cgltf_accessor* uv0 = cgltf_find_accessor(&source, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor* color0 = cgltf_find_accessor(&source, cgltf_attribute_type_color, 0);
            if (!position || position->type != cgltf_type_vec3) {
                result.error = "POSITION vec3 is required (" + context + ")";
                return result;
            }
            const auto matchingCount = [position](const cgltf_accessor* accessor) {
                return !accessor || accessor->count == position->count;
            };
            if (!matchingCount(normal) || !matchingCount(tangent) || !matchingCount(uv0) || !matchingCount(color0)) {
                result.error = "vertex attribute counts do not match POSITION (" + context + ")";
                return result;
            }
            if ((normal && normal->type != cgltf_type_vec3) ||
                (tangent && tangent->type != cgltf_type_vec4) ||
                (uv0 && uv0->type != cgltf_type_vec2) ||
                (color0 && color0->type != cgltf_type_vec3 && color0->type != cgltf_type_vec4)) {
                result.error = "unsupported vertex attribute shape (" + context + ")";
                return result;
            }
            if (source.indices->type != cgltf_type_scalar ||
                (source.indices->component_type != cgltf_component_type_r_8u &&
                 source.indices->component_type != cgltf_component_type_r_16u &&
                 source.indices->component_type != cgltf_component_type_r_32u)) {
                result.error = "indices must be unsigned scalar values (" + context + ")";
                return result;
            }
            if (source.indices->count % 3 != 0) {
                result.error = "triangle index count is not divisible by three (" + context + ")";
                return result;
            }

            Primitive primitive;
            primitive.materialIndex = pointerIndex(source.material, raw->materials, raw->materials_count);
            primitive.vertices.resize(position->count);
            for (std::size_t vertexIndex = 0; vertexIndex < position->count; ++vertexIndex) {
                Vertex& vertex = primitive.vertices[vertexIndex];
                float values[4] = {};
                if (!readAttribute(position, vertexIndex, values, 3)) {
                    result.error = "cannot read POSITION (" + context + ")";
                    return result;
                }
                vertex.positionCm = {values[0] * kMetresToCentimetres,
                                     values[1] * kMetresToCentimetres,
                                     values[2] * kMetresToCentimetres};
                if (normal) {
                    if (!readAttribute(normal, vertexIndex, values, 3)) { result.error = "cannot read NORMAL (" + context + ")"; return result; }
                    vertex.normal = {values[0], values[1], values[2]}; vertex.hasNormal = true;
                }
                if (tangent) {
                    if (!readAttribute(tangent, vertexIndex, values, 4)) { result.error = "cannot read TANGENT (" + context + ")"; return result; }
                    vertex.tangent = {values[0], values[1], values[2], values[3]}; vertex.hasTangent = true;
                }
                if (uv0) {
                    if (!readAttribute(uv0, vertexIndex, values, 2)) { result.error = "cannot read TEXCOORD_0 (" + context + ")"; return result; }
                    vertex.uv0 = {values[0], values[1]}; vertex.hasUv0 = true;
                }
                if (color0) {
                    const std::size_t components = color0->type == cgltf_type_vec3 ? 3 : 4;
                    if (!readAttribute(color0, vertexIndex, values, components)) { result.error = "cannot read COLOR_0 (" + context + ")"; return result; }
                    vertex.color0 = {values[0], values[1], values[2], components == 4 ? values[3] : 1.0f};
                    vertex.hasColor0 = true;
                }
            }
            primitive.indices.reserve(source.indices->count);
            for (std::size_t index = 0; index < source.indices->count; ++index) {
                const std::size_t value = cgltf_accessor_read_index(source.indices, index);
                if (value >= primitive.vertices.size() || value > std::numeric_limits<std::uint32_t>::max()) {
                    result.error = "index is outside POSITION bounds (" + context + ")";
                    return result;
                }
                primitive.indices.push_back(static_cast<std::uint32_t>(value));
            }
            mesh.triangleCount += primitive.indices.size() / 3;
            mesh.primitives.push_back(std::move(primitive));
        }
        asset.triangleCount += mesh.triangleCount;
        asset.meshes.push_back(std::move(mesh));
    }

    asset.nodes.resize(raw->nodes_count);
    for (std::size_t i = 0; i < raw->nodes_count; ++i) {
        const cgltf_node& source = raw->nodes[i];
        Node& node = asset.nodes[i];
        node.name = safeName(source.name, "");
        node.meshIndex = pointerIndex(source.mesh, raw->meshes, raw->meshes_count);
        node.parentIndex = pointerIndex(source.parent, raw->nodes, raw->nodes_count);
        for (std::size_t child = 0; child < source.children_count; ++child)
            node.children.push_back(pointerIndex(source.children[child], raw->nodes, raw->nodes_count));
        float world[16];
        cgltf_node_transform_world(&source, world);
        std::copy(world, world + 16, node.worldTransformCm.begin());
        node.worldTransformCm[12] *= kMetresToCentimetres;
        node.worldTransformCm[13] *= kMetresToCentimetres;
        node.worldTransformCm[14] *= kMetresToCentimetres;
        if (startsWith(node.name, "SOCKET_")) {
            asset.sockets.push_back({node.name, static_cast<int>(i),
                                     {node.worldTransformCm[12], node.worldTransformCm[13], node.worldTransformCm[14]}});
        }
    }

    for (const Node& node : asset.nodes) {
        if (node.meshIndex < 0) continue;
        const Mesh& mesh = asset.meshes[static_cast<std::size_t>(node.meshIndex)];
        for (const Primitive& primitive : mesh.primitives)
            for (const Vertex& vertex : primitive.vertices)
                addBounds(asset.worldBounds, transformPoint(node.worldTransformCm, vertex.positionCm));
    }

    result.ok = true;
    result.asset = std::move(asset);
    return result;
}

PackageImportResult importAssetPackage(const std::string& gltfPath,
                                       const std::string& manifestPath,
                                       const std::string& pbrParametersPath)
{
    PackageImportResult result;
    ImportResult imported = importAsset(gltfPath);
    if (!imported.ok) { result.error = imported.error; return result; }
    result.package.asset = std::move(imported.asset);
    try {
        const json::Value manifest = parseJsonFile(manifestPath);
        const json::Value pbr = parseJsonFile(pbrParametersPath);
        const json::Value& budgets = manifest.at("budgets");
        result.package.declaredTriangles = static_cast<std::size_t>(budgets.at("unique_triangles").asInt());
        result.package.declaredTextures = static_cast<std::size_t>(budgets.at("textures").asInt());
        if (result.package.declaredTriangles != result.package.asset.triangleCount)
            throw std::runtime_error("manifest unique_triangles does not match imported meshes");
        if (result.package.declaredTextures != result.package.asset.textures.size())
            throw std::runtime_error("manifest textures does not match imported textures");

        const json::Value& constraints = manifest.at("constraints");
        result.package.clearcoatExtensionsRequired = constraints.at("clearcoat_extensions_required").asBool();
        result.package.metallicRoughnessFallbackPresent = constraints.at("metallic_roughness_fallback_present").asBool();
        if (result.package.clearcoatExtensionsRequired)
            throw std::runtime_error("manifest requires clearcoat; presentation assets must retain a core fallback");
        if (result.package.metallicRoughnessFallbackPresent) {
            for (const Material& material : result.package.asset.materials)
                if (material.hasClearcoat && !material.hasMetallicRoughnessFallback)
                    throw std::runtime_error("clearcoat material '" + material.name + "' lacks metallic-roughness fallback");
        }

        const json::Value::Object& textureSets = pbr.at("texture_sets").asObject();
        for (const auto& textureSet : textureSets) {
            for (const auto& channel : textureSet.second.at("files").asObject()) {
                const std::string filename = channel.second.asString();
                if (!hasTextureUri(result.package.asset, filename))
                    throw std::runtime_error("PBR texture set '" + textureSet.first +
                                             "' references image missing from glTF: '" + filename + "'");
            }
        }
        for (const auto& entry : manifest.at("pbr_material_bindings").asObject()) {
            PbrMaterialBinding binding;
            binding.materialName = entry.first;
            binding.materialIndex = findMaterial(result.package.asset, binding.materialName);
            if (binding.materialIndex < 0) throw std::runtime_error("PBR binding references missing material '" + binding.materialName + "'");
            binding.textureSet = entry.second.at("texture_set").asString();
            if (textureSets.find(binding.textureSet) == textureSets.end())
                throw std::runtime_error("PBR binding references missing texture set '" + binding.textureSet + "'");
            const json::Value::Array& repeat = entry.second.at("uv_repeat").asArray();
            if (repeat.size() != 2) throw std::runtime_error("PBR binding uv_repeat must have two values");
            binding.uvRepeat = {static_cast<float>(repeat[0].asNumber()), static_cast<float>(repeat[1].asNumber())};
            const json::Value::Array& setRepeat = textureSets.at(binding.textureSet).at("uv_repeat").asArray();
            if (setRepeat.size() != 2 ||
                std::fabs(binding.uvRepeat.x - static_cast<float>(setRepeat[0].asNumber())) > 0.0001f ||
                std::fabs(binding.uvRepeat.y - static_cast<float>(setRepeat[1].asNumber())) > 0.0001f)
                throw std::runtime_error("PBR binding uv_repeat disagrees with texture set '" + binding.textureSet + "'");
            result.package.pbrBindings.push_back(binding);
        }

        for (const auto& group : manifest.at("lod_sets").asObject()) {
            const json::Value::Object& nodes = group.second.at("nodes").asObject();
            const json::Value::Object& triangles = group.second.at("triangles").asObject();
            for (const auto& levelEntry : nodes) {
                LodRoot root;
                root.group = group.first;
                root.level = std::stoi(levelEntry.first);
                root.nodeIndex = levelEntry.second.asInt();
                if (root.nodeIndex < 0 || static_cast<std::size_t>(root.nodeIndex) >= result.package.asset.nodes.size())
                    throw std::runtime_error("LOD root node index is out of range");
                root.nodeName = result.package.asset.nodes[static_cast<std::size_t>(root.nodeIndex)].name;
                const auto expected = triangles.find(levelEntry.first);
                if (expected == triangles.end()) throw std::runtime_error("LOD root is missing triangle budget");
                root.expectedTriangles = static_cast<std::size_t>(expected->second.asInt());
                std::size_t actual = 0;
                std::unordered_set<int> active;
                if (!nodeTreeTriangles(result.package.asset, root.nodeIndex, actual, active))
                    throw std::runtime_error("LOD node tree is invalid for '" + root.nodeName + "'");
                if (actual != root.expectedTriangles)
                    throw std::runtime_error("LOD triangle count mismatch for '" + root.nodeName + "'");
                result.package.lodRoots.push_back(root);
            }
        }
    } catch (const std::exception& error) {
        result.error = std::string("asset package validation failed: ") + error.what();
        return result;
    }
    result.ok = true;
    return result;
}

} } }  // namespace billiardgl::render::gltf
