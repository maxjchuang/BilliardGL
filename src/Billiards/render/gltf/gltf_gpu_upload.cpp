#include "render/gltf/gltf_gpu_upload.h"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace billiardgl { namespace render { namespace gltf {
namespace {

std::string glErrorName(GLenum error)
{
    switch (error) {
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default: {
        std::ostringstream text;
        text << "OpenGL error 0x" << std::hex << error;
        return text.str();
    }
    }
}

void requireNoGlError(const std::string& mesh, const char* accessor)
{
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        throw std::runtime_error("mesh '" + mesh + "' accessor " + accessor +
                                 " upload failed: " + glErrorName(error));
}

void enableAttribute(GLuint location, GLint components, std::size_t offset)
{
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE,
                          static_cast<GLsizei>(sizeof(MeshVertex)),
                          reinterpret_cast<const void*>(offset));
}

}  // namespace

GpuUploadResult uploadDrawPackage(const CpuDrawPackage& source)
{
    GpuUploadResult result;
    if (glGetString(GL_VERSION) == nullptr) {
        result.error = "glTF draw package upload requires a current OpenGL Core context";
        return result;
    }
    while (glGetError() != GL_NO_ERROR) {}
    result.package.sourceTriangleCount = source.sourceTriangleCount;
    result.package.meshes.reserve(source.meshes.size());
    try {
        for (const CpuMesh& sourceMesh : source.meshes) {
            const std::size_t vertexBytes = sourceMesh.vertices.size() * sizeof(MeshVertex);
            const std::size_t indexBytes = sourceMesh.indices.size() * sizeof(std::uint32_t);
            if (vertexBytes > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()))
                throw std::runtime_error("mesh '" + sourceMesh.sourceMeshName + "' accessor vertex data exceeds GLsizeiptr");
            if (indexBytes > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()))
                throw std::runtime_error("mesh '" + sourceMesh.sourceMeshName + "' accessor index data exceeds GLsizeiptr");
            if (sourceMesh.vertices.empty() || sourceMesh.indices.empty())
                throw std::runtime_error("mesh '" + sourceMesh.sourceMeshName + "' accessor data is empty");

            GpuMesh mesh;
            mesh.sourceMeshName = sourceMesh.sourceMeshName;
            mesh.vertexCount = sourceMesh.vertices.size();
            mesh.indexCount = sourceMesh.indices.size();
            try {
                mesh.vertexArray = createVertexArray();
                mesh.vertexBuffer = createBuffer();
                mesh.indexBuffer = createBuffer();
            } catch (const std::exception& error) {
                throw std::runtime_error("mesh '" + sourceMesh.sourceMeshName +
                                         "' accessor resource creation failed: " + error.what());
            }
            glBindVertexArray(mesh.vertexArray.id());
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer.id());
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexBytes),
                         sourceMesh.vertices.data(), GL_STATIC_DRAW);
            requireNoGlError(sourceMesh.sourceMeshName, "POSITION/NORMAL/TANGENT/UV0/COLOR0");
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer.id());
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexBytes),
                         sourceMesh.indices.data(), GL_STATIC_DRAW);
            requireNoGlError(sourceMesh.sourceMeshName, "indices");
            enableAttribute(static_cast<GLuint>(MeshAttribute::Position), 3, offsetof(MeshVertex, position));
            enableAttribute(static_cast<GLuint>(MeshAttribute::Normal), 3, offsetof(MeshVertex, normal));
            enableAttribute(static_cast<GLuint>(MeshAttribute::Tangent), 4, offsetof(MeshVertex, tangent));
            enableAttribute(static_cast<GLuint>(MeshAttribute::TexCoord0), 2, offsetof(MeshVertex, texCoord0));
            enableAttribute(static_cast<GLuint>(MeshAttribute::Color0), 4, offsetof(MeshVertex, color));
            requireNoGlError(sourceMesh.sourceMeshName, "VAO layout");
            result.package.vertexBytes += vertexBytes;
            result.package.indexBytes += indexBytes;
            result.package.meshes.push_back(std::move(mesh));
        }
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    } catch (const std::exception& error) {
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        result.error = error.what();
        return result;
    }
    result.ok = true;
    return result;
}

} } }  // namespace billiardgl::render::gltf
