#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace billiardgl::render {

using Float2 = std::array<float, 2>;
using Float3 = std::array<float, 3>;
using Float4 = std::array<float, 4>;
using Matrix4 = std::array<float, 16>;

enum class MeshAttribute : std::uint8_t {
    Position = 0,
    Normal = 1,
    Tangent = 2,
    TexCoord0 = 3,
    Color0 = 4,
};

struct MeshVertex {
    Float3 position{};
    Float3 normal{};
    Float4 tangent{};
    Float2 texCoord0{};
    Float4 color{{1.0f, 1.0f, 1.0f, 1.0f}};
};

// These structures define ownership and update frequency. They are CPU-side
// contracts, not a promise that every platform will upload them as one UBO.
struct PerFrameData {
    float elapsedSeconds = 0.0f;
    float deltaSeconds = 0.0f;
    std::uint64_t frameIndex = 0;
};

struct PerViewData {
    Matrix4 view{};
    Matrix4 projection{};
    Matrix4 viewProjection{};
    Float4 cameraWorldPosition{};
    Float4 viewportPixels{};
};

struct PerObjectData {
    Matrix4 model{};
    Matrix4 normalTransform{};
    std::uint32_t objectId = 0;
    std::uint32_t materialIndex = 0;
};

struct PerMaterialData {
    Float4 baseColorFactor{{1.0f, 1.0f, 1.0f, 1.0f}};
    Float3 emissiveFactor{};
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float clearcoatFactor = 0.0f;
    float clearcoatRoughness = 0.0f;
    std::uint32_t baseColorTexture = 0;
    std::uint32_t normalTexture = 0;
    std::uint32_t metallicRoughnessTexture = 0;
    std::uint32_t identityDecalTexture = 0;
};

static_assert(std::is_standard_layout<MeshVertex>::value,
              "MeshVertex must remain uploadable as interleaved attributes");
static_assert(offsetof(MeshVertex, position) == 0,
              "Position is the first interleaved mesh attribute");

}  // namespace billiardgl::render
