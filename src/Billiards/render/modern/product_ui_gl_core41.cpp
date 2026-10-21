#include "render/modern/product_ui_gl_core41.h"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>

namespace billiardgl::render::ui {
namespace {

UiGpuResult ok() { return {true, {}}; }

void clearErrors()
{
    while (glGetError() != GL_NO_ERROR) {}
}

UiGpuResult glStatus(const char* operation)
{
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR) return ok();
    return {false, std::string("product_ui_gl_error:") + operation + ":" +
        std::to_string(static_cast<unsigned int>(error))};
}

std::string shaderLog(GLuint shader)
{
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 0) return {};
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = 0;
    glGetShaderInfoLog(shader, length, &written, log.data());
    log.resize(static_cast<std::size_t>(std::max<GLsizei>(0, written)));
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
    log.resize(static_cast<std::size_t>(std::max<GLsizei>(0, written)));
    return log;
}

UiGpuResult compile(GLenum stage, const std::string& source, GLuint& shader)
{
    shader = glCreateShader(stage);
    if (shader == 0)
        return {false, "product_ui_gl_shader_create_failed"};
    const char* pointer = source.c_str();
    const GLint length = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &pointer, &length);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return ok();
    const std::string log = shaderLog(shader);
    glDeleteShader(shader);
    shader = 0;
    return {false, "product_ui_gl_shader_compile_failed:" + log};
}

void deleteHandles(UiGpuResourceHandles& handles)
{
    if (handles.atlasTexture != 0)
        glDeleteTextures(1, &handles.atlasTexture);
    if (handles.vertexBuffer != 0)
        glDeleteBuffers(1, &handles.vertexBuffer);
    if (handles.vertexArray != 0)
        glDeleteVertexArrays(1, &handles.vertexArray);
    if (handles.program != 0) glDeleteProgram(handles.program);
    handles = {};
}

void setEnabled(GLenum capability, bool enabled)
{
    if (enabled) glEnable(capability);
    else glDisable(capability);
}

}  // namespace

