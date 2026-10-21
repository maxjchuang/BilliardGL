#pragma once

#include "render/modern/shader_diagnostics.h"

#include <cstdint>
#include <utility>

namespace billiardgl::render {

using GpuHandle = std::uint32_t;

struct ShaderTag { static void destroy(GpuHandle handle) noexcept; };
struct ProgramTag { static void destroy(GpuHandle handle) noexcept; };
struct BufferTag { static void destroy(GpuHandle handle) noexcept; };
struct VertexArrayTag { static void destroy(GpuHandle handle) noexcept; };
struct Texture2DTag { static void destroy(GpuHandle handle) noexcept; };
struct SamplerTag { static void destroy(GpuHandle handle) noexcept; };
struct FramebufferTag { static void destroy(GpuHandle handle) noexcept; };

template <typename Tag>
class UniqueGpuResource {
public:
    UniqueGpuResource() noexcept = default;
    explicit UniqueGpuResource(GpuHandle handle) noexcept : handle_(handle) {}
    ~UniqueGpuResource() { reset(); }

    UniqueGpuResource(const UniqueGpuResource&) = delete;
    UniqueGpuResource& operator=(const UniqueGpuResource&) = delete;

    UniqueGpuResource(UniqueGpuResource&& other) noexcept
        : handle_(other.release()) {}

    UniqueGpuResource& operator=(UniqueGpuResource&& other) noexcept
    {
        if (this != &other) reset(other.release());
        return *this;
    }

    GpuHandle id() const noexcept { return handle_; }
    explicit operator bool() const noexcept { return handle_ != 0; }

    GpuHandle release() noexcept
    {
        return std::exchange(handle_, 0);
    }

    void reset(GpuHandle replacement = 0) noexcept
    {
        if (handle_ == replacement) return;
        if (handle_ != 0) Tag::destroy(handle_);
        handle_ = replacement;
    }

private:
    GpuHandle handle_ = 0;
};

using Shader = UniqueGpuResource<ShaderTag>;
using Program = UniqueGpuResource<ProgramTag>;
using Buffer = UniqueGpuResource<BufferTag>;
using VertexArray = UniqueGpuResource<VertexArrayTag>;
using Texture2D = UniqueGpuResource<Texture2DTag>;
using Sampler = UniqueGpuResource<SamplerTag>;
using Framebuffer = UniqueGpuResource<FramebufferTag>;

Shader compileShader(ShaderStage stage,
                     const std::string& source,
                     const std::string& resourceName);
Program linkProgram(const Shader& vertex,
                    const Shader& fragment,
                    const std::string& resourceName);
Buffer createBuffer();
VertexArray createVertexArray();
Texture2D createTexture2D();
Sampler createSampler();
Framebuffer createFramebuffer();

}  // namespace billiardgl::render
