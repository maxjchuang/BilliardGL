#include "render/modern/pbr_texture_resources.h"
#include "render/modern/environment_lighting.h"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "dependencies/include/stb_image.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace billiardgl::render {
namespace {

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

int mipCount(int width, int height)
{
    int levels = 1;
    while (width > 1 || height > 1) {
        width = std::max(width / 2, 1);
        height = std::max(height / 2, 1);
        ++levels;
    }
    return levels;
}

bool hasExtension(const char* wanted)
{
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint index = 0; index < count; ++index) {
        const auto* extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(index)));
        if (extension != nullptr && std::strcmp(extension, wanted) == 0) return true;
    }
    return false;
}

int specularKeyLobeDiagnosticMode()
{
    const char* value =
        std::getenv("BILLIARDGL_SPECULAR_KEY_LOBE_EXCLUSION_DIAGNOSTIC");
    if (value == nullptr || value[0] == '\0' || std::strcmp(value, "0") == 0)
        return 0;
    if (std::strcmp(value, "1") == 0) return 1;
    if (std::strcmp(value, "2") == 0) return 2;
    throw std::runtime_error(
        "BILLIARDGL_SPECULAR_KEY_LOBE_EXCLUSION_DIAGNOSTIC must be 0, 1 or 2");
}

UploadedTexture upload(const unsigned char* pixels, int width, int height, TextureEncoding encoding)
{
    UploadedTexture result;
    result.texture = createTexture2D();
    result.encoding = encoding;
    result.width = width;
    result.height = height;
    result.mipLevels = mipCount(width, height);
    result.gpuBytes = rgba8FullMipBytes(width, height);
    glBindTexture(GL_TEXTURE_2D, result.texture.id());
    const GLint internal = encoding == TextureEncoding::SrgbRgba8 ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, result.mipLevels - 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return result;
}

UploadedTexture loadTexture(const TextureReference& reference, std::string& warning)
{
    int width = 0, height = 0, sourceChannels = 0;
    unsigned char* pixels = stbi_load(reference.path.c_str(), &width, &height, &sourceChannels, 4);
    if (pixels == nullptr) {
        warning = "texture unavailable: " + reference.path + ": "
            + (stbi_failure_reason() ? stbi_failure_reason() : "unknown stb_image error");
        return {};
    }
    if (width != reference.expectedWidth || height != reference.expectedHeight) {
        warning = "texture dimensions differ from manifest: " + reference.path;
        stbi_image_free(pixels);
        return {};
    }
    auto texture = upload(pixels, width, height, reference.encoding);
    stbi_image_free(pixels);
    return texture;
}

UploadedTexture checker(TextureEncoding encoding)
{
    constexpr int size = 8;
    std::array<unsigned char, size * size * 4> pixels{};
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const bool magenta = ((x / 2) + (y / 2)) % 2 == 0;
        const std::size_t p = static_cast<std::size_t>((y * size + x) * 4);
        pixels[p] = magenta ? 255 : 12;
        pixels[p + 1] = magenta ? 0 : 12;
        pixels[p + 2] = magenta ? 255 : 12;
        pixels[p + 3] = 255;
    }
    return upload(pixels.data(), size, size, encoding);
}

UploadedTexture flatNormal()
{
    const std::array<unsigned char, 4> pixel{{128, 128, 255, 255}};
    return upload(pixel.data(), 1, 1, TextureEncoding::LinearRgba8);
}

UploadedTexture neutralOrm(float roughness, float metallic)
{
    const std::array<unsigned char, 4> pixel{{255,
        static_cast<unsigned char>(std::clamp(roughness, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(metallic, 0.0f, 1.0f) * 255.0f), 255}};
    return upload(pixel.data(), 1, 1, TextureEncoding::LinearRgba8);
}

Texture2D proceduralCube(int resolution, int mipLevels,
                         const LightingRigDefinition& lighting,
                         EnvironmentCubeRole role,
                         int legacySpecularKeyLobeDiagnosticMode,
                         std::size_t& byteEstimate)
{
    Texture2D cube = createTexture2D();
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube.id());
    for (int level = 0; level < mipLevels; ++level) {
        const int size = std::max(1, resolution >> level);
        // This parameter controls only scene/specular prefiltering.  The v2
        // diffuse role is an analytic Lambertian E/pi convolution and ignores
        // it; a one-mip texture must not be mistaken for roughness=1 filtering.
        const float prefilterRoughness = mipLevels > 1
            ? static_cast<float>(level) / static_cast<float>(mipLevels - 1)
            : 1.0f;
        for (int face = 0; face < 6; ++face) {
            std::vector<float> pixels(static_cast<std::size_t>(size * size * 3));
            for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
                const float dx=(static_cast<float>(x)+0.5f)/static_cast<float>(size)-0.5f;
                const float dy=(static_cast<float>(y)+0.5f)/static_cast<float>(size)-0.5f;
                const auto radiance = environmentCubeRadiance(
                    role,
                    environmentCubeDirection(face, dx * 2.0f, dy * 2.0f),
                    prefilterRoughness, lighting,
                    legacySpecularKeyLobeDiagnosticMode);
                const std::size_t p=static_cast<std::size_t>((y*size+x)*3);
                for(int channel=0;channel<3;++channel){
                    pixels[p+static_cast<std::size_t>(channel)]=radiance[static_cast<std::size_t>(channel)];
                }
            }
            glTexImage2D(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face),level,GL_RGB16F,
                         size,size,0,GL_RGB,GL_FLOAT,pixels.data());
            byteEstimate+=static_cast<std::size_t>(size*size*6); // RGB16F
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,mipLevels>1?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAX_LEVEL,mipLevels-1);
    return cube;
}

}  // namespace

