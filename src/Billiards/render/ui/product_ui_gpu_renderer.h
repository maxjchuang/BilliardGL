#pragma once

#include "render/ui/product_ui_frame_coordinator.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace billiardgl::render::ui {

struct UiGpuResult {
    bool ok = false;
    std::string errorCode;
};

struct UiGpuResourceHandles {
    std::uint32_t program = 0;
    std::uint32_t vertexArray = 0;
    std::uint32_t vertexBuffer = 0;
    std::uint32_t atlasTexture = 0;

    bool complete() const noexcept
    {
        return program != 0 && vertexArray != 0 && vertexBuffer != 0 &&
            atlasTexture != 0;
    }
};

struct UiGpuResourceCreateInfo {
    std::string vertexShader;
    std::string fragmentShader;
    int atlasWidth = 0;
    int atlasHeight = 0;
    const std::vector<std::uint8_t>* atlasPixels = nullptr;
};

struct UiGpuVertex {
    float ndcX = 0.0f;
    float ndcY = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    UiColor color;
    float sdf = 0.0f;
    // Bottom-left physical-pixel clip bounds.  Keeping clip data per vertex
    // lets one ordered draw preserve every original scissor rectangle.
    float clipLeft = 0.0f;
    float clipBottom = 0.0f;
    float clipRight = 0.0f;
    float clipTop = 0.0f;
    float interactiveChrome = 1.0f;
};

enum class UiGpuBatchKind { Solid, Sdf };

// Capture-only counterfactual. Normal is the only shipping mode. Suppression
// removes the exact submitted glyph quads and icon-label glyphs while leaving
// layout, panels, icons, ordering, camera and simulation state unchanged.
enum class ProductUiGpuEvidenceMode { Normal, SuppressGlyphs };

struct UiGpuScissor {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool operator==(const UiGpuScissor& other) const noexcept
    {
        return x == other.x && y == other.y && width == other.width &&
            height == other.height;
    }
};

struct UiGpuBatch {
    UiGpuBatchKind kind = UiGpuBatchKind::Solid;
    UiGpuScissor scissor;
    std::size_t firstVertex = 0;
    std::size_t vertexCount = 0;

    bool operator==(const UiGpuBatch& other) const noexcept
    {
        return kind == other.kind && scissor == other.scissor &&
            firstVertex == other.firstVertex && vertexCount == other.vertexCount;
    }
};

struct TrajectoryInsetChromeGpuTargetBinding {
    std::uint32_t framebuffer = 0;
    std::uint32_t colorTexture = 0;
    bool valid = false;
};

// Strong begin-frame receipt.  A successful begin can only be constructed as
// a captured transaction that requires the outer owner to restore it.
class UiGpuBeginReceipt {
public:
    static UiGpuBeginReceipt preCaptureFailure(std::string errorCode)
    { return UiGpuBeginReceipt(false, false, false, std::move(errorCode)); }
    static UiGpuBeginReceipt capturedFailure(std::string errorCode)
    { return UiGpuBeginReceipt(false, true, true, std::move(errorCode)); }
    static UiGpuBeginReceipt capturedSuccess()
    { return UiGpuBeginReceipt(true, true, true, {}); }

    bool setupOk() const noexcept { return setupOk_; }
    bool stateCaptured() const noexcept { return stateCaptured_; }
    bool restoreRequired() const noexcept { return restoreRequired_; }
    const std::string& errorCode() const noexcept { return errorCode_; }

private:
    UiGpuBeginReceipt(bool setupOk, bool stateCaptured, bool restoreRequired,
        std::string errorCode)
        : setupOk_(setupOk), stateCaptured_(stateCaptured),
          restoreRequired_(restoreRequired), errorCode_(std::move(errorCode))
    {}

    bool setupOk_ = false;
    bool stateCaptured_ = false;
    bool restoreRequired_ = false;
    std::string errorCode_;
};

