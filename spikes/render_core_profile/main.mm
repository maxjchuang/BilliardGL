#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr GLsizei kWidth = 640;
constexpr GLsizei kHeight = 360;

const char* kVertexShader = R"GLSL(#version 410 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
uniform vec4 uRect;
out vec2 vUv;
void main() {
    vec2 unitPosition = aPosition * 0.5 + 0.5;
    gl_Position = vec4(mix(uRect.xy, uRect.zw, unitPosition), 0.0, 1.0);
    vUv = aUv;
}
)GLSL";

const char* kFragmentShader = R"GLSL(#version 410 core
in vec2 vUv;
layout(location = 0) out vec4 outColor;
uniform sampler2D uTexture;
uniform int uMode;

void main() {
    if (uMode == 0) {
        vec3 texel = texture(uTexture, vUv * vec2(5.0, 3.0)).rgb;
        float vignette = 0.86 + 0.14 * (1.0 - length(vUv - 0.5));
        outColor = vec4(texel * vignette, 1.0);
        return;
    }

    if (uMode == 1) {
        vec2 p = vUv * 2.0 - 1.0;
        float radiusSquared = dot(p, p);
        if (radiusSquared > 1.0) discard;
        vec3 normal = normalize(vec3(p, sqrt(1.0 - radiusSquared)));
        vec3 lightDirection = normalize(vec3(-0.55, 0.75, 1.0));
        vec3 viewDirection = vec3(0.0, 0.0, 1.0);
        vec3 halfVector = normalize(lightDirection + viewDirection);
        float diffuse = max(dot(normal, lightDirection), 0.0);
        float clearcoat = pow(max(dot(normal, halfVector), 0.0), 96.0);
        float rim = pow(1.0 - max(normal.z, 0.0), 3.0);
        vec3 base = vec3(0.11, 0.30, 0.92);
        outColor = vec4(base * (0.18 + diffuse * 0.82)
                      + vec3(clearcoat * 0.85)
                      + vec3(0.08, 0.14, 0.26) * rim, 1.0);
        return;
    }

    float coverage = texture(uTexture, vUv).r;
    outColor = vec4(vec3(0.93, 0.96, 1.0), coverage);
}
)GLSL";

GLuint compileShader(GLenum stage, const char* source) {
    const GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("GLSL compile failed: " + log);
}

GLuint makeProgram() {
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) return program;

    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    glDeleteProgram(program);
    throw std::runtime_error("GLSL link failed: " + log);
}

GLuint makeCheckerTexture() {
    constexpr int size = 8;
    std::array<std::uint8_t, size * size * 4> pixels{};
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool light = ((x / 2) + (y / 2)) % 2 == 0;
            const std::array<std::uint8_t, 4> color = light
                ? std::array<std::uint8_t, 4>{34, 105, 73, 255}
                : std::array<std::uint8_t, 4>{15, 57, 43, 255};
            const std::size_t offset = static_cast<std::size_t>((y * size + x) * 4);
            std::copy(color.begin(), color.end(), pixels.begin() + offset);
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

std::array<std::uint8_t, 7> glyph(char value) {
    switch (value) {
        case 'C': return {14, 17, 16, 16, 16, 17, 14};
        case 'O': return {14, 17, 17, 17, 17, 17, 14};
        case 'R': return {30, 17, 17, 30, 20, 18, 17};
        case 'E': return {31, 16, 16, 30, 16, 16, 31};
        case '4': return {18, 18, 18, 31, 2, 2, 2};
        case '1': return {4, 12, 4, 4, 4, 4, 14};
        default: return {};
    }
}

GLuint makeTextTexture(int& width, int& height) {
    const std::string text = "CORE 41";
    width = static_cast<int>(text.size()) * 6;
    height = 7;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height), 0);
    for (std::size_t character = 0; character < text.size(); ++character) {
        const auto rows = glyph(text[character]);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < 5; ++x) {
                if ((rows[static_cast<std::size_t>(y)] & (1u << (4 - x))) != 0) {
                    const int destinationY = height - 1 - y;
                    pixels[static_cast<std::size_t>(destinationY * width
                        + static_cast<int>(character) * 6 + x)] = 255;
                }
            }
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED,
                 GL_UNSIGNED_BYTE, pixels.data());
    return texture;
}