void PbrTextureResources::load(const PbrPackageDescription& package, int environmentResolution)
{
    reset();
    const auto validation = validatePbrPackage(package);
    if (!validation.contractValid) {
        std::string message = "PBR package contract invalid";
        for (const auto& error : validation.errors) message += "; " + error;
        throw std::runtime_error(message);
    }
    report_.budgetGpuBytes = package.expectedGpuBytes;
    sampler_ = createSampler();
    glSamplerParameteri(sampler_.id(), GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_.id(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(sampler_.id(), GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(sampler_.id(), GL_TEXTURE_WRAP_T, GL_REPEAT);
    report_.anisotropySupported = hasExtension("GL_EXT_texture_filter_anisotropic");
    if (report_.anisotropySupported) {
        GLfloat maximum = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
        report_.anisotropy = std::min(maximum, 8.0f);
        glSamplerParameterf(sampler_.id(), GL_TEXTURE_MAX_ANISOTROPY_EXT, report_.anisotropy);
    }

    for (const auto& definition : package.materials) {
        UploadedPbrMaterial material;
        material.uvRepeat = definition.uvRepeat;
        material.roughnessFallback = definition.roughnessFallback;
        material.metallicFallback = definition.metallicFallback;
        material.clearcoatFallback = definition.clearcoatFallback;
        std::string baseWarning, normalWarning, ormWarning;
        material.baseColor = loadTexture(definition.baseColor, baseWarning);
        material.normal = loadTexture(definition.normal, normalWarning);
        material.orm = loadTexture(definition.orm, ormWarning);
        if (!material.baseColor.texture || !material.normal.texture || !material.orm.texture) {
            material.errorFallback = true;
            material.baseColor = checker(TextureEncoding::SrgbRgba8);
            material.normal = flatNormal();
            material.orm = neutralOrm(definition.roughnessFallback, definition.metallicFallback);
            if (!baseWarning.empty()) report_.warnings.push_back(baseWarning);
            if (!normalWarning.empty()) report_.warnings.push_back(normalWarning);
            if (!ormWarning.empty()) report_.warnings.push_back(ormWarning);
            report_.warnings.push_back("using explicit checker/error material: " + definition.name);
        }
        report_.uploadedGpuBytes += material.baseColor.gpuBytes + material.normal.gpuBytes + material.orm.gpuBytes;
        report_.uploadedTextureCount += 3;
        materials_.emplace(definition.name, std::move(material));
    }

    std::string atlasWarning;
    ballAtlas_ = loadTexture(package.ballAtlas, atlasWarning);
    if (!ballAtlas_.texture) {
        ballAtlas_ = checker(TextureEncoding::SrgbRgba8);
        report_.warnings.push_back(atlasWarning);
        report_.warnings.push_back("using explicit checker/error ball atlas");
    }
    report_.uploadedGpuBytes += ballAtlas_.gpuBytes;
    ++report_.uploadedTextureCount;
    report_.loadedPackage = true;
    const int environmentSize=std::max(16,environmentResolution);
    const int irradianceSize=environmentSize<=64?16:32;
    const int specularLevels=mipCount(environmentSize,environmentSize);
    const int keyLobeDiagnostic = specularKeyLobeDiagnosticMode();
    environmentMap_ = proceduralCube(
        environmentSize, 1, package.lighting,
        EnvironmentCubeRole::SceneRadiance, keyLobeDiagnostic,
        report_.environmentGpuBytes);
    irradianceMap_ = proceduralCube(
        irradianceSize, 1, package.lighting,
        EnvironmentCubeRole::DiffuseIrradiance, keyLobeDiagnostic,
        report_.environmentGpuBytes);
    specularMap_ = proceduralCube(
        environmentSize, specularLevels, package.lighting,
        EnvironmentCubeRole::SpecularRadiance, keyLobeDiagnostic,
        report_.environmentGpuBytes);
    report_.environmentResolution=environmentSize;
}

void PbrTextureResources::reset() noexcept
{
    materials_.clear();
    ballAtlas_ = {};
    environmentMap_.reset();
    irradianceMap_.reset();
    specularMap_.reset();
    sampler_.reset();
    report_ = {};
}

const UploadedPbrMaterial* PbrTextureResources::material(const std::string& name) const noexcept
{
    const auto found = materials_.find(name);
    return found == materials_.end() ? nullptr : &found->second;
}

}  // namespace billiardgl::render