// Injectable command boundary. The production implementation maps these calls
// to OpenGL 4.1 Core; unit tests use a recording implementation with no GL
// library or window dependency.
class ProductUiGlDispatch {
public:
    virtual ~ProductUiGlDispatch() = default;
    virtual UiGpuResult createResources(const UiGpuResourceCreateInfo& info,
        UiGpuResourceHandles& handles) = 0;
    virtual void destroyResources(UiGpuResourceHandles& handles) noexcept = 0;
    virtual void abandonResources(UiGpuResourceHandles& handles) noexcept = 0;
    virtual UiGpuResult beginFrame(const UiGpuResourceHandles& handles,
        int physicalWidth, int physicalHeight) = 0;
    // Compatibility adapter for legacy test dispatches.  Production Core41
    // overrides this typed method and never relies on inferred receipt state.
    virtual UiGpuBeginReceipt beginFrameReceipt(
        const UiGpuResourceHandles& handles, int physicalWidth,
        int physicalHeight)
    {
        const UiGpuResult result = beginFrame(
            handles, physicalWidth, physicalHeight);
        return result.ok ? UiGpuBeginReceipt::capturedSuccess() :
            UiGpuBeginReceipt::capturedFailure(result.errorCode);
    }
    virtual UiGpuResult uploadVertices(const UiGpuResourceHandles& handles,
        const std::vector<UiGpuVertex>& vertices) = 0;
    virtual UiGpuResult setScissor(const UiGpuScissor& scissor) = 0;
    virtual UiGpuResult draw(const UiGpuResourceHandles& handles,
        const UiGpuBatch& batch) = 0;
    virtual UiGpuResult endFrame() = 0;
    virtual TrajectoryInsetChromeGpuTargetBinding
    bindAndQueryTrajectoryInsetChromeTarget()
    {
        return trajectoryInsetChromeTargetQuery_ ?
            trajectoryInsetChromeTargetQuery_() : trajectoryInsetChromeTarget_;
    }
    virtual std::uint32_t adjacentTrajectoryInsetChromeGlError()
    {
        return trajectoryInsetChromeErrorQuery_ ?
            trajectoryInsetChromeErrorQuery_() : 0;
    }
    void configureTrajectoryInsetChromeTarget(
        TrajectoryInsetChromeGpuTargetBinding target) noexcept
    {
        trajectoryInsetChromeTarget_ = target;
    }
    void configureTrajectoryInsetChromeQueries(
        std::function<TrajectoryInsetChromeGpuTargetBinding()> targetQuery,
        std::function<std::uint32_t()> errorQuery)
    {
        trajectoryInsetChromeTargetQuery_ = std::move(targetQuery);
        trajectoryInsetChromeErrorQuery_ = std::move(errorQuery);
    }

private:
    TrajectoryInsetChromeGpuTargetBinding trajectoryInsetChromeTarget_;
    std::function<TrajectoryInsetChromeGpuTargetBinding()>
        trajectoryInsetChromeTargetQuery_;
    std::function<std::uint32_t()> trajectoryInsetChromeErrorQuery_;
};

struct ProductUiGpuMetrics {
    std::size_t inputPrimitives = 0;
    std::size_t inputTriangles = 0;
    std::size_t inputIconLabels = 0;
    std::size_t solidVertices = 0;
    std::size_t sdfVertices = 0;
    std::size_t batches = 0;
    std::size_t drawCalls = 0;
    std::size_t clippedItems = 0;
    std::size_t rejectedOpaqueIcons = 0;
    bool glyphEvidenceSuppressed = false;
    std::size_t suppressedGlyphPrimitives = 0;
    std::size_t suppressedIconLabels = 0;
};

struct ProductUiGpuFrameResult {
    bool ok = false;
    std::string errorCode;
    ProductUiGpuMetrics metrics;
    std::vector<UiGpuBatch> batches;
    std::vector<UiGpuVertex> vertices;
};

struct TrajectoryInsetChromeGpuRange {
    std::size_t first = 0;
    std::size_t count = 0;
};

struct TrajectoryInsetChromeGpuRow {
    std::string token;
    TrajectoryInsetChromeKind kind = TrajectoryInsetChromeKind::OpaquePanel;
    product_hud::HudRect rect;
    std::vector<TrajectoryInsetChromeGpuRange> vertexRanges;
    std::vector<TrajectoryInsetChromeGpuRange> batchRanges;
};

struct TrajectoryInsetChromeGpuResult : ProductUiGpuFrameResult {
    TrajectoryInsetChromeGpuTargetBinding target;
    std::uint32_t adjacentGlError = 0;
    bool stateRestored = false;
    bool restoreFailed = false;
    std::vector<TrajectoryInsetChromeGpuRow> rows;
};

class ProductUiGpuRenderer {
public:
    explicit ProductUiGpuRenderer(ProductUiGlDispatch& dispatch)
        : dispatch_(dispatch) {}
    ~ProductUiGpuRenderer();

    ProductUiGpuRenderer(const ProductUiGpuRenderer&) = delete;
    ProductUiGpuRenderer& operator=(const ProductUiGpuRenderer&) = delete;

    UiGpuResult initialize(const SdfFontAtlas& atlas);
    ProductUiGpuFrameResult render(const ProductUiFrame& frame,
        ProductUiGpuEvidenceMode evidenceMode =
            ProductUiGpuEvidenceMode::Normal);
    TrajectoryInsetChromeGpuResult renderTrajectoryInsetChrome(
        const TrajectoryInsetChromeDrawList& chrome,
        const TrajectoryInsetShellLayout& layout, int physicalWidth,
        int physicalHeight);
    void shutdown() noexcept;
    // A lost context owns and invalidates its GL objects; do not issue deletes
    // into the replacement context. The next initialize performs a full rebuild.
    void contextLost() noexcept;

    bool initialized() const noexcept { return resources_.complete(); }
    const std::string& atlasCacheKey() const noexcept { return atlasCacheKey_; }

private:
    ProductUiGlDispatch& dispatch_;
    UiGpuResourceHandles resources_;
    const SdfFontAtlas* atlas_ = nullptr;
    std::string atlasCacheKey_;
};

}  // namespace billiardgl::render::ui