void writePpm(const std::filesystem::path& outputPath) {
    if (!outputPath.parent_path().empty()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kWidth * kHeight * 3));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) throw std::runtime_error("Cannot open screenshot: " + outputPath.string());
    output << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    const std::size_t rowBytes = static_cast<std::size_t>(kWidth * 3);
    for (int y = kHeight - 1; y >= 0; --y) {
        output.write(reinterpret_cast<const char*>(pixels.data()
            + static_cast<std::size_t>(y) * rowBytes), static_cast<std::streamsize>(rowBytes));
    }
}

std::filesystem::path screenshotPath(int argc, const char* const* argv) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::string(argv[index]) == "--screenshot") return argv[index + 1];
    }
    return "/tmp/billiardgl-render-core-profile-spike.ppm";
}

int run(int argc, const char* const* argv) {
    const NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFAOpenGLProfile,
        static_cast<NSOpenGLPixelFormatAttribute>(NSOpenGLProfileVersion4_1Core),
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        static_cast<NSOpenGLPixelFormatAttribute>(0)
    };

    NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    if (pixelFormat == nil) throw std::runtime_error("macOS rejected the OpenGL 4.1 Core pixel format");
    NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
    if (context == nil) throw std::runtime_error("macOS could not create an OpenGL 4.1 Core context");
    [context makeCurrentContext];

    GLint profile = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* shadingLanguage = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    if (version == nullptr || (profile & GL_CONTEXT_CORE_PROFILE_BIT) == 0) {
        throw std::runtime_error("Created context is not an OpenGL Core Profile context");
    }
    std::cout << "OpenGL=" << version << " GLSL=" << shadingLanguage
              << " profile=core framebuffer=" << kWidth << 'x' << kHeight << '\n';

    GLuint color = 0;
    GLuint depth = 0;
    GLuint framebuffer = 0;
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kWidth, kHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kWidth, kHeight);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Spike framebuffer is incomplete");
    }

    const GLuint program = makeProgram();
    constexpr std::array<float, 24> quad = {
        -1.f, -1.f, 0.f, 0.f,  1.f, -1.f, 1.f, 0.f,  1.f, 1.f, 1.f, 1.f,
        -1.f, -1.f, 0.f, 0.f,  1.f,  1.f, 1.f, 1.f, -1.f, 1.f, 0.f, 1.f
    };
    GLuint vao = 0;
    GLuint buffer = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &buffer);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<const void*>(2 * sizeof(float)));

    const GLuint checker = makeCheckerTexture();
    int textWidth = 0;
    int textHeight = 0;
    const GLuint text = makeTextTexture(textWidth, textHeight);
    glViewport(0, 0, kWidth, kHeight);
    glClearColor(0.018f, 0.025f, 0.042f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTexture"), 0);

    const GLint rectLocation = glGetUniformLocation(program, "uRect");
    const GLint modeLocation = glGetUniformLocation(program, "uMode");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, checker);
    glUniform4f(rectLocation, -0.92f, -0.76f, 0.92f, 0.18f);
    glUniform1i(modeLocation, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glUniform4f(rectLocation, -0.42f, -0.10f, 0.30f, 0.94f);
    glUniform1i(modeLocation, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, text);
    glUniform4f(rectLocation, 0.36f, 0.48f, 0.92f, 0.74f);
    glUniform1i(modeLocation, 2);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) throw std::runtime_error("OpenGL error after draw: " + std::to_string(error));
    const auto outputPath = screenshotPath(argc, argv);
    writePpm(outputPath);

    glDeleteTextures(1, &text);
    glDeleteTextures(1, &checker);
    glDeleteBuffers(1, &buffer);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteRenderbuffers(1, &depth);
    glDeleteTextures(1, &color);
    [NSOpenGLContext clearCurrentContext];
    std::cout << "screenshot=" << outputPath << " components=shader-ball,texture-plane,bitmap-font\n";
    return 0;
}

}  // namespace

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        try {
            return run(argc, argv);
        } catch (const std::exception& error) {
            std::cerr << "render-core-profile-spike: " << error.what() << '\n';
            return 1;
        }
    }
}
