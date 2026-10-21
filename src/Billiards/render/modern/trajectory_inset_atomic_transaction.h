#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::render::modern {

enum class TrajectoryInsetAtomicStage {
  None,
  FormatSampleQuery,
  ColorAllocation,
  SemanticR32uiAllocation,
  GeometryUpload,
  FramebufferAttachComplete,
  ColorClear,
  SemanticClear,
  TableDraw,
  TrajectoryDraw,
  ColorResolve,
  PerSampleSemanticResolve,
  FixedPaletteToneEncode,
  GenerationFrameSerialCheck,
  AtomicComposite,
  StateRestore
};
const char *trajectoryInsetAtomicStageName(TrajectoryInsetAtomicStage);
std::vector<TrajectoryInsetAtomicStage>
orderedTrajectoryInsetAtomicFailureStages();

struct TrajectoryInsetAtomicIdentityContract {
  std::string cp1GenerationId;
  std::uint64_t frameSerial = 0, lastAttemptSerial = 0, cp2LayoutSerial = 0;
  std::array<std::string, 5> embeddedCp1Digests;
  std::array<std::string, 5> recomputedCp1Digests;
  std::array<std::string, 5> expectedCp1Digests;
  std::string recomputedCp1GeometryDigest, expectedCp1GeometryDigest;
  std::string recomputedCp2LayoutDigest, expectedCp2LayoutDigest;
  std::size_t sourceGroups = 0, mappingRows = 0, orderedParents = 0;
  std::size_t semanticMembers = 0, semanticDenominator = 0;
  std::size_t probes = 0, probeDenominator = 0, orderedProbeIds = 0;
  bool generationContentDeepValid = false;
};

bool validateTrajectoryInsetAtomicIdentity(
    const TrajectoryInsetAtomicIdentityContract &, std::string &error);

struct TrajectoryInsetFrozenColorPair {
  std::array<float, 4> expected{{0, 0, 0, 0}};
  std::array<float, 4> observed{{0, 0, 0, 0}};
};
bool validateTrajectoryInsetFrozenPaletteColors(
    const std::vector<TrajectoryInsetFrozenColorPair> &, std::string &error);

struct TrajectoryInsetGlState {
  std::uint64_t drawFramebuffer = 0, readFramebuffer = 0;
  int viewport[4] = {0, 0, 0, 0};
  bool scissorEnabled = false;
  int scissorBox[4] = {0, 0, 0, 0};
  bool blendEnabled = false;
  std::array<bool, 8> indexedBlendEnabled{};
  bool ditherEnabled = false;
  bool sampleAlphaToOneEnabled = false;
  bool colorWriteMask[4] = {true, true, true, true};
  int blendEquationRgb = 0, blendEquationAlpha = 0, blendSrcRgb = 0,
      blendDstRgb = 0;
  int blendSrcAlpha = 0, blendDstAlpha = 0;
  bool depthTestEnabled = false;
  bool depthWriteEnabled = false;
  int depthFunction = 0;
  bool cullEnabled = false;
  int frontFace = 0;
  bool framebufferSrgb = false;
  int activeTexture = 0;
  std::uint64_t texture2d = 0, texture2dMsaa = 0, sampler = 0, program = 0;
  int touchedTextureUnit = 7;
  std::uint64_t touchedTexture2d = 0, touchedTexture2dMsaa = 0,
                touchedSampler = 0;
  std::uint64_t vertexArray = 0, arrayBuffer = 0, elementBuffer = 0,
                uniformBuffer = 0;
  std::uint64_t pixelPackBuffer = 0, pixelUnpackBuffer = 0;
  std::array<int, 8> drawBuffers{{0, 0, 0, 0, 0, 0, 0, 0}};
  int drawBufferCount = 1, readBuffer = 0, packAlignment = 4,
      unpackAlignment = 4;
  static TrajectoryInsetGlState fixture();
  bool operator==(const TrajectoryInsetGlState &) const;
};

struct TrajectoryInsetAtomicRequest {
  int width = 0, height = 0, samples = 1, semanticSamples = 0;
  std::uint64_t allocationGeneration = 0, frameSerial = 0;
  std::uint64_t cp1PackageGeneration = 0, cp2LayoutSerial = 0;
  std::uint64_t colorSerial = 0, semanticSerial = 0, toneSerial = 0;
  bool completeParentMapping = false, tokensValid = false;
  bool colorSemanticCoveragePaired = false, toneAlphaCoverageValid = false;
  bool fixedPaletteOnly = false, instrumentationEnabled = false;
  enum class ExternalResourceKind { Texture, Renderbuffer };
  struct ExternalResource {
    ExternalResourceKind kind = ExternalResourceKind::Texture;
    std::uint64_t handle = 0;
  };
  std::array<ExternalResource, 16> forbiddenMainResources{};
  std::size_t forbiddenMainResourceCount = 0;
  std::size_t instrumentationExpectedPairs = 0;
  bool insetRequested = true, worldOverlayRequested = false;
};