UiGpuResult Core41ProductUiGlDispatch::createResources(
    const UiGpuResourceCreateInfo& info, UiGpuResourceHandles& handles)
{
    if (handles.program != 0 || handles.vertexArray != 0 ||
        handles.vertexBuffer != 0 || handles.atlasTexture != 0)
        return {false, "product_ui_gl_resources_already_exist"};
    if (info.vertexShader.empty() || info.fragmentShader.empty() ||
        info.atlasWidth <= 0 || info.atlasHeight <= 0 ||
        info.atlasPixels == nullptr || info.atlasPixels->size() !=
            static_cast<std::size_t>(info.atlasWidth * info.atlasHeight))
        return {false, "product_ui_gl_resource_info_invalid"};
    clearErrors();
    GLint previousProgram = 0;
    GLint previousVertexArray = 0;
    GLint previousArrayBuffer = 0;
    GLint previousActiveTexture = 0;
    GLint previousTextureUnit0 = 0;
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureUnit0);
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    const auto restore = [&] {
        glUseProgram(static_cast<GLuint>(previousProgram));
        glBindVertexArray(static_cast<GLuint>(previousVertexArray));
        glBindBuffer(GL_ARRAY_BUFFER,
            static_cast<GLuint>(previousArrayBuffer));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,
            static_cast<GLuint>(previousTextureUnit0));
        glActiveTexture(static_cast<GLenum>(previousActiveTexture));
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    };
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    UiGpuResult result = compile(GL_VERTEX_SHADER, info.vertexShader,
        vertexShader);
    if (!result.ok) {
        restore();
        return result;
    }
    result = compile(GL_FRAGMENT_SHADER, info.fragmentShader, fragmentShader);
    if (!result.ok) {
        glDeleteShader(vertexShader);
        restore();
        return result;
    }
    handles.program = glCreateProgram();
    if (handles.program == 0) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        restore();
        return {false, "product_ui_gl_program_create_failed"};
    }
    glAttachShader(handles.program, vertexShader);
    glAttachShader(handles.program, fragmentShader);
    glLinkProgram(handles.program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    GLint linked = GL_FALSE;
    glGetProgramiv(handles.program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        const std::string log = programLog(handles.program);
        deleteHandles(handles);
        restore();
        return {false, "product_ui_gl_program_link_failed:" + log};
    }

    glGenVertexArrays(1, &handles.vertexArray);
    glGenBuffers(1, &handles.vertexBuffer);
    glGenTextures(1, &handles.atlasTexture);
    if (!handles.complete()) {
        deleteHandles(handles);
        restore();
        return {false, "product_ui_gl_resource_create_failed"};
    }
    glBindVertexArray(handles.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, handles.vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiGpuVertex),
        reinterpret_cast<const void*>(offsetof(UiGpuVertex, ndcX)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UiGpuVertex),
        reinterpret_cast<const void*>(offsetof(UiGpuVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UiGpuVertex),
        reinterpret_cast<const void*>(offsetof(UiGpuVertex, color)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(UiGpuVertex),
        reinterpret_cast<const void*>(offsetof(UiGpuVertex, sdf)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(UiGpuVertex),
        reinterpret_cast<const void*>(offsetof(UiGpuVertex, clipLeft)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(UiGpuVertex),
        reinterpret_cast<const void*>(offsetof(UiGpuVertex, interactiveChrome)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, handles.atlasTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, info.atlasWidth,
        info.atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE,
        info.atlasPixels->data());
    glUseProgram(handles.program);
    const GLint atlasLocation = glGetUniformLocation(handles.program,
        "uSdfAtlas");
    if (atlasLocation < 0) {
        deleteHandles(handles);
        restore();
        return {false, "product_ui_gl_atlas_uniform_missing"};
    }
    glUniform1i(atlasLocation, 0);
    result = glStatus("create_resources");
    if (!result.ok) deleteHandles(handles);
    restore();
    return result;
}

void Core41ProductUiGlDispatch::destroyResources(
    UiGpuResourceHandles& handles) noexcept
{
    deleteHandles(handles);
    state_ = {};
}

void Core41ProductUiGlDispatch::abandonResources(
    UiGpuResourceHandles& handles) noexcept
{
    handles = {};
    state_ = {};
}

UiGpuResult Core41ProductUiGlDispatch::beginFrame(
    const UiGpuResourceHandles& handles, int physicalWidth,
    int physicalHeight)
{
    const UiGpuBeginReceipt receipt = beginFrameReceipt(
        handles, physicalWidth, physicalHeight);
    return {receipt.setupOk(), receipt.errorCode()};
}

UiGpuBeginReceipt Core41ProductUiGlDispatch::beginFrameReceipt(
    const UiGpuResourceHandles& handles, int physicalWidth,
    int physicalHeight)
{
    if (!handles.complete())
        return UiGpuBeginReceipt::preCaptureFailure(
            "product_ui_gl_resources_incomplete");
    if (physicalWidth <= 0 || physicalHeight <= 0)
        return UiGpuBeginReceipt::preCaptureFailure(
            "product_ui_gl_viewport_invalid");
    if (state_.valid)
        return UiGpuBeginReceipt::preCaptureFailure(
            "product_ui_gl_frame_already_active");
    clearErrors();
    state_.valid = true;
    state_.blend = glIsEnabled(GL_BLEND) == GL_TRUE;
    state_.depthTest = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    state_.scissorTest = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    state_.framebufferSrgb = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &state_.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state_.vertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state_.arrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state_.activeTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state_.texture2DUnit0);
    glGetIntegerv(GL_BLEND_SRC_RGB, &state_.blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &state_.blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &state_.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &state_.blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &state_.blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state_.blendEquationAlpha);
    glGetIntegerv(GL_SCISSOR_BOX, state_.scissor);

    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
        GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glUseProgram(handles.program);
    glBindVertexArray(handles.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, handles.vertexBuffer);
    glBindTexture(GL_TEXTURE_2D, handles.atlasTexture);
    const UiGpuResult result = glStatus("begin_frame");
    return result.ok ? UiGpuBeginReceipt::capturedSuccess() :
        UiGpuBeginReceipt::capturedFailure(result.errorCode);
}

UiGpuResult Core41ProductUiGlDispatch::uploadVertices(
    const UiGpuResourceHandles& handles,
    const std::vector<UiGpuVertex>& vertices)
{
    if (!state_.valid) return {false, "product_ui_gl_frame_not_active"};
    if (vertices.empty()) return {false, "product_ui_gl_vertices_empty"};
    if (vertices.size() > static_cast<std::size_t>(
            std::numeric_limits<GLsizeiptr>::max() / sizeof(UiGpuVertex)))
        return {false, "product_ui_gl_vertex_buffer_overflow"};
    glBindBuffer(GL_ARRAY_BUFFER, handles.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(UiGpuVertex)),
        vertices.data(), GL_STREAM_DRAW);
    return glStatus("upload_vertices");
}

UiGpuResult Core41ProductUiGlDispatch::setScissor(
    const UiGpuScissor& scissor)
{
    if (!state_.valid) return {false, "product_ui_gl_frame_not_active"};
    if (scissor.width <= 0 || scissor.height <= 0)
        return {false, "product_ui_gl_scissor_invalid"};
    glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
    return glStatus("set_scissor");
}

UiGpuResult Core41ProductUiGlDispatch::draw(
    const UiGpuResourceHandles& handles, const UiGpuBatch& batch)
{
    if (!state_.valid) return {false, "product_ui_gl_frame_not_active"};
    if (batch.vertexCount == 0 || batch.firstVertex >
            static_cast<std::size_t>(std::numeric_limits<GLint>::max()) ||
        batch.vertexCount > static_cast<std::size_t>(
            std::numeric_limits<GLsizei>::max()))
        return {false, "product_ui_gl_draw_range_invalid"};
    glUseProgram(handles.program);
    glBindVertexArray(handles.vertexArray);
    glBindTexture(GL_TEXTURE_2D, handles.atlasTexture);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(batch.firstVertex),
        static_cast<GLsizei>(batch.vertexCount));
    return glStatus("draw");
}

UiGpuResult Core41ProductUiGlDispatch::endFrame()
{
    if (!state_.valid) return {false, "product_ui_gl_frame_not_active"};
    glUseProgram(static_cast<GLuint>(state_.program));
    glBindVertexArray(static_cast<GLuint>(state_.vertexArray));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(state_.arrayBuffer));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state_.texture2DUnit0));
    glActiveTexture(static_cast<GLenum>(state_.activeTexture));
    glBlendFuncSeparate(static_cast<GLenum>(state_.blendSrcRgb),
        static_cast<GLenum>(state_.blendDstRgb),
        static_cast<GLenum>(state_.blendSrcAlpha),
        static_cast<GLenum>(state_.blendDstAlpha));
    glBlendEquationSeparate(static_cast<GLenum>(state_.blendEquationRgb),
        static_cast<GLenum>(state_.blendEquationAlpha));
    glScissor(state_.scissor[0], state_.scissor[1], state_.scissor[2],
        state_.scissor[3]);
    setEnabled(GL_BLEND, state_.blend);
    setEnabled(GL_DEPTH_TEST, state_.depthTest);
    setEnabled(GL_SCISSOR_TEST, state_.scissorTest);
    setEnabled(GL_FRAMEBUFFER_SRGB, state_.framebufferSrgb);
    state_ = {};
    return glStatus("end_frame");
}

}  // namespace billiardgl::render::ui
