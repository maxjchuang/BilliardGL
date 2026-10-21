#pragma once

#include "render/modern/modern_product_session.h"
#include "render/ui/product_ui_gpu_renderer.h"

#include <memory>
#include <optional>
#include <array>
#include <string>
#include <vector>

namespace billiardgl::render {

struct ModernProductUiRuntimeConfig {
    std::string hudManifestPath;
    std::string hudManifestSha256;
    std::string hudTokenPath;
    std::string hudTokenSha256;
    std::string shellTokenPath;
    std::string shellTokenSha256;
    ui::FontAtlasConfig font;
    // Optional independent atlas for the fixed bilingual inset chrome.
    // Primary HUD/menu font and its evidence identity remain unchanged.
    std::optional<ui::FontAtlasConfig> insetChromeFont;
    // Optional for installed system fonts. Packaged fonts should provide a
    // locked SHA through configuration.
    std::string fontSha256;
    std::vector<std::string> supplementalUtf8Strings;
    // Internal capture/runtime verification only; trustedDefaults leaves it
    // false and the product application has no promotion path.
    bool rulesDecisionUiImplementationCapability = false;
    bool rulesDecisionIllegalBreakProductCapability = false;
    // Native evidence only. Normal shipping frames do not retain/copy the
    // complete text and glyph streams after GPU submission.
    bool retainSubmittedTextEvidence = false;