struct TrajectoryInsetAtomicResources {
  int width = 0, height = 0, samples = 0, semanticSamples = 0;
  std::string semanticFormat;
  std::uint64_t generation = 0, colorAttachment = 0, semanticAttachment = 0;
  std::uint64_t framebuffer = 0, resolvedColor = 0, resolvedSemantic = 0;
  std::uint64_t toneSrgba = 0, atomicCompositeStaging = 0;
  std::uint64_t colorResolveFramebuffer = 0, semanticResolveFramebuffer = 0,
                toneFramebuffer = 0, atomicCompositeStagingFramebuffer = 0;
  std::uint64_t geometryBuffer = 0, vertexArray = 0, fullscreenVertexArray = 0;
  std::uint64_t programSet = 0, semanticResolveSingleProgram = 0,
                semanticResolveMsaaProgram = 0, toneProgram = 0;
  std::uint64_t mappingGeneration = 0, probeGeneration = 0;
  std::uint64_t denominatorGeneration = 0, tokenGeneration = 0,
                cacheGeneration = 0;
  bool framebufferAttachmentMappingVerified = false;
  std::vector<std::uint64_t> handles() const;
};

bool validateTrajectoryInsetAtomicResourceIsolation(
    const TrajectoryInsetAtomicRequest &,
    const TrajectoryInsetAtomicResources &, std::string &error);

struct TrajectoryInsetAtomicLedger {
  int offscreenColorWrites = 0, offscreenSemanticWrites = 0;
  int mainHdrWrites = 0, mainWorldTokenWrites = 0, finalLdrWrites = 0,
      uiWrites = 0;
  int instrumentationPairs = 0, instrumentationColorPairs = 0,
      instrumentationSemanticPairs = 0, instrumentationCurrentTokenPairs = 0;
  std::uint64_t invalidatedResourceGeneration = 0,
                invalidatedMappingGeneration = 0,
                invalidatedProbeGeneration = 0,
                invalidatedDenominatorGeneration = 0,
                invalidatedTokenGeneration = 0, invalidatedCacheGeneration = 0;
  std::vector<std::uint64_t> destroyedOrQuarantined;
};

struct TrajectoryInsetAtomicResult {
  bool ok = false, committed = false, invalidated = false,
       stateRestored = false;
  bool stateRestoredAttempted = false, requestedInsetRetained = false;
  bool worldOverlayFallback = false, instrumentationParity = false;
  bool instrumentationEnabled = false, diagnosticPairEmitted = false;
  std::string errorCode;
  int sampleCount = 0;
  std::uint64_t resourceGeneration = 0;
  TrajectoryInsetAtomicResources resources;
  TrajectoryInsetAtomicLedger ledger;
  std::vector<std::string> orderedStates, stageLedger;
};

class TrajectoryInsetAtomicDispatch {
public:
  virtual ~TrajectoryInsetAtomicDispatch() = default;
  virtual bool invoke(TrajectoryInsetAtomicStage,
                      const TrajectoryInsetAtomicRequest &,
                      TrajectoryInsetAtomicResources &,
                      TrajectoryInsetAtomicLedger &, std::string &) = 0;
  virtual TrajectoryInsetGlState snapshotState() = 0;
  virtual bool restoreState(const TrajectoryInsetGlState &, std::string &) = 0;
  virtual unsigned readError() = 0;
  virtual void invalidate(TrajectoryInsetAtomicResources &,
                          TrajectoryInsetAtomicLedger &) = 0;
  virtual bool publishAtomicComposite(const TrajectoryInsetAtomicRequest &,
                                      const TrajectoryInsetAtomicResources &,
                                      TrajectoryInsetAtomicLedger &,
                                      std::string &) = 0;
};

class TrajectoryInsetAtomicTransaction {
public:
  explicit TrajectoryInsetAtomicTransaction(TrajectoryInsetAtomicDispatch &d)
      : dispatch_(d) {}
  TrajectoryInsetAtomicResult execute(const TrajectoryInsetAtomicRequest &);

private:
  TrajectoryInsetAtomicDispatch &dispatch_;
};
} // namespace billiardgl::render::modern
