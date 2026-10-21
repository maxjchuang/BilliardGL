#define GLEW_STATIC
#include <GL/glew.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "screenshot.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>

namespace billiardgl {
namespace {

struct FramebufferApi {
    bool core = false;

    bool available() const
    {
        return core || GLEW_EXT_framebuffer_object;
    }

    void generateFramebuffer(GLuint* value) const
    {
        if (core) glGenFramebuffers(1, value);
        else glGenFramebuffersEXT(1, value);
    }

    void bindFramebuffer(GLuint value) const
    {
        if (core) glBindFramebuffer(GL_FRAMEBUFFER, value);
        else glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, value);
    }

    void deleteFramebuffer(GLuint value) const
    {
        if (core) glDeleteFramebuffers(1, &value);
        else glDeleteFramebuffersEXT(1, &value);
    }

    void generateRenderbuffer(GLuint* value) const
    {
        if (core) glGenRenderbuffers(1, value);
        else glGenRenderbuffersEXT(1, value);
    }

    void bindRenderbuffer(GLuint value) const
    {
        if (core) glBindRenderbuffer(GL_RENDERBUFFER, value);
        else glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, value);
    }

    void allocateDepth(GLsizei width, GLsizei height) const
    {
        if (core) {
            glRenderbufferStorage(
                GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        } else {
            glRenderbufferStorageEXT(
                GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height);
        }
    }

    void attachColorTexture(GLuint texture) const
    {
        if (core) {
            glFramebufferTexture2D(
                GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        } else {
            glFramebufferTexture2DEXT(
                GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                GL_TEXTURE_2D, texture, 0);
        }
    }

    void attachDepthRenderbuffer(GLuint renderbuffer) const
    {
        if (core) {
            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
        } else {
            glFramebufferRenderbufferEXT(
                GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                GL_RENDERBUFFER_EXT, renderbuffer);
        }
    }

    GLenum status() const
    {
        return core
            ? glCheckFramebufferStatus(GL_FRAMEBUFFER)
            : glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    }

    bool complete(GLenum value) const
    {
        return value == (core
            ? static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE)
            : static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE_EXT));
    }

    void deleteRenderbuffer(GLuint value) const
    {
        if (core) glDeleteRenderbuffers(1, &value);
        else glDeleteRenderbuffersEXT(1, &value);
    }
};

bool readFramebufferToPpm(
    const std::string& path, int width, int height, GLenum readBuffer)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height) * 3u;
    std::vector<unsigned char> pixels(pixelCount);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    while (glGetError() != GL_NO_ERROR) {}
    glReadBuffer(readBuffer);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) {
        return false;
    }

    std::vector<unsigned char> flipped(pixelCount);
    const std::size_t rowSize = static_cast<std::size_t>(width) * 3u;
    for (int y = 0; y < height; ++y) {
        const std::size_t sourceOffset = static_cast<std::size_t>(y) * rowSize;
        const std::size_t destinationOffset =
            static_cast<std::size_t>(height - 1 - y) * rowSize;
        std::copy(
            pixels.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
            pixels.begin() + static_cast<std::ptrdiff_t>(sourceOffset + rowSize),
            flipped.begin() + static_cast<std::ptrdiff_t>(destinationOffset));
    }

    return hasVisiblePixels(flipped) && writePpm(path, width, height, flipped);
}

}  // namespace

bool saveFramebufferToPpm(const std::string& path, int width, int height)
{
    return readFramebufferToPpm(path, width, height, GL_BACK);
}

bool captureOffscreenToPpm(
    const std::string& path, int width, int height,
    const std::function<void()>& render)
{
    if (width <= 0 || height <= 0 || !render) {
        return false;
    }

    FramebufferApi api;
    api.core = GLEW_VERSION_3_0 || GLEW_ARB_framebuffer_object;
    if (!api.available()) {
        std::fprintf(stderr, "Offscreen screenshot requires framebuffer object support\n");
        return false;
    }

    GLint maximumTextureSize = 0;
    GLint maximumRenderbufferSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maximumRenderbufferSize);
    if (width > maximumTextureSize || height > maximumTextureSize ||
        width > maximumRenderbufferSize || height > maximumRenderbufferSize) {
        std::fprintf(stderr,
            "Offscreen screenshot %dx%d exceeds GL limits texture=%d renderbuffer=%d\n",
            width, height, maximumTextureSize, maximumRenderbufferSize);
        return false;
    }

    GLint previousFramebuffer = 0;
    GLint previousRenderbuffer = 0;
    GLint previousTexture = 0;
    GLint previousReadBuffer = 0;
    GLint previousDrawBuffer = 0;
    GLint previousViewport[4] = {};
    glGetIntegerv(
        api.core ? GL_FRAMEBUFFER_BINDING : GL_FRAMEBUFFER_BINDING_EXT,
        &previousFramebuffer);
    glGetIntegerv(
        api.core ? GL_RENDERBUFFER_BINDING : GL_RENDERBUFFER_BINDING_EXT,
        &previousRenderbuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_DRAW_BUFFER, &previousDrawBuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    GLuint depthRenderbuffer = 0;
    api.generateFramebuffer(&framebuffer);
    glGenTextures(1, &colorTexture);
    api.generateRenderbuffer(&depthRenderbuffer);
    if (framebuffer == 0 || colorTexture == 0 || depthRenderbuffer == 0) {
        std::fprintf(stderr, "Failed to allocate offscreen screenshot objects\n");
        if (depthRenderbuffer != 0) api.deleteRenderbuffer(depthRenderbuffer);
        if (colorTexture != 0) glDeleteTextures(1, &colorTexture);
        if (framebuffer != 0) api.deleteFramebuffer(framebuffer);
        return false;
    }

    api.bindFramebuffer(framebuffer);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    api.attachColorTexture(colorTexture);

    api.bindRenderbuffer(depthRenderbuffer);
    api.allocateDepth(width, height);
    api.attachDepthRenderbuffer(depthRenderbuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    const GLenum framebufferStatus = api.status();
    bool saved = false;
    if (!api.complete(framebufferStatus)) {
        std::fprintf(stderr,
            "Offscreen screenshot framebuffer is incomplete: 0x%04x\n",
            static_cast<unsigned int>(framebufferStatus));
    } else {
        glViewport(0, 0, width, height);
        render();
        glFinish();
        saved = readFramebufferToPpm(
            path, width, height, GL_COLOR_ATTACHMENT0);
    }

    api.bindFramebuffer(static_cast<GLuint>(previousFramebuffer));
    api.bindRenderbuffer(static_cast<GLuint>(previousRenderbuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    glDrawBuffer(static_cast<GLenum>(previousDrawBuffer));
    glViewport(
        previousViewport[0], previousViewport[1],
        previousViewport[2], previousViewport[3]);
    api.deleteRenderbuffer(depthRenderbuffer);
    glDeleteTextures(1, &colorTexture);
    api.deleteFramebuffer(framebuffer);
    return saved;
}

}  // namespace billiardgl
