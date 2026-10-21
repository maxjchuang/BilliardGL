#pragma once

#include "render/gltf/gltf_draw_package.h"
#include "render/modern/gpu_resources.h"

#include <cstddef>
#include <string>
#include <vector>

namespace billiardgl { namespace render { namespace gltf {

struct GpuMesh {
    std::string sourceMeshName;
    VertexArray vertexArray;
    Buffer vertexBuffer;
    Buffer indexBuffer;
    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;
};

struct GpuDrawPackage {
    std::vector<GpuMesh> meshes;
    std::size_t vertexBytes = 0;
    std::size_t indexBytes = 0;
    std::size_t sourceTriangleCount = 0;
};

struct GpuUploadResult {
    bool ok = false;
    GpuDrawPackage package;
    std::string error;
};

// Requires a current OpenGL 3.3+ Core Profile context.
GpuUploadResult uploadDrawPackage(const CpuDrawPackage& source);

} } }  // namespace billiardgl::render::gltf
