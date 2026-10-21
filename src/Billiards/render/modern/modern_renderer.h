#pragma once

#include "render/modern/lookdev_settings.h"
#include "render/modern/modern_renderer_product_ui.h"
#include "render/modern/pbr_materials.h"
#include "render/modern/pbr_texture_resources.h"
#include "render/modern/renderer_backend.h"
#include "training/final_semantic_ownership.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace billiardgl::render {

struct ModernProductSessionSnapshot;
namespace ui {
struct ProductUiFrameEnvironment;
struct TrajectoryInsetShellLayout;
struct TrajectoryInsetVisibleCurrentFrame;
} // namespace ui

} // namespace billiardgl::render

namespace billiardgl::training {
struct TrajectoryInsetDrawPackage;
}

namespace billiardgl::render {

enum class RenderQuality { Low, Medium, High };

struct RenderQualitySettings {
  int shadowMapSize = 1024;
  int pcfKernelWidth = 2;
  float shadowBias = 0.0015f;
  float normalBias = 0.003f;
  int rectangularLightSamples = 4;
  float contactShadowOpacity = 0.24f;
};

inline RenderQualitySettings qualitySettings(RenderQuality quality) noexcept {
  switch (quality) {
  case RenderQuality::Low:
    return {1024, 1, 0.0025f, 0.0040f, 1, 0.16f};
  case RenderQuality::Medium:
    return {2048, 2, 0.0015f, 0.0030f, 16, 0.22f};
  case RenderQuality::High:
    return {4096, 3, 0.0009f, 0.0020f, 16, 0.26f};
  }
  return {};
}

inline int effectiveBallAreaLightSamples(
    RenderQuality quality,
    std::optional<int> diagnosticOverride = std::nullopt) noexcept {
  return diagnosticOverride.has_value()
             ? *diagnosticOverride
             : qualitySettings(quality).rectangularLightSamples;
}

inline float effectiveBallShadowReceiverBiasScale(
    std::optional<float> diagnosticRollback = std::nullopt) noexcept {
  return diagnosticRollback.value_or(4.0f);
}

// Low keeps basic tone mapping. Medium/High use a small pre-tone-map glow;
// the user's accessibility switch always wins over the quality preset.
inline float effectiveBloomStrength(RenderQuality quality, bool enabled) noexcept {
  return enabled && quality != RenderQuality::Low ? 0.06f : 0.0f;
}

inline float effectiveDepthOfFieldRadius(RenderQuality quality, bool enabled,
                                        bool replay) noexcept {
  return enabled && replay && quality == RenderQuality::High ? 2.0f : 0.0f;
}

struct RenderPassMetric {
  std::string name;
  double cpuMilliseconds = 0.0;
  double gpuMilliseconds = 0.0;
  std::uint32_t drawCalls = 0;
  // True only when this render observed a newly completed GPU query.  The
  // milliseconds field may retain the last value for display while false.
  bool gpuTimingAvailable = false;
  // Submission frame associated with the newly completed query.  This is
  // non-zero only when gpuTimingAvailable is true, so consumers can combine
  // passes only when their asynchronous samples came from the same frame.
  std::uint64_t gpuTimingSourceFrame = 0;
};

struct ModernFrameMetrics {
  float depthOfFieldRadius = 0.0f;
  int sceneDepthBits = 0;
  bool bloomApplied = false;
  float bloomStrength = 0.0f;
  std::vector<RenderPassMetric> passes;
  std::uint32_t visibleBalls = 0;
  std::uint32_t triangles = 0;
  int msaaSamples = 0;
  RenderQuality quality = RenderQuality::Medium;
  int shadowMapSize = 0;
  int pcfKernelWidth = 0;
  Float4 ballNdcBounds{{1.0f, 1.0f, -1.0f, -1.0f}};
  bool compositionSafe = false;
  bool pbrPackageLoaded = false;
  int pbrTextureCount = 0;
  std::size_t pbrGpuBytes = 0;
  std::size_t environmentGpuBytes = 0;
  int environmentResolution = 0;
  bool gltfSceneLoaded = false;
  bool venueSceneLoaded = false;
  std::size_t sparsePropShadowGpuBytes = 0;
  std::size_t gltfSceneTriangles = 0;
  bool anisotropySupported = false;
  float anisotropy = 1.0f;
  std::size_t trajectoryOverlayVertices = 0;
  double trajectoryOverlayCpuBuildMilliseconds = 0.0;
  std::uint32_t trajectoryOverlayDrawCalls = 0;
  bool trajectoryOverlayActualDrawSubmitted = false;
  bool trajectoryOverlayActualBackdropFrozen = false;
  bool trajectoryOverlayPredictedDrawSubmitted = false;
  bool trajectoryOverlaySubmissionOrderValid = false;
  std::uint32_t trajectoryOverlayTextureCount = 0;
  bool trajectoryOverlayUsedSharedProjection = false;
  bool trajectoryOverlayHomogeneousClip = false;
  bool trajectoryOverlaySafeAreaClip = false;
  bool trajectoryOverlayPlaySurfaceClip = false;
  std::string trajectoryOverlayErrorCode;
  bool productPresentPerformed = false;
  bool trajectoryInsetBlockedProductPresent = false;
  std::string trajectoryInsetVisibleErrorCode;
  std::string trajectoryInsetLayoutDiagnostic;
};

struct ModernRendererOptions {
  // Non-product lookdev/capture is opt-in; product frames use live settings.
  bool enableBloom = false;
  bool diagnosticPassTrace = false; // test-only; synchronizes each pass
  int msaaSamples = 4;
  bool debugErrors = true;
  bool synchronousGpuTiming =
      false; // test-only; production uses non-blocking query rings
  RenderQuality quality = RenderQuality::Medium;
  std::string lookdevManifestPath;
  std::string pbrParametersPath;
  std::string pbrAssetManifestPath;
  std::string lightingRigPath;
  std::string gltfScenePath;
  std::string venueScenePath;
  // Explicit candidate-only real-geometry floor shadows; no default shader,
  // resources or table-shadow changes. Requires the sparse furniture contract.
  bool enableSparsePropShadows = false;
  bool enableProductUi = false;
  bool suppressLegacyUi = false;
  bool pocketObjectIdPass = false;
  // Debug-only visible-surface ownership, same geometry/camera/MSAA.
  // Palette: felt red, cue0 green, venue blue, numbered balls yellow,
  // everything else black. Mixed coverage/alpha pixels are not class cores.
  bool objectClassMaskPass = false;
  // Opt-in art-calibrated local indirect light; not energy-conserving GI.
  bool enablePocketIndirectBounce = false;
  std::string productUiSourceRoot;
  std::string productUiFontPath;
  long productUiFontFaceIndex = 0;
  // Internal evidence implementation only. Shipping options leave false.
  bool productUiRulesDecisionImplementationCapability = false;
  // Shipping exposes only the typed WPA II 6(c) IllegalBreak choice.
  bool productUiRulesDecisionIllegalBreakCapability = false;
  // Native evidence only; shipping leaves full text/glyph retention disabled.
  bool productUiRetainSubmittedTextEvidence = false;
  // Capture-only MRT/readback capability. False in every shipping profile.
  bool enableFinalOwnershipInstrumentation = false;
};

// CP3 exposes a narrow, explicit, non-presenting input seam. No product screen,
// camera, automation flag, or runtime toggle can synthesize this input. CP4
// must separately authorize consumption of a successfully prepared texture by
// the visible product frame.
struct TrajectoryInsetAtomicFrameInput {
  const training::TrajectoryInsetDrawPackage *cp1Package = nullptr;
  const ui::TrajectoryInsetShellLayout *cp2Layout = nullptr;
  std::uint64_t frameSerial = 0;
  std::string cp1GenerationId;
  std::array<std::string, 5> cp1OrderedDigests;
  std::string cp1GeometryDigest;
  std::string cp2LayoutDigest;
  std::uint64_t cp2LayoutSerial = 0;
  bool instrumentationEnabled = false;
};

struct TrajectoryInsetAtomicPreparation {
  bool ok = false;
  bool committedOffscreen = false;
  bool visibleCompositePerformed = false;
  bool stateRestored = false;
  std::string errorCode = "trajectory_inset_input_absent";
  std::uint64_t frameSerial = 0;
  std::uint64_t resourceGeneration = 0;
  std::uint32_t mainHdrWrites = 0;
  std::uint32_t mainWorldTokenWrites = 0;
  std::uint32_t visibleCompositeWrites = 0;
  std::vector<std::string> stageLedger;
};

// CPU-only consumer audit, shared by real submission and regression tests.
bool validateTrajectoryInsetPackageForSubmission(
    const training::TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetAtomicFrameInput& input, std::string& error);

struct TrajectoryInsetVisibleFinalGlAdapter {
  bool prepared = false;
  bool poisoned = false;
  std::uint64_t outputGeneration = 0;
};

class ModernRenderer final : public RendererBackend {
public:
  explicit ModernRenderer(ModernRendererOptions options);
  ~ModernRenderer() override;

