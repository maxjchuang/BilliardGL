#pragma once

#include "product_shell_controller.h"
#include "render/ui/product_shell_draw_list.h"
#include "render/ui/product_shell_layout.h"
#include "training/trajectory_inset_model.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::render::ui {
struct TrajectoryInsetVisibleCurrentFrame;
}

namespace billiardgl::render::modern {

enum class TrajectoryInsetVisibleStage {
  None,
  FreezeCurrentIdentity,
  AuditCp1Cp2,
  PrepareCp3,
  VerifyCompositeEligible,
  StagePlot,
  StageChrome,
  RestoreState,
  AtomicPresent
};

const char *trajectoryInsetVisibleStageName(TrajectoryInsetVisibleStage);
std::vector<TrajectoryInsetVisibleStage>
orderedTrajectoryInsetVisibleFailureStages();

enum class TrajectoryInsetVisibleInvalidation {
  Resize,
  ContextLoss,
  ScreenChange,
  ModalChange,
  ToggleChange
};

struct TrajectoryInsetVisibleFrameRequest {
  bool present = false;
  std::uint64_t sessionSerial = 0, frameSerial = 0, layoutSerial = 0;
  ProductScreen screen = ProductScreen::MainMenu;
  bool sessionStable = false, transitioning = false, errored = false,
       modalVisible = false;
  bool insetToggleOn = false, worldOverlayToggleOn = false;
  int samples = 1;
  training::TrajectoryInsetPalette palette =
      training::TrajectoryInsetPalette::Standard;
  const training::TrajectoryInsetDrawPackage *package = nullptr;
  const ui::TrajectoryInsetShellRequest *shellRequest = nullptr;
  const ui::TrajectoryInsetShellLayout *layout = nullptr;
  const ui::TrajectoryInsetChromeDrawList *chrome = nullptr;
  std::string cp1GenerationId;
  std::array<std::string, 5> cp1Digests;
  std::string cp1GeometryDigest;
  std::string cp2ProfileId;
  std::string cp2LayoutDigest, cp2ExclusionDigest;
};

struct TrajectoryInsetVisiblePreparation {
  bool ok = false;
  bool committedOffscreen = false;
  bool visibleCompositePerformed = false;
  bool stateRestored = false;
  std::string errorCode;
  std::uint64_t frameSerial = 0;
  std::uint64_t resourceGeneration = 0;
};

struct TrajectoryInsetVisibleWorkingSet {
  TrajectoryInsetVisiblePreparation preparation;
  std::uint64_t cp3StagingResource = 0;
  std::uint64_t preparedSessionSerial = 0, preparedLayoutSerial = 0;
  std::string preparedCp1GenerationId;
  std::array<std::string, 5> preparedCp1Digests;
  std::string preparedCp2ProfileId;
  std::string preparedCp1GeometryDigest, preparedCp2LayoutDigest,
      preparedCp2ExclusionDigest;
  std::size_t stagedPlotPixels = 0, stagedInsetTokens = 0,
              stagedChromePrimitives = 0;
};

struct TrajectoryInsetVisibleResult {
  bool ok = false, presented = false, stateRestored = false,
       invalidated = false;
  std::string errorCode;
  std::vector<std::string> orderedLedger;
};

class TrajectoryInsetVisibleDispatch {
public:
  virtual ~TrajectoryInsetVisibleDispatch() = default;
  virtual std::uint64_t snapshotState() = 0;
  virtual bool invoke(TrajectoryInsetVisibleStage,
                      const TrajectoryInsetVisibleFrameRequest &,
                      TrajectoryInsetVisibleWorkingSet &, std::string &) = 0;
  virtual unsigned readError(TrajectoryInsetVisibleStage) = 0;
  virtual bool restoreState(std::uint64_t, std::string &) = 0;
  virtual unsigned readRestoreError() = 0;
  virtual bool presentAtomic(const TrajectoryInsetVisibleFrameRequest &,
                             const TrajectoryInsetVisibleWorkingSet &,
                             std::string &) = 0;
  virtual void invalidate(TrajectoryInsetVisibleWorkingSet &) = 0;
  virtual void
  invalidateLifecycle(TrajectoryInsetVisibleInvalidation) noexcept = 0;
};

class TrajectoryInsetVisibleIntegration {
public:
  explicit TrajectoryInsetVisibleIntegration(
      TrajectoryInsetVisibleDispatch &dispatch)
      : dispatch_(dispatch) {}