    static ModernProductUiRuntimeConfig trustedDefaults(
        const std::string& sourceRoot, const std::string& fontPath,
        long faceIndex = 0);
};

struct GlyphAtlasBindingAudit {
    bool ok = false;
    std::string errorCode;
    std::string atlasCacheKey;
    std::vector<std::string> glyphRecordSha256;
};

GlyphAtlasBindingAudit auditSubmittedGlyphAtlasBindings(
    const ui::SdfFontAtlas& atlas,
    const std::vector<ui::TextRunMetrics>& runs,
    const std::vector<ui::UiPrimitive>& glyphs);

struct TrajectoryInsetProductUiChromeTarget {
    std::uint64_t sessionSerial = 0, frameSerial = 0, layoutSerial = 0;
    std::string cp1GenerationId;
    std::array<std::string, 5> cp1Digests;
    std::string cp1GeometryDigest, cp2LayoutDigest, cp2ExclusionDigest;
    std::string chromeSemanticDigest, atlasCacheKey;
    training::TrajectoryInsetPalette palette =
        training::TrajectoryInsetPalette::Standard;
    int physicalWidth = 0, physicalHeight = 0, samples = 1;
    bool insetControlFocused = false;
    std::uint32_t currentLdrFramebuffer = 0, currentLdrColorTexture = 0;
};

struct TrajectoryInsetChromeSemanticRow {
    std::string token;
    ui::TrajectoryInsetChromeKind kind =
        ui::TrajectoryInsetChromeKind::OpaquePanel;
    product_hud::HudRect rect;
    std::size_t firstVertex = 0, vertexCount = 0;
    std::size_t firstBatch = 0, batchCount = 0;
    std::vector<ui::TrajectoryInsetChromeGpuRange> vertexRanges;
    std::vector<ui::TrajectoryInsetChromeGpuRange> batchRanges;
};

struct TrajectoryInsetChromePhysicalSample {
    int x = 0, y = 0, sampleIndex = 0;
    std::string semanticToken;
};

struct TrajectoryInsetProductUiChromeReceipt {
    bool ok = false, stateRestored = false, adapterContextPoisoned = false;
    std::string errorCode;
    std::uint32_t glError = 0;
    std::uint64_t sessionSerial = 0, frameSerial = 0, layoutSerial = 0;
    std::array<std::string, 5> cp1Digests;
    std::string cp1GeometryDigest, cp2LayoutDigest, cp2ExclusionDigest;
    std::string chromeSemanticDigest, atlasCacheKey;
    training::TrajectoryInsetPalette palette =
        training::TrajectoryInsetPalette::Standard;
    int physicalWidth = 0, physicalHeight = 0;
    std::uint32_t targetCurrentLdrFramebuffer = 0;
    std::uint32_t targetCurrentLdrColorTexture = 0;
    std::size_t semanticPrimitiveCount = 0;
    std::size_t submittedSolidVertexCount = 0, submittedSdfVertexCount = 0;
    std::size_t submittedBatchCount = 0, submittedDrawCallCount = 0;
    std::size_t coveredPhysicalSampleCount = 0;
    std::string orderedPrimitiveDigest, orderedBatchDigest;
    std::string cutoutZeroCoverageProofDigest;
    std::vector<TrajectoryInsetChromeSemanticRow> orderedSemanticRows;
    std::vector<ui::UiGpuBatch> submittedBatches;
    std::vector<TrajectoryInsetChromePhysicalSample> coveredPhysicalSamples;
    int finalPrepareCalls = 0, terminalPublishCalls = 0;
    int productPresentCalls = 0, fallbackPresentCalls = 0;
    static TrajectoryInsetProductUiChromeReceipt echoForTest(
        const ui::TrajectoryInsetVisibleCurrentFrame& current,
        const TrajectoryInsetProductUiChromeTarget& target);
};

std::string trajectoryInsetChromeSemanticDigest(
    const ui::TrajectoryInsetChromeDrawList& chrome);

struct ModernProductUiRuntimeResult {
    bool ok = false;
    std::string errorCode;
    ui::ProductUiOverlayKind overlay = ui::ProductUiOverlayKind::None;
    ProductScreen screen = ProductScreen::MainMenu;
    std::string canonicalSha256;
    bool gpuSubmitted = false;
    bool glyphEvidenceSuppressed = false;
    ui::ProductUiGpuMetrics gpuMetrics;
    // Retained semantic bounds for native final-frame audits.  These are the
    // exact runs consumed by the GPU submission, not a second layout pass.
    std::vector<ui::TextRunMetrics> textRuns;
    std::vector<ui::UiPrimitive> glyphQuads;
    std::vector<ui::UiPrimitive> diagnosticPrimitives;
    std::vector<ui::UiTriangle> diagnosticTriangles;
    std::string fontAtlasCacheKey;
    std::vector<std::string> glyphAtlasRecordSha256;
    bool glyphAtlasBindingVerified = false;
    std::vector<ui::RulesDecisionUiHitRect> rulesDecisionHitRects;
    std::vector<product_hud::HudRect> rulesDecisionExclusionRects;
};

class ModernProductUiBackend {
public:
    virtual ~ModernProductUiBackend() = default;
    virtual ui::UiGpuResult initialize(
        const ModernProductUiRuntimeConfig& config) = 0;
    virtual ui::UiGpuResult reinitializeGpu() = 0;
    virtual ui::UiGpuResult resize(
        int physicalWidth, int physicalHeight, float dpi) = 0;
    virtual ModernProductUiRuntimeResult render(
        const ModernProductSessionSnapshot& session,
        const PresentationSnapshot& presentation,
        const ui::ProductUiFrameEnvironment& environment) = 0;
    virtual TrajectoryInsetProductUiChromeReceipt submitTrajectoryInsetChrome(
        const ui::TrajectoryInsetVisibleCurrentFrame&,
        const TrajectoryInsetProductUiChromeTarget&)
    {
        TrajectoryInsetProductUiChromeReceipt result;
        result.errorCode = "trajectory_inset_chrome_backend_unsupported";
        return result;
    }
    virtual void contextLost() noexcept = 0;
    virtual void shutdown() noexcept = 0;
};

// Production-facing facade. It deliberately owns no layout, palette, input,
// shell, or gameplay authority; those remain in the session/coordinator.
class ModernProductUiRuntime final : public ModernProductUiBackend {
public:
    explicit ModernProductUiRuntime(ui::ProductUiGlDispatch& dispatch)
        : dispatch_(dispatch) {}
    ~ModernProductUiRuntime() override;

