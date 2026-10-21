#include "render/modern/trajectory_inset_atomic_transaction.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace modern = billiardgl::render::modern;
namespace {
void require(bool v, const std::string &m) {
  if (!v) {
    std::cerr << "FAIL " << m << '\n';
    std::exit(1);
  }
}
std::string readFile(const std::string &path) {
  std::ifstream input(path);
  require(input.good(), "read static adapter source " + path);
  std::ostringstream bytes;
  bytes << input.rdbuf();
  return bytes.str();
}
class Dispatch final : public modern::TrajectoryInsetAtomicDispatch {
public:
  using ResourceKind =
      modern::TrajectoryInsetAtomicRequest::ExternalResourceKind;
  static std::uint64_t resourceKey(std::uint64_t kind, std::uint64_t raw) {
    return (kind << 32u) | raw;
  }
  std::uint64_t allocate(std::uint64_t kind) {
    return resourceKey(kind, next++);
  }
  modern::TrajectoryInsetAtomicStage
      fail = modern::TrajectoryInsetAtomicStage::None,
      errorStage = modern::TrajectoryInsetAtomicStage::None,
      throwStage = modern::TrajectoryInsetAtomicStage::None,
      current = modern::TrajectoryInsetAtomicStage::None;
  modern::TrajectoryInsetGlState state =
      modern::TrajectoryInsetGlState::fixture();
  std::uint64_t next = 100;
  bool badDimensions = false, silentSampleDowngrade = false,
       badSemanticFormat = false, duplicateHandle = false,
       missingResource = false, badAttachmentMapping = false,
       preexisting = false, failPublish = false;
  bool dirtyOnPublishBeforeWrite = false, omitCurrentTokenPair = false;
  std::vector<std::string> calls;
  std::vector<std::uint64_t> invalidatedInputHandles;
  bool invoke(modern::TrajectoryInsetAtomicStage s,
              const modern::TrajectoryInsetAtomicRequest &r,
              modern::TrajectoryInsetAtomicResources &x,
              modern::TrajectoryInsetAtomicLedger &l, std::string &e) override {
    current = s;
    calls.push_back(std::string("invoke:") +
                    modern::trajectoryInsetAtomicStageName(s));
    dirty(s);
    if (s == throwStage)
      throw std::runtime_error("injected_gl_helper_exception");
    if (s == fail) {
      e = std::string("injected_") + modern::trajectoryInsetAtomicStageName(s);
      return false;
    }
    if (s == modern::TrajectoryInsetAtomicStage::FormatSampleQuery) {
      x.width = badDimensions ? r.width - 1 : r.width;
      x.height = r.height;
      x.samples = silentSampleDowngrade ? 1 : r.samples;
      x.semanticSamples =
          silentSampleDowngrade
              ? 1
              : (r.semanticSamples ? r.semanticSamples : r.samples);
      x.semanticFormat = badSemanticFormat ? "GL_R32UI" :
          "GL_RGBA8_UNORM_U24";
    }
    if (s == modern::TrajectoryInsetAtomicStage::ColorAllocation) {
      x.colorAttachment = allocate(1);
      x.resolvedColor = allocate(1);
      x.toneSrgba = allocate(1);
      x.atomicCompositeStaging = allocate(1);
    }
    if (s == modern::TrajectoryInsetAtomicStage::SemanticR32uiAllocation) {
      x.semanticAttachment = duplicateHandle ? x.colorAttachment : allocate(1);
      x.resolvedSemantic = allocate(1);
    }
    if (s == modern::TrajectoryInsetAtomicStage::GeometryUpload) {
      x.geometryBuffer = allocate(3);
      x.vertexArray = allocate(4);
      x.fullscreenVertexArray = allocate(4);
      x.programSet = allocate(5);
      x.semanticResolveSingleProgram = allocate(5);
      x.semanticResolveMsaaProgram = allocate(5);
      x.toneProgram = allocate(5);
    }
    if (s == modern::TrajectoryInsetAtomicStage::FramebufferAttachComplete) {
      x.framebuffer = allocate(2);
      x.colorResolveFramebuffer = allocate(2);
      x.semanticResolveFramebuffer = allocate(2);
      x.toneFramebuffer = allocate(2);
      x.atomicCompositeStagingFramebuffer = allocate(2);
      x.framebufferAttachmentMappingVerified = !badAttachmentMapping;
    }
    if (missingResource &&
        s == modern::TrajectoryInsetAtomicStage::FramebufferAttachComplete)
      x.resolvedSemantic = 0;
    if (s == modern::TrajectoryInsetAtomicStage::ColorClear ||
        s == modern::TrajectoryInsetAtomicStage::TableDraw ||
        s == modern::TrajectoryInsetAtomicStage::TrajectoryDraw ||
        s == modern::TrajectoryInsetAtomicStage::ColorResolve ||
        s == modern::TrajectoryInsetAtomicStage::FixedPaletteToneEncode ||
        s == modern::TrajectoryInsetAtomicStage::AtomicComposite)
      ++l.offscreenColorWrites;
    if (s == modern::TrajectoryInsetAtomicStage::SemanticClear ||
        s == modern::TrajectoryInsetAtomicStage::TrajectoryDraw ||
        s == modern::TrajectoryInsetAtomicStage::PerSampleSemanticResolve)
      ++l.offscreenSemanticWrites;
    if (s == modern::TrajectoryInsetAtomicStage::GenerationFrameSerialCheck &&
        r.instrumentationEnabled) {
      ++l.instrumentationPairs;
      ++l.instrumentationColorPairs;
      ++l.instrumentationSemanticPairs;
      if (!omitCurrentTokenPair)
        ++l.instrumentationCurrentTokenPairs;
    }
    return true;
  }
  modern::TrajectoryInsetGlState snapshotState() override {
    calls.push_back("state_snapshot");
    return state;
  }
  bool restoreState(const modern::TrajectoryInsetGlState &s,
                    std::string &e) override {
    current = modern::TrajectoryInsetAtomicStage::StateRestore;
    calls.push_back("state_restore");
    if (fail == current) {
      e = "injected_state_restore";
      return false;
    }
    state = s;
    return true;
  }
  unsigned readError() override {
    if (preexisting) {
      preexisting = false;
      return 1280;
    }
    return errorStage != modern::TrajectoryInsetAtomicStage::None &&
                   current == errorStage
               ? 1282
               : 0;
  }
  void invalidate(modern::TrajectoryInsetAtomicResources &x,
                  modern::TrajectoryInsetAtomicLedger &l) override {
    calls.push_back("destroy_or_quarantine");
    invalidatedInputHandles.clear();
    l.invalidatedResourceGeneration = x.generation;
    l.invalidatedMappingGeneration = x.mappingGeneration;
    l.invalidatedProbeGeneration = x.probeGeneration;
    l.invalidatedDenominatorGeneration = x.denominatorGeneration;
    l.invalidatedTokenGeneration = x.tokenGeneration;
    l.invalidatedCacheGeneration = x.cacheGeneration;
    for (auto h : x.handles())
      if (h) {
        invalidatedInputHandles.push_back(h);
        l.destroyedOrQuarantined.push_back(h);
      }
    x = {};
  }
  bool publishAtomicComposite(const modern::TrajectoryInsetAtomicRequest &r,
                              const modern::TrajectoryInsetAtomicResources &x,
                              modern::TrajectoryInsetAtomicLedger &l,
                              std::string &e) override {
    calls.push_back("atomic_publish_after_state_restore");
    if (failPublish) {
      e = "injected_atomic_publish_fail_before_write";
      return false;
    }
    if (dirtyOnPublishBeforeWrite) {
      dirty(modern::TrajectoryInsetAtomicStage::AtomicComposite);
      e = "injected_atomic_publish_state_mutation_before_write";
      return false;
    }
    if (x.generation != r.frameSerial || !x.atomicCompositeStaging) {
      e = "publish_stale";
      return false;
    }
    ++l.finalLdrWrites;
    ++l.uiWrites;
    return true;
  }
  void dirty(modern::TrajectoryInsetAtomicStage s) {
    auto n = 1000 + static_cast<std::uint64_t>(s);
    state.drawFramebuffer = n;
    state.readFramebuffer = n + 1;
    for (int &v : state.viewport)
      ++v;
    state.scissorEnabled = !state.scissorEnabled;
    for (int &v : state.scissorBox)
      ++v;
    state.blendEnabled = !state.blendEnabled;
    for (bool &channel : state.colorWriteMask)
      channel = !channel;
    state.blendEquationRgb++;
    state.blendEquationAlpha++;
    state.blendSrcRgb++;
    state.blendDstRgb++;
    state.blendSrcAlpha++;
    state.blendDstAlpha++;
    state.depthTestEnabled = !state.depthTestEnabled;
    state.depthWriteEnabled = !state.depthWriteEnabled;
    state.depthFunction++;
    state.cullEnabled = !state.cullEnabled;
    state.frontFace++;
    state.framebufferSrgb = !state.framebufferSrgb;
    state.activeTexture++;
    state.texture2d = n + 2;
    state.texture2dMsaa = n + 3;
    state.sampler = n + 4;
    state.touchedTextureUnit = 7;
    state.touchedTexture2d = n + 20;
    state.touchedTexture2dMsaa = n + 21;
    state.touchedSampler = n + 22;
    state.program = n + 5;
    state.vertexArray = n + 6;
    state.arrayBuffer = n + 7;
    state.elementBuffer = n + 8;
    state.uniformBuffer = n + 9;
    state.pixelPackBuffer = n + 10;
    state.pixelUnpackBuffer = n + 11;
    for (int &buffer : state.drawBuffers)
      ++buffer;
    state.drawBufferCount = 2;
    state.readBuffer++;
    state.packAlignment = 8;
    state.unpackAlignment = 8;
  }
};
modern::TrajectoryInsetAtomicRequest request(int samples, std::uint64_t serial,
                                             bool instrumentation) {
  modern::TrajectoryInsetAtomicRequest r;
  r.width = 416;
  r.height = 208;
  r.samples = samples;
  r.allocationGeneration = r.frameSerial = r.cp1PackageGeneration =
      r.cp2LayoutSerial = r.colorSerial = r.semanticSerial = r.toneSerial =
          serial;
  r.completeParentMapping = r.tokensValid = r.colorSemanticCoveragePaired =
      r.toneAlphaCoverageValid = r.fixedPaletteOnly = true;
  r.forbiddenMainResources[0] = {
      modern::TrajectoryInsetAtomicRequest::ExternalResourceKind::Texture,
      Dispatch::resourceKey(1, 9001)};
  r.forbiddenMainResources[1] = {
      modern::TrajectoryInsetAtomicRequest::ExternalResourceKind::Renderbuffer,
      Dispatch::resourceKey(6, 9002)};
  r.forbiddenMainResources[2] = {
      modern::TrajectoryInsetAtomicRequest::ExternalResourceKind::Texture,
      Dispatch::resourceKey(1, 9003)};
  r.forbiddenMainResourceCount = 3;
  r.instrumentationEnabled = instrumentation;
  r.instrumentationExpectedPairs = instrumentation ? 1u : 0u;
  return r;
}
modern::TrajectoryInsetAtomicIdentityContract identityContract() {
  modern::TrajectoryInsetAtomicIdentityContract identity;
  identity.cp1GenerationId = "v19_generation_071";
  identity.frameSerial = 71;
  identity.cp2LayoutSerial = 71;
  identity.embeddedCp1Digests = {{"p", "q", "e", "c", "s"}};
  identity.recomputedCp1Digests = identity.embeddedCp1Digests;
  identity.expectedCp1Digests = identity.embeddedCp1Digests;
  identity.recomputedCp1GeometryDigest = "geometry";
  identity.expectedCp1GeometryDigest = "geometry";
  identity.recomputedCp2LayoutDigest = "layout";
  identity.expectedCp2LayoutDigest = "layout";
  identity.sourceGroups = identity.mappingRows = identity.orderedParents = 8;
  identity.semanticMembers = identity.semanticDenominator = 92;
  identity.probes = identity.probeDenominator = identity.orderedProbeIds = 35;
  identity.generationContentDeepValid = true;
  return identity;
}
void identityNegatives() {
  std::string error;
  require(
      modern::validateTrajectoryInsetAtomicIdentity(identityContract(), error),
      "deep identity positive");
  for (int mutation = 0; mutation < 15; ++mutation) {
    auto identity = identityContract();
    if (mutation == 0)
      identity.cp1GenerationId = "v19_generation_072";
    if (mutation == 1)
      identity.frameSerial = 70;
    if (mutation == 2)
      identity.lastAttemptSerial = 71;
    if (mutation == 3)
      identity.cp2LayoutSerial = 72;
    if (mutation == 4)
      identity.embeddedCp1Digests[0] = "drift";
    if (mutation == 5)
      identity.expectedCp1Digests[4] = "drift";
    if (mutation == 6)
      identity.expectedCp1GeometryDigest = "wrong_color_or_geometry";
    if (mutation == 7)
      identity.expectedCp2LayoutDigest = "drift";
    if (mutation == 8)
      --identity.mappingRows;
    if (mutation == 9)
      --identity.orderedParents;
    if (mutation == 10)
      --identity.semanticDenominator;
    if (mutation == 11)
      --identity.probeDenominator;
    if (mutation == 12)
      --identity.orderedProbeIds;
    if (mutation == 13)
      identity.generationContentDeepValid = false;
    if (mutation == 14)
      identity.cp1GenerationId = "forged_generation_071";
    error.clear();
    require(!modern::validateTrajectoryInsetAtomicIdentity(identity, error) &&
                !error.empty(),
            "stale relabel/digest/count/deep identity mutation fails");
  }
  const std::vector<modern::TrajectoryInsetFrozenColorPair> colors{
      {{{1.0f, 0.78f, 0.08f, 1.0f}}, {{1.0f, 0.78f, 0.08f, 1.0f}}},
      {{{0.01f, 0.28f, 0.55f, 1.0f}}, {{0.01f, 0.28f, 0.55f, 1.0f}}}};
  error.clear();
  require(modern::validateTrajectoryInsetFrozenPaletteColors(colors, error),
          "frozen role palette positive");
  auto sceneDerived = colors;
  sceneDerived[0].observed = {{0.018f, 0.105f, 0.071f, 1.0f}};
  error.clear();
  require(
      !modern::validateTrajectoryInsetFrozenPaletteColors(sceneDerived, error),
      "scene-derived trajectory color fails");
  auto alphaOnly = colors;
  alphaOnly[1].observed = {{0.018f, 0.105f, 0.071f, 1.0f}};
  error.clear();
  require(!modern::validateTrajectoryInsetFrozenPaletteColors(alphaOnly, error),
          "alpha-only background false-positive fails");
}
void success() {
  const std::array<const char *, 8> states{
      {"UNALLOCATED", "ALLOCATING", "READY_CLEARED", "DRAWN",
       "COLOR_AND_SEMANTIC_RESOLVED", "FIXED_PALETTE_TONED",
       "COMPOSITE_ELIGIBLE", "COMPOSITED"}};
  for (int s : {1, 2, 4})
    for (bool i : {false, true}) {
      Dispatch d;
      auto before = d.state;
      modern::TrajectoryInsetAtomicTransaction tx(d);
      auto o = tx.execute(request(s, 1, i));
      if (!(o.ok && o.committed && o.stateRestored && d.state == before &&
            o.orderedStates.size() == states.size())) {
        std::cerr << "success detail samples=" << s << " instrumentation=" << i
                  << " ok=" << o.ok << " committed=" << o.committed
                  << " restored=" << o.stateRestored
                  << " state_equal=" << (d.state == before)
                  << " states=" << o.orderedStates.size()
                  << " error=" << o.errorCode << '\n';
      }
      require(o.ok && o.committed && o.stateRestored && d.state == before &&
                  o.orderedStates.size() == states.size(),
              "success/restored");
      for (std::size_t n = 0; n < states.size(); ++n)
        require(o.orderedStates[n] == states[n], "state order");
      require(o.resources.width == 416 && o.resources.height == 208 &&
                  o.resources.samples == s &&
                  o.resources.semanticSamples == s &&
                  o.resources.semanticFormat == "GL_RGBA8_UNORM_U24",
              "exact query metadata");
      require(o.ledger.mainHdrWrites == 0 &&
                  o.ledger.mainWorldTokenWrites == 0 &&
                  o.ledger.finalLdrWrites == 1 && o.ledger.uiWrites == 1 &&
                  o.ledger.offscreenColorWrites > 0 &&
                  o.ledger.offscreenSemanticWrites > 0,
              "dispatch observable atomic ledger");
      require(o.diagnosticPairEmitted == i && o.instrumentationParity &&
                  o.ledger.instrumentationPairs == (i ? 1 : 0) &&
                  o.ledger.instrumentationColorPairs == (i ? 1 : 0) &&
                  o.ledger.instrumentationSemanticPairs == (i ? 1 : 0) &&
                  o.ledger.instrumentationCurrentTokenPairs == (i ? 1 : 0),
              "instrumentation observable");
      const auto restoreIt =
          std::find(d.calls.begin(), d.calls.end(), "state_restore");
      const auto publishIt = std::find(d.calls.begin(), d.calls.end(),
                                       "atomic_publish_after_state_restore");
      require(restoreIt != d.calls.end() && publishIt != d.calls.end() &&
                  restoreIt < publishIt &&
                  std::count(d.calls.begin(), d.calls.end(),
                             "atomic_publish_after_state_restore") == 1,
              "success restores before exactly one state-neutral publish");
    }
}
void failures() {
  for (auto stage : modern::orderedTrajectoryInsetAtomicFailureStages()) {
    Dispatch d;
    const auto cleanState = d.state;
    modern::TrajectoryInsetAtomicTransaction tx(d);
    auto a = tx.execute(request(4, 10, false));
    d.calls.clear();
    d.fail = stage;
    auto b = tx.execute(request(4, 11, false));
    require(b.ledger.destroyedOrQuarantined == d.invalidatedInputHandles,
            "destroy/quarantine exact resource set");
    require(b.ledger.invalidatedResourceGeneration == 11 &&
                b.ledger.invalidatedMappingGeneration == 11 &&
                b.ledger.invalidatedProbeGeneration == 11 &&
                b.ledger.invalidatedDenominatorGeneration == 11 &&
                b.ledger.invalidatedTokenGeneration == 11 &&
                b.ledger.invalidatedCacheGeneration == 11,
            "failed generation mapping probe denominator token cache "
            "invalidated");
    d.fail = modern::TrajectoryInsetAtomicStage::None;
    if (stage == modern::TrajectoryInsetAtomicStage::StateRestore)
      d.state = cleanState; // Explicit external context recovery for N+2.
    auto c = tx.execute(request(4, 12, false));
    require(a.ok && c.ok && !b.ok && !b.committed && b.invalidated,
            "valid fail valid");
    require(b.ledger.mainHdrWrites == 0 && b.ledger.mainWorldTokenWrites == 0 &&
                b.ledger.finalLdrWrites == 0 && b.ledger.uiWrites == 0,
            "failed frame zero main writes");
    require(stage == modern::TrajectoryInsetAtomicStage::StateRestore
                ? !b.stateRestored
                : b.stateRestored,
            "restore semantics");
    for (auto x : a.resources.handles())
      for (auto y : c.resources.handles())
        require(x != y, "all handles disjoint");
    for (auto x : b.ledger.destroyedOrQuarantined)
      for (auto y : c.resources.handles())
        require(x != y, "failed generation handles not reused by N+2");
    require(c.resources.mappingGeneration == 12 &&
                c.resources.probeGeneration == 12 &&
                c.resources.denominatorGeneration == 12 &&
                c.resources.tokenGeneration == 12 &&
                c.resources.cacheGeneration == 12,
            "all lineage generations fresh");
    Dispatch g;
    g.errorStage = stage;
    modern::TrajectoryInsetAtomicTransaction gtx(g);
    auto e = gtx.execute(request(4, 21, false));
    require(!e.ok && !e.committed && e.invalidated &&
                e.ledger.finalLdrWrites == 0,
            "invoke true plus GL error fails");
  }

  for (bool dirtyBeforeFail : {false, true}) {
    Dispatch d;
    const auto saved = d.state;
    d.failPublish = !dirtyBeforeFail;
    d.dirtyOnPublishBeforeWrite = dirtyBeforeFail;
    modern::TrajectoryInsetAtomicTransaction tx(d);
    auto o = tx.execute(request(4, 31 + dirtyBeforeFail, false));
    require(!o.ok && !o.committed && o.invalidated && o.stateRestored &&
                d.state == saved,
            "publish failure is restored and not committed");
    require(o.ledger.mainHdrWrites == 0 && o.ledger.mainWorldTokenWrites == 0 &&
                o.ledger.finalLdrWrites == 0 && o.ledger.uiWrites == 0,
            "publish failure is fail-before-main-write");
    const auto restoreIt =
        std::find(d.calls.begin(), d.calls.end(), "state_restore");
    const auto publishIt = std::find(d.calls.begin(), d.calls.end(),
                                     "atomic_publish_after_state_restore");
    require(restoreIt != d.calls.end() && publishIt != d.calls.end() &&
                restoreIt < publishIt &&
                std::count(d.calls.begin(), d.calls.end(),
                           "atomic_publish_after_state_restore") == 1,
            "state restore precedes exactly one real publish");
  }
  for (auto stage : modern::orderedTrajectoryInsetAtomicFailureStages()) {
    if (stage == modern::TrajectoryInsetAtomicStage::StateRestore)
      continue;
    Dispatch d;
    const auto saved = d.state;
    d.throwStage = stage;
    modern::TrajectoryInsetAtomicTransaction tx(d);
    auto o = tx.execute(request(4, 40 + static_cast<int>(stage), false));
    require(!o.ok && !o.committed && o.invalidated && o.stateRestored &&
                d.state == saved && o.ledger.finalLdrWrites == 0,
            "GL helper exception restores and invalidates transaction");
  }
}
void negatives() {
  // The scene now owns additional sampled depth textures for no-MSAA and
  // replay DOF. Exercise the entire inventory, not only the original slots.
  {
    auto r = request(2, 20, true);
    r.forbiddenMainResourceCount = r.forbiddenMainResources.size();
    for (std::size_t i = 0; i < r.forbiddenMainResources.size(); ++i)
      r.forbiddenMainResources[i] = {
          modern::TrajectoryInsetAtomicRequest::ExternalResourceKind::Texture,
          Dispatch::resourceKey(1, 10000 + i)};
    Dispatch d;
    modern::TrajectoryInsetAtomicTransaction tx(d);
    require(tx.execute(r).ok, "full main-resource inventory accepts independent inset");
    for (std::size_t i = 0; i < r.forbiddenMainResources.size(); ++i) {
      auto alias = r;
      alias.forbiddenMainResources[i].handle = Dispatch::resourceKey(1, 100);
      Dispatch isolated;
      modern::TrajectoryInsetAtomicTransaction transaction(isolated);
      require(transaction.execute(alias).errorCode == "trajectory_inset_attachment_alias",
              "all main-resource slots reject depth/color texture aliasing");
    }
    r.forbiddenMainResourceCount = r.forbiddenMainResources.size() + 1;
    Dispatch isolated;
    modern::TrajectoryInsetAtomicTransaction transaction(isolated);
    require(transaction.execute(r).errorCode == "trajectory_inset_main_resource_inventory_invalid",
            "oversized resource inventory rejected before indexing");
  }
  for (int s : {0, 3, 8}) {
    Dispatch d;
    modern::TrajectoryInsetAtomicTransaction tx(d);
    require(!tx.execute(request(s, 1, false)).ok, "bad samples");
  }
  for (int kind = 0; kind < 10; ++kind) {
    auto r = request(2, 20, true);
    if (kind == 0)
      r.cp1PackageGeneration = 19;
    if (kind == 1)
      r.colorSerial = 19;
    if (kind == 2)
      r.cp2LayoutSerial = 19;
    if (kind == 3)
      r.semanticSamples = 4;
    if (kind == 4)
      r.forbiddenMainResources[0].handle = Dispatch::resourceKey(1, 100);
    if (kind == 5)
      r.forbiddenMainResourceCount = 0;
    if (kind == 6)
      r.completeParentMapping = false;
    if (kind == 7)
      r.tokensValid = false;
    if (kind == 8)
      r.fixedPaletteOnly = false;
    if (kind == 9)
      r.forbiddenMainResources[0].kind = modern::TrajectoryInsetAtomicRequest::
          ExternalResourceKind::Renderbuffer;
    Dispatch d;
    modern::TrajectoryInsetAtomicTransaction tx(d);
    const auto result = tx.execute(r);
    require(!result.ok, "input negative");
    if (kind == 4)
      require(result.errorCode == "trajectory_inset_attachment_alias",
              "same raw GL name and Texture kind aliases fail closed");
    if (kind == 9)
      require(result.errorCode == "trajectory_inset_main_resource_kind_invalid",
              "external resource type tag drift fails closed");
  }
  for (int kind = 0; kind < 6; ++kind) {
    Dispatch d;
    d.badDimensions = kind == 0;
    d.silentSampleDowngrade = kind == 1;
    d.badSemanticFormat = kind == 2;
    d.duplicateHandle = kind == 3;
    d.missingResource = kind == 4;
    d.badAttachmentMapping = kind == 5;
    modern::TrajectoryInsetAtomicTransaction tx(d);
    const auto result = tx.execute(request(4, 50 + kind, false));
    require(!result.ok, "resource metadata/missing/duplicate negative");
    if (kind == 5)
      require(result.errorCode ==
                  "trajectory_inset_framebuffer_attachment_mapping_invalid",
              "FBO attachment feedback mapping fails closed");
  }
  Dispatch p;
  p.preexisting = true;
  modern::TrajectoryInsetAtomicTransaction tx(p);
  require(!tx.execute(request(1, 60, false)).ok, "preexisting GL error");

  Dispatch instrumentationMismatch;
  instrumentationMismatch.omitCurrentTokenPair = true;
  modern::TrajectoryInsetAtomicTransaction instrumentationTx(
      instrumentationMismatch);
  require(!instrumentationTx.execute(request(4, 61, true)).ok,
          "instrumentation color semantic current-token mismatch");
}
void productionAdapterStaticContract() {
  const std::string header =
      readFile("src/Billiards/render/modern/modern_renderer.h");
  const std::string source =
      readFile("src/Billiards/render/modern/modern_renderer.cpp");
  for (const char *needle : {
           "TrajectoryInsetAtomicFrameInput",
           "prepareTrajectoryInsetAtomicFrame",
           "ModernTrajectoryInsetGlDispatch",
           "trajectory_inset_input_absent",
           "GL_RGBA8_UNORM_U24",
           "GL_RGBA32UI",
           "glTexImage2DMultisample",
           "trajectory_inset_atomic_publish_state_neutral",
           "trajectory_inset_visible_composite_deferred_to_cp4",
       })
    require(header.find(needle) != std::string::npos ||
                source.find(needle) != std::string::npos,
            std::string("production adapter contract token ") + needle);
  const auto renderBegin = source.find("void ModernRenderer::renderFrame");
  const auto captureBegin = source.find("bool ModernRenderer::capture(");
  require(renderBegin != std::string::npos && captureBegin > renderBegin,
          "renderFrame source bounds");
  const std::string renderFrame =
      source.substr(renderBegin, captureBegin - renderBegin);
  // CP4 now integrates CP3 through an explicitly supplied current-frame
  // context. Screen/product mode alone still cannot authorize the inset.
  const auto contextGate = renderFrame.find(
      "productFrame && productSession->trajectoryInsetVisibleCurrentContext");
  const auto acceptedGate = renderFrame.find(
      "if (trajectoryInsetVisibleRequest && trajectoryInsetVisibleRequest->ok)");
  const auto receiptGate = renderFrame.find("if (!chromeReceiptValidated)");
  const auto failureGate = renderFrame.find(
      "if (trajectoryInsetAcceptedFailureBlocksProductPresent)", receiptGate);
  const auto prepareCall = renderFrame.find("prepareTrajectoryInsetAtomicFrame(input)");
  require(contextGate != std::string::npos && acceptedGate > contextGate &&
              receiptGate > acceptedGate && failureGate > receiptGate &&
              prepareCall > failureGate && prepareCall != std::string::npos,
          "CP4 requires explicit current context, accepted request and validated chrome before CP3");
  require(source.find("mainHdrWrites = 0") != std::string::npos &&
              source.find("mainWorldTokenWrites = 0") != std::string::npos,
          "production adapter records zero main HDR/world-token writes");
  const auto adapterBegin =
      source.find("class ModernTrajectoryInsetGlDispatch final");
  const auto implBegin =
      source.find("struct ModernRenderer::Impl", adapterBegin);
  require(adapterBegin != std::string::npos && implBegin > adapterBegin,
          "production adapter source bounds");
  const std::string adapter =
      source.substr(adapterBegin, implBegin - adapterBegin);
  const std::array<const char *, 14> invokeStages{
      {"FormatSampleQuery", "ColorAllocation", "SemanticR32uiAllocation",
       "GeometryUpload", "FramebufferAttachComplete", "ColorClear",
       "SemanticClear", "TableDraw", "TrajectoryDraw", "ColorResolve",
       "PerSampleSemanticResolve", "FixedPaletteToneEncode",
       "GenerationFrameSerialCheck", "AtomicComposite"}};
  for (const char *stage : invokeStages)
    require(adapter.find(std::string("TrajectoryInsetAtomicStage::") + stage) !=
                std::string::npos,
            std::string("production stage call-site mapping ") + stage);
  for (const char *resourceContract :
       {"GL_RGBA16F", "GL_RGBA8_UNORM_U24", "GL_RGBA32UI", "GL_SRGB8_ALPHA8",
        "GL_TEXTURE_2D_MULTISAMPLE", "GL_FRAMEBUFFER_COMPLETE",
        "GL_RED_INTEGER", "GL_RGBA_INTEGER", "glGetTexLevelParameteriv",
        "glGetFramebufferAttachmentParameteriv",
        "framebufferAttachmentMappingVerified", "externalTextureKey",
        "externalRenderbufferKey"})
    require(adapter.find(resourceContract) != std::string::npos,
            std::string("production exact resource contract ") +
                resourceContract);
  const auto publishBegin = adapter.find("bool publishAtomicComposite(");
  const auto publishEnd =
      adapter.find("bool committedOffscreen()", publishBegin);
  require(publishBegin != std::string::npos && publishEnd > publishBegin,
          "production publish source bounds");
  const std::string publish =
      adapter.substr(publishBegin, publishEnd - publishBegin);
  for (const char *forbiddenGlCall :
       {"glBind", "glDraw", "glBlit", "glUse", "glActive", "glDelete", "glGet",
        "glClear"})
    require(publish.find(forbiddenGlCall) == std::string::npos,
            std::string("production publish is GL-state-neutral: ") +
                forbiddenGlCall);
  const auto prepareBegin =
      source.find("ModernRenderer::prepareTrajectoryInsetAtomicFrame(");
  const auto invalidateBegin = source.find(
      "ModernRenderer::invalidatePreparedTrajectoryInsetAtomicFrame",
      prepareBegin);
  require(prepareBegin != std::string::npos && invalidateBegin > prepareBegin,
          "production explicit prepare source bounds");
  const std::string prepare =
      source.substr(prepareBegin, invalidateBegin - prepareBegin);
  const auto absentCheck = prepare.find("input.cp1Package == nullptr");
  const auto dispatchConstruction =
      prepare.find("make_unique<ModernTrajectoryInsetGlDispatch>");
  require(absentCheck != std::string::npos &&
              dispatchConstruction > absentCheck,
          "absent input exits before any GL adapter construction");
  for (const char *authority :
       {"cp1GenerationId", "cp1OrderedDigests", "cp2LayoutDigest",
        "cp2LayoutSerial", "sourceGroupDenominator",
        "semanticMemberDenominator", "probeDenominator", "plotCutoutRect",
        "worldOverlayControlOn"})
    require(source.find(authority) != std::string::npos,
            std::string("explicit CP1/CP2 authority binding ") + authority);
  for (const char *lifecycle :
       {"lastTrajectoryInsetAttemptSerial",
        "validateTrajectoryInsetPackageIdentity",
        "recomputeTrajectoryInsetDigests", "trajectoryInsetLayoutDigest",
        "trajectoryInsetGeometryDigest", "expectedTrajectoryInsetColor",
        "cp1GeometryDigest", "validateInstrumentation",
        "trajectory_inset_instrumentation_member_color_mismatch",
        "forbiddenMainResources",
        "appendMainTexture(impl_->hdrDepth.id())",
        "appendMainTexture(impl_->trajectoryBackdropDepth.id())",
        "invalidatePreparedTrajectoryInsetAtomicFrame();"})
    require(source.find(lifecycle) != std::string::npos,
            std::string("production stale/deep/instrumentation lifecycle ") +
                lifecycle);
  for (const char *stateContract :
       {"GL_COLOR_WRITEMASK", "GL_DRAW_BUFFER0", "glDrawBuffers(",
        "glDisable(GL_SCISSOR_TEST)", "glDisable(GL_CULL_FACE)",
        "glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)"})
    require(source.find(stateContract) != std::string::npos,
            std::string("production normalized/restored GL state ") +
                stateContract);
  require(std::count(source.begin(), source.end(), '\0') == 0,
          "adapter source is text");
  std::size_t lifecycleCalls = 0, position = 0;
  while (
      (position = source.find("invalidatePreparedTrajectoryInsetAtomicFrame();",
                              position)) != std::string::npos) {
    ++lifecycleCalls;
    ++position;
  }
  require(lifecycleCalls >= 2,
          "resize and context loss invalidate prepared generation");
}
} // namespace
int main() {
  success();
  failures();
  negatives();
  identityNegatives();
  productionAdapterStaticContract();
  std::cout << "PASS phase4_v19_cp3_offscreen_atomic success=6 "
               "valid_fail_valid=15 gl_error_failures=15 publish_failures=2 "
               "input_negatives=18 "
               "full_resource_inventory_positive=1 resource_alias_negatives=16 inventory_overflow_negative=1 "
               "red_status=RED_UNVERIFIABLE no_gpu=1\n";
}