  TrajectoryInsetVisibleResult
  execute(const TrajectoryInsetVisibleFrameRequest &request);
  void invalidate(TrajectoryInsetVisibleInvalidation) noexcept;
  bool hasReusableEligibility() const noexcept { return reusableEligibility_; }

private:
  TrajectoryInsetVisibleDispatch &dispatch_;
  bool reusableEligibility_ = false;
  std::uint64_t lastAttemptSerial_ = 0;
};

enum class TrajectoryInsetFinalResourceKind { None, Texture, Framebuffer };

struct TrajectoryInsetFinalResourceKey {
  TrajectoryInsetFinalResourceKind kind =
      TrajectoryInsetFinalResourceKind::None;
  std::uint64_t handle = 0;
  friend bool operator==(const TrajectoryInsetFinalResourceKey &first,
                         const TrajectoryInsetFinalResourceKey &second) {
    return first.kind == second.kind && first.handle == second.handle;
  }
  friend bool operator!=(const TrajectoryInsetFinalResourceKey &first,
                         const TrajectoryInsetFinalResourceKey &second) {
    return !(first == second);
  }
};

enum class TrajectoryInsetVisibleAtomicFinalStage {
  None,
  FreezeIdentity,
  AllocateFinal,
  ValidateIsolation,
  StagePlot,
  StageSemantic,
  StageChrome,
  RestoreState,
  AtomicPresent
};

const char *trajectoryInsetVisibleAtomicFinalStageName(
    TrajectoryInsetVisibleAtomicFinalStage);
std::vector<TrajectoryInsetVisibleAtomicFinalStage>
orderedTrajectoryInsetVisibleAtomicFinalStages();

struct TrajectoryInsetVisibleAtomicFinalResources {
  std::uint64_t sessionSerial = 0, frameSerial = 0, layoutSerial = 0,
                preparationGeneration = 0, generation = 0;
  std::string packageGeneration;
  int width = 0, height = 0, samples = 0;
  product_hud::HudRect cutout, plotPhysicalCoverage;
  TrajectoryInsetFinalResourceKey cp3ColorStaging, mainLdr, mainSemantic,
      finalColor, finalSemantic, finalFramebuffer;
  std::string finalSemanticFormat;
  bool isolationValidated = false, feedbackValidated = false,
       fixedPaletteSrgb = false, chromeAudited = false,
       chromeRefillsCutout = false,
       panelContainerExcludedFromTrajectoryDenominator = false;
  int toneApplicationCount = 0;
  std::uint32_t panelContainerUiToken = 0;
  std::size_t plotPixels = 0, semanticTokens = 0, chromePrimitives = 0;

