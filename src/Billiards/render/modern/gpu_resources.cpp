#include "render/modern/gpu_resources.h"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <algorithm>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace billiardgl::render {

static_assert(sizeof(GpuHandle) == sizeof(GLuint), "GPU handle ABI mismatch");

void ShaderTag::destroy(GpuHandle handle) noexcept { glDeleteShader(handle); }
void ProgramTag::destroy(GpuHandle handle) noexcept { glDeleteProgram(handle); }
void BufferTag::destroy(GpuHandle handle) noexcept { glDeleteBuffers(1, &handle); }
void VertexArrayTag::destroy(GpuHandle handle) noexcept { glDeleteVertexArrays(1, &handle); }
void Texture2DTag::destroy(GpuHandle handle) noexcept { glDeleteTextures(1, &handle); }
void SamplerTag::destroy(GpuHandle handle) noexcept { glDeleteSamplers(1, &handle); }
void FramebufferTag::destroy(GpuHandle handle) noexcept { glDeleteFramebuffers(1, &handle); }

namespace {

GLenum glStage(ShaderStage stage)
{
    switch (stage) {
        case ShaderStage::Vertex: return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
    }
    throw std::invalid_argument("Unsupported shader stage");
}

std::string shaderLog(GLuint shader)
{
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 0) return {};
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    glGetShaderInfoLog(shader, length, &written, log.data());
    log.resize(static_cast<std::size_t>(std::max<GLsizei>(written, 0)));
    return log;
}

std::string programLog(GLuint program)
{
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 0) return {};
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    glGetProgramInfoLog(program, length, &written, log.data());
    log.resize(static_cast<std::size_t>(std::max<GLsizei>(written, 0)));
    return log;
}

template <typename Resource, typename Generate>
Resource createObject(Generate generate, const char* resourceName)
{
    GLuint handle = 0;
    generate(&handle);
    if (handle == 0) throw std::runtime_error(std::string("OpenGL failed to create ") + resourceName);
    return Resource(handle);
}

}  // namespace

Shader compileShader(ShaderStage stage,
                     const std::string& source,
                     const std::string& resourceName)
{
    Shader shader(glCreateShader(glStage(stage)));
    if (!shader) throw std::runtime_error("OpenGL failed to create shader: " + resourceName);
    const char* sourcePointer = source.c_str();
    const GLint sourceLength = static_cast<GLint>(source.size());
    glShaderSource(shader.id(), 1, &sourcePointer, &sourceLength);
    glCompileShader(shader.id());
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader.id(), GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        throw std::runtime_error(formatShaderCompilationError(
            stage, resourceName, shaderLog(shader.id()), source));
    }
    return shader;
}

Program linkProgram(const Shader& vertex,
                    const Shader& fragment,
                    const std::string& resourceName)
{
    Program program(glCreateProgram());
    if (!program) throw std::runtime_error("OpenGL failed to create program: " + resourceName);
    glAttachShader(program.id(), vertex.id());
    glAttachShader(program.id(), fragment.id());
    glLinkProgram(program.id());
    GLint linked = GL_FALSE;
    glGetProgramiv(program.id(), GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        throw std::runtime_error(formatProgramLinkError(resourceName, programLog(program.id())));
    }
    return program;
}

Buffer createBuffer()
{
    return createObject<Buffer>([](GLuint* handle) { glGenBuffers(1, handle); }, "buffer");
}

VertexArray createVertexArray()
{
    return createObject<VertexArray>([](GLuint* handle) { glGenVertexArrays(1, handle); }, "vertex array");
}

Texture2D createTexture2D()
{
    return createObject<Texture2D>([](GLuint* handle) { glGenTextures(1, handle); }, "texture");
}

Sampler createSampler()
{
    return createObject<Sampler>([](GLuint* handle) { glGenSamplers(1, handle); }, "sampler");
}

Framebuffer createFramebuffer()
{
    return createObject<Framebuffer>([](GLuint* handle) { glGenFramebuffers(1, handle); }, "framebuffer");
}

}  // namespace billiardgl::render