    ModernProductUiRuntime(const ModernProductUiRuntime&) = delete;
    ModernProductUiRuntime& operator=(const ModernProductUiRuntime&) = delete;

    ui::UiGpuResult initialize(
        const ModernProductUiRuntimeConfig& config) override;
    ui::UiGpuResult reinitializeGpu() override;
    ui::UiGpuResult resize(int physicalWidth, int physicalHeight,
        float dpi) override;
    ModernProductUiRuntimeResult render(
        const ModernProductSessionSnapshot& session,
        const PresentationSnapshot& presentation,
        const ui::ProductUiFrameEnvironment& environment) override;
    TrajectoryInsetProductUiChromeReceipt submitTrajectoryInsetChrome(
        const ui::TrajectoryInsetVisibleCurrentFrame& current,
        const TrajectoryInsetProductUiChromeTarget& target) override;
    TrajectoryInsetProductUiChromeReceipt submitTrajectoryInsetChrome(
        const ui::TrajectoryInsetVisibleCurrentFrame& current,
        const TrajectoryInsetProductUiChromeTarget& target,
        const TrajectoryInsetProductUiChromeReceipt& echoed);
    std::string trajectoryInsetChromeAtlasCacheKey() const
    { return insetChromeAtlas_.initialized() ? insetChromeAtlas_.cacheKey() : atlas_.cacheKey(); }
    bool trajectoryInsetChromeAtlasContainsExactCodepoints(
        const std::vector<std::uint32_t>& codepoints) const;
    // CPU-only preparation for same-frame inset exclusions. The next render
    // with these exact immutable frame inputs consumes this layout once.
    const ui::ProductUiFrame& prepareFrame(
        const ModernProductSessionSnapshot& session,
        const PresentationSnapshot& presentation,
        const ui::ProductUiFrameEnvironment& environment);
    // Evidence-only paired-frame control. It is rejected unless exact
    // submitted text evidence was enabled at initialization.
    ui::UiGpuResult setGlyphEvidenceSuppressed(bool suppressed);
    void contextLost() noexcept override;
    void shutdown() noexcept override;

    bool assetsInitialized() const noexcept { return assetsInitialized_; }
    bool gpuInitialized() const noexcept { return gpuInitialized_; }

private:
    ui::ProductUiGlDispatch& dispatch_;
    ModernProductUiRuntimeConfig config_;
    product_hud::ProductHudManifest manifest_;
    ui::HudShippingTokenSet hudTokens_;
    ui::HudShippingTokenSet shellTokens_;
    ui::HudShippingTokenRegistry registry_;
    ui::SdfFontAtlas atlas_;
    ui::SdfFontAtlas insetChromeAtlas_;
    std::unique_ptr<ui::ProductUiFrameCoordinator> coordinator_;
    std::unique_ptr<ui::ProductUiGpuRenderer> gpuRenderer_;
    std::unique_ptr<ui::ProductUiGpuRenderer> insetChromeRenderer_;
    bool assetsInitialized_ = false;
    bool gpuInitialized_ = false;
    bool hasResize_ = false;
    int physicalWidth_ = 0;
    int physicalHeight_ = 0;
    float dpi_ = 96.0f;
    std::string cachedShellFrameKey_;
    ui::ProductUiFrame cachedShellFrame_;
    bool hasCachedShellFrame_ = false;
    bool trajectoryInsetChromeContextPoisoned_ = false;
    bool glyphEvidenceSuppressed_ = false;
    std::optional<ui::ProductUiFrame> preparedFrame_;
    const ModernProductSessionSnapshot* preparedSession_ = nullptr;
    const PresentationSnapshot* preparedPresentation_ = nullptr;
    const ui::ProductUiFrameEnvironment* preparedEnvironment_ = nullptr;
};

}  // namespace billiardgl::render