  bool complete() const noexcept;
};

class TrajectoryInsetVisibleAtomicFinalDispatch {
public:
  virtual ~TrajectoryInsetVisibleAtomicFinalDispatch() = default;
  virtual bool invoke(TrajectoryInsetVisibleAtomicFinalStage,
                      const ui::TrajectoryInsetVisibleCurrentFrame &,
                      TrajectoryInsetVisibleAtomicFinalResources &,
                      std::string &) = 0;
  virtual unsigned readError(TrajectoryInsetVisibleAtomicFinalStage) = 0;
  virtual bool preflightPresent(
      const TrajectoryInsetVisibleAtomicFinalResources &, std::string &) = 0;
  virtual void publishAtomicNoexcept(
      const TrajectoryInsetVisibleAtomicFinalResources &) noexcept = 0;
  virtual void invalidate(
      TrajectoryInsetVisibleAtomicFinalResources &) noexcept = 0;
};

TrajectoryInsetVisibleResult executeTrajectoryInsetVisibleAtomicFinal(
    ui::TrajectoryInsetVisibleCurrentFrame &,
    TrajectoryInsetVisibleAtomicFinalDispatch &);

// CP4-D keeps every failure-prone GL operation in an independent final
// staging transaction.  The terminal publication below is deliberately a
// CPU-only output-handle publication: it may not issue GL commands or throw.
enum class TrajectoryInsetVisibleFinalGlStage {
  None,
  FreezeIdentity,
  Allocate,
  Attach,
  ValidateIsolation,
  CopyProductLdr,
  CompositeInset,
  CompositeSemantic,
  CompositeChrome,
  FinalFramebufferCheck,
  RestoreState,
  FinalErrorQuery
};

const char *trajectoryInsetVisibleFinalGlStageName(
    TrajectoryInsetVisibleFinalGlStage);
std::vector<TrajectoryInsetVisibleFinalGlStage>
orderedTrajectoryInsetVisibleFinalGlStages();

enum class TrajectoryInsetVisibleFinalGlInvalidation {
  FrameSuperseded,
  Resize,
  ContextLoss
};

struct TrajectoryInsetVisibleFinalGlInput {
  const ui::TrajectoryInsetVisibleCurrentFrame *current = nullptr;
  std::uint64_t sessionSerial = 0, frameSerial = 0, layoutSerial = 0,
                preparationGeneration = 0;
  std::string packageGeneration;
  int width = 0, height = 0, outputWidth = 0, outputHeight = 0,
      cp3InputSamples = 0, finalOutputSamples = 1;
  product_hud::HudRect cutout;
  TrajectoryInsetFinalResourceKey cp3ColorStaging, cp3InsetSemantic,
      currentProductLdr, mainWorldSemantic;
  std::string cp3InsetSemanticFormat;
  std::uint64_t cp3InsetSemanticGeneration = 0;
  int cp3InsetSemanticChannel = 0;
};

enum class TrajectoryInsetVisibleFinalContentOpKind {
  ClearFinalSemanticR32ui,
  CopyCurrentProductLdr,
  CompositeCp3InsetColor,
  CopyCp3InsetSemantic
};

struct TrajectoryInsetVisibleFinalContentOp {
  TrajectoryInsetVisibleFinalContentOpKind kind =
      TrajectoryInsetVisibleFinalContentOpKind::ClearFinalSemanticR32ui;
  TrajectoryInsetFinalResourceKey source, destination;
  int destinationAttachment = 0;
  product_hud::HudRect sourceRect, destinationRect;
  std::string sourceFormat;
  std::uint64_t sourceGeneration = 0;
  int integerSourceChannel = 0;
  bool integerShaderConversionToR32ui = false;
  std::uint32_t clearIntegerValue = 0;
};

struct TrajectoryInsetVisibleFinalContentOpReceipt {
  TrajectoryInsetVisibleFinalContentOpKind kind =
      TrajectoryInsetVisibleFinalContentOpKind::ClearFinalSemanticR32ui;
  TrajectoryInsetFinalResourceKey actualSource, actualDestination;
  int actualDestinationAttachment = 0;
  product_hud::HudRect actualSourceRect, actualDestinationRect;
  std::string actualSourceFormat;
  std::uint64_t actualSourceGeneration = 0;
  int actualIntegerSourceChannel = 0;
  bool integerShaderConversionToR32ui = false;
  int actualWriteCount = 0;
  unsigned glError = 0;
};

struct TrajectoryInsetVisibleFinalGlResources {
  TrajectoryInsetFinalResourceKey finalColor, finalInsetSemantic,
      finalFramebuffer;
  std::string finalInsetSemanticFormat;
  int width = 0, height = 0, cp3InputSamples = 0,
      finalOutputSamples = 1, finalColorMetadataSamples = 0,
      finalSemanticMetadataSamples = 0;
  bool attachmentsExact = false, isolationExact = false,
       feedbackFree = false, copiedProductLdr = false,
       insetColorComposited = false,
       insetSemanticRetainedIndependent = false,
       chromeComposited = false, cutoutNotRefilled = false,
       finalFramebufferComplete = false, finalErrorQueryComplete = false;
  int toneApplicationCount = 0;
  bool productChromeAlreadyInCurrentLdr = false;
  int additionalChromeDrawWrites = 0;
  std::size_t verifiedContentOpCount = 0;
  std::string orderedContentOpDigest;

  bool complete() const noexcept;
};

struct TrajectoryInsetVisibleFinalGlResult {
  bool ok = false, stateRestored = false, hardRestoreFailure = false,
       adapterContextPoisoned = false;
  std::string errorCode;
  TrajectoryInsetVisibleFinalGlResources resources;
};

class TrajectoryInsetVisibleFinalGlDispatch {
public:
  virtual ~TrajectoryInsetVisibleFinalGlDispatch() = default;
  virtual bool contextUsable() const noexcept = 0;
  virtual void poisonContext() noexcept = 0;
  virtual std::uint64_t captureState() = 0;
  virtual bool invoke(TrajectoryInsetVisibleFinalGlStage,
                      const TrajectoryInsetVisibleFinalGlInput &,
                      TrajectoryInsetVisibleFinalGlResources &,
                      std::string &) = 0;
  virtual bool executeContentOp(
      const TrajectoryInsetVisibleFinalContentOp &,
      TrajectoryInsetVisibleFinalContentOpReceipt &, std::string &) = 0;
  virtual unsigned readError(TrajectoryInsetVisibleFinalGlStage) = 0;
  virtual bool restoreState(std::uint64_t, std::string &) = 0;
  virtual void destroy(TrajectoryInsetVisibleFinalGlResources &) noexcept = 0;
  virtual void terminalPublishOutputHandleNoexcept(
      const TrajectoryInsetVisibleFinalGlResources &) noexcept = 0;
};

TrajectoryInsetVisibleFinalGlResult
prepareTrajectoryInsetVisibleFinalGlStaging(
    const TrajectoryInsetVisibleFinalGlInput &,
    TrajectoryInsetVisibleFinalGlDispatch &);
void invalidateTrajectoryInsetVisibleFinalGlStaging(
    TrajectoryInsetVisibleFinalGlInvalidation,
    const TrajectoryInsetVisibleFinalGlResult &,
    TrajectoryInsetVisibleFinalGlDispatch &) noexcept;

} // namespace billiardgl::render::modern