  bool initialize(const RendererInitialization &initialization,
                  std::string &error) override;
  void resize(int framebufferWidth, int framebufferHeight) override;
  void render(const PresentationSnapshot &snapshot) override;
  void renderProductFrame(const PresentationSnapshot &snapshot,
                          const ModernProductSessionSnapshot &session,
                          const ui::ProductUiFrameEnvironment &environment);
  // Replays the already-frozen product inputs for native pixel causality
  // evidence without consuming a second one-shot trajectory-inset authority
  // transaction. The ordinary renderProductFrame remains the sole
  // user-visible path; callers must prove the restored pixels are identical.
  void renderProductFrameEvidenceReplay(
      const PresentationSnapshot &snapshot,
      const ModernProductSessionSnapshot &session,
      const ui::ProductUiFrameEnvironment &environment);
  void productUiContextLost() noexcept;
  bool reinitializeProductUi(std::string &error);
  // Native evidence only. Shipping initialization leaves submitted-text
  // retention disabled, so attempts to enable this counterfactual fail closed.
  bool setProductUiGlyphEvidenceSuppressed(bool suppressed,
                                           std::string &error);
  // Performs the authorized offscreen CP3 transaction only. It never calls
  // the product UI pass and never composites into the visible main LDR.
  TrajectoryInsetAtomicPreparation prepareTrajectoryInsetAtomicFrame(
      const TrajectoryInsetAtomicFrameInput &input);
  void invalidatePreparedTrajectoryInsetAtomicFrame() noexcept;
  bool prepareTrajectoryInsetVisibleFinalStaging(
      ui::TrajectoryInsetVisibleCurrentFrame &current,
      std::string &error);
  std::uint32_t publishedProductOutputFramebuffer() const noexcept;
  std::optional<product_hud::HudRect> latestTrajectoryInsetToggleRect() const noexcept;
  std::optional<product_hud::HudRect> latestTrajectoryInsetPlotRect() const noexcept;
  std::uint64_t latestTrajectoryInsetFrameSerial() const noexcept;
  // Explicit bounded native self-test; never called by normal product frames.
  bool verifyTrajectoryInsetTokenCodec(std::string& error);
  void invalidateTrajectoryInsetVisibleFinalStaging() noexcept;
  bool capture(const RenderCaptureRequest &request,
               std::string &error) override;
  // Explicit validation readback of the light-space depth attachment. This is
  // never used by the shipping frame path and therefore cannot introduce a
  // per-frame synchronization point.
  bool captureShadowDepthForValidation(const std::string &outputPath,
                                       std::string &error) const;
  bool captureIntegratedFinalOwnership(
      const std::string &captureId,
      training::IntegratedFinalOwnershipFrame &frame, std::string &error);
  void shutdown() noexcept override;

  const ModernFrameMetrics &metrics() const noexcept;
  const ModernRendererProductUiMetrics &productUiMetrics() const noexcept;
  const ModernProductUiRuntimeResult &latestProductUiResult() const noexcept;
  const LookdevLoadResult &lookdev() const noexcept;
  const PbrPackageDescription &pbrPackage() const noexcept;
  const PbrUploadReport &pbrUpload() const noexcept;
  bool gltfSceneLoaded() const noexcept;
  bool venueSceneLoaded() const noexcept;

private:
  void renderFrame(const PresentationSnapshot &snapshot,
                   const ModernProductSessionSnapshot *session,
                   const ui::ProductUiFrameEnvironment *environment,
                   bool evidenceReplay = false);
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace billiardgl::render
