#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "render/gltf/gltf_draw_package.h"
#include "render/gltf/gltf_gpu_upload.h"
#include "render/gltf/gltf_importer.h"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace billiardgl::render::gltf;

int main()
{
    @autoreleasepool {
        try {
            const NSOpenGLPixelFormatAttribute attributes[] = {
                NSOpenGLPFAOpenGLProfile,
                static_cast<NSOpenGLPixelFormatAttribute>(NSOpenGLProfileVersion4_1Core),
                NSOpenGLPFAAccelerated,
                static_cast<NSOpenGLPixelFormatAttribute>(0)
            };
            NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
            if (format == nil) throw std::runtime_error("4.1 Core pixel format unavailable");
            NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
            if (context == nil) throw std::runtime_error("4.1 Core context creation failed");
            [context makeCurrentContext];

            const std::string root = std::string(BILLIARDGL_SOURCE_ROOT) + "/assets/modern/phase2/";
            PackageImportResult imported = importAssetPackage(
                root + "focused_practice_hall_modular_v1.gltf",
                root + "focused_practice_hall_modular_v1.asset.json",
                root + "focused_practice_hall_pbr_v1.params.json");
            if (!imported.ok) throw std::runtime_error(imported.error);
            DrawPackageResult cpu = buildCpuDrawPackage(imported.package);
            if (!cpu.ok) throw std::runtime_error(cpu.error);

            {
                GpuUploadResult gpu = uploadDrawPackage(cpu.package);
                if (!gpu.ok) throw std::runtime_error(gpu.error);
                // Current modular asset: 59 mesh primitives, 8064 unique triangles.
                // Keep exact expectations so an incomplete upload cannot pass.
                if (gpu.package.meshes.size() != 59 || gpu.package.sourceTriangleCount != 8064 ||
                    gpu.package.vertexBytes == 0 || gpu.package.indexBytes == 0)
                    throw std::runtime_error("uploaded GPU package statistics do not match Phase2 asset");
                for (const GpuMesh& mesh : gpu.package.meshes) {
                    if (!mesh.vertexArray || !mesh.vertexBuffer || !mesh.indexBuffer)
                        throw std::runtime_error("RAII GPU resource missing for mesh '" + mesh.sourceMeshName + "'");
                    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer.id());
                    GLint vertexBytes = 0;
                    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &vertexBytes);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer.id());
                    GLint indexBytes = 0;
                    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexBytes);
                    if (vertexBytes <= 0 || indexBytes <= 0)
                        throw std::runtime_error("Core context did not retain buffers for mesh '" + mesh.sourceMeshName + "'");
                }
                if (glGetError() != GL_NO_ERROR)
                    throw std::runtime_error("OpenGL error while inspecting uploaded Phase2 buffers");
            }

            std::cout << "gltf-gpu-upload-smoke: pass meshes=59 triangles=8064\n";
            [NSOpenGLContext clearCurrentContext];
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "gltf-gpu-upload-smoke: " << error.what() << '\n';
            return 1;
        }
    }
}
