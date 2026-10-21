#include "render/modern/trajectory_inset_atomic_transaction.h"
#include <algorithm>
#include <array>

namespace billiardgl::render::modern {
namespace {
constexpr std::array<TrajectoryInsetAtomicStage, 15> kStages{
    {TrajectoryInsetAtomicStage::FormatSampleQuery,
     TrajectoryInsetAtomicStage::ColorAllocation,
     TrajectoryInsetAtomicStage::SemanticR32uiAllocation,
     TrajectoryInsetAtomicStage::GeometryUpload,
     TrajectoryInsetAtomicStage::FramebufferAttachComplete,
     TrajectoryInsetAtomicStage::ColorClear,
     TrajectoryInsetAtomicStage::SemanticClear,
     TrajectoryInsetAtomicStage::TableDraw,
     TrajectoryInsetAtomicStage::TrajectoryDraw,
     TrajectoryInsetAtomicStage::ColorResolve,
     TrajectoryInsetAtomicStage::PerSampleSemanticResolve,
     TrajectoryInsetAtomicStage::FixedPaletteToneEncode,
     TrajectoryInsetAtomicStage::GenerationFrameSerialCheck,
     TrajectoryInsetAtomicStage::AtomicComposite,
     TrajectoryInsetAtomicStage::StateRestore}};
bool valid(const TrajectoryInsetAtomicRequest &r, std::string &e) {
  const int ss = r.semanticSamples ? r.semanticSamples : r.samples;
  if (r.width <= 0 || r.height <= 0 ||
      (r.samples != 1 && r.samples != 2 && r.samples != 4) || ss != r.samples)
    e = "trajectory_inset_sample_or_size_invalid";
  else if (!r.allocationGeneration || r.allocationGeneration != r.frameSerial ||
           r.cp1PackageGeneration != r.frameSerial ||
           r.cp2LayoutSerial != r.frameSerial ||
           r.colorSerial != r.frameSerial ||
           r.semanticSerial != r.frameSerial || r.toneSerial != r.frameSerial)
    e = "trajectory_inset_serial_mismatch";
  else if (!r.completeParentMapping)
    e = "trajectory_inset_mapping_incomplete";
  else if (!r.tokensValid)
    e = "trajectory_inset_token_inventory_invalid";
  else if (!r.colorSemanticCoveragePaired || !r.toneAlphaCoverageValid)
    e = "trajectory_inset_color_semantic_pairing_invalid";
  else if (!r.fixedPaletteOnly)
    e = "trajectory_inset_palette_not_fixed";
  else if (r.forbiddenMainResourceCount == 0 ||
           r.forbiddenMainResourceCount > r.forbiddenMainResources.size())
    e = "trajectory_inset_main_resource_inventory_invalid";
  else if (!r.insetRequested || r.worldOverlayRequested)
    e = "trajectory_inset_request_authority_invalid";
  else if (!r.instrumentationEnabled && r.instrumentationExpectedPairs != 0)
    e = "trajectory_inset_instrumentation_request_invalid";
  return e.empty();
}
bool complete(const TrajectoryInsetAtomicResources &r) {
  auto h = r.handles();
  h.erase(std::remove(h.begin(), h.end(), 0), h.end());
  auto u = h;
  std::sort(u.begin(), u.end());
  u.erase(std::unique(u.begin(), u.end()), u.end());
  return r.width > 0 && r.height > 0 &&
         (r.samples == 1 || r.samples == 2 || r.samples == 4) &&
         r.semanticSamples == r.samples &&
         r.semanticFormat == "GL_RGBA8_UNORM_U24" &&
         h.size() == 18 && u.size() == h.size() &&
         r.mappingGeneration == r.generation &&
         r.probeGeneration == r.generation &&
         r.denominatorGeneration == r.generation &&
         r.tokenGeneration == r.generation && r.cacheGeneration == r.generation;
}
} // namespace

bool validateTrajectoryInsetAtomicIdentity(
    const TrajectoryInsetAtomicIdentityContract &identity, std::string &error) {
  constexpr const char kGenerationPrefix[] = "v19_generation_";
  const std::size_t separator = identity.cp1GenerationId.rfind('_');
  std::uint64_t generationSerial = 0;
  bool canonicalGeneration =
      identity.cp1GenerationId.rfind(kGenerationPrefix, 0) == 0 &&
      separator == sizeof(kGenerationPrefix) - 2 &&
      separator + 1 < identity.cp1GenerationId.size();
  if (canonicalGeneration) {
    for (std::size_t index = separator + 1;
         index < identity.cp1GenerationId.size(); ++index) {
      const char digit = identity.cp1GenerationId[index];
      if (digit < '0' || digit > '9') {
        canonicalGeneration = false;
        break;
      }
      const std::uint64_t next =
          generationSerial * 10u + static_cast<std::uint64_t>(digit - '0');
      if (next < generationSerial) {
        canonicalGeneration = false;
        break;
      }
      generationSerial = next;
    }
  }
  if (!canonicalGeneration || identity.frameSerial == 0 ||
      generationSerial != identity.frameSerial ||
      identity.cp2LayoutSerial != identity.frameSerial ||
      identity.frameSerial <= identity.lastAttemptSerial) {
    error = "trajectory_inset_generation_or_serial_identity_invalid";
  } else if (identity.embeddedCp1Digests != identity.recomputedCp1Digests ||
             identity.expectedCp1Digests != identity.recomputedCp1Digests ||
             identity.recomputedCp1GeometryDigest.empty() ||
             identity.expectedCp1GeometryDigest !=
                 identity.recomputedCp1GeometryDigest ||
             identity.recomputedCp2LayoutDigest.empty() ||
             identity.expectedCp2LayoutDigest !=
                 identity.recomputedCp2LayoutDigest) {
    error = "trajectory_inset_content_digest_identity_invalid";
  } else if (identity.sourceGroups == 0 ||
             identity.sourceGroups != identity.mappingRows ||
             identity.sourceGroups != identity.orderedParents ||
             identity.semanticMembers == 0 ||
             identity.semanticMembers != identity.semanticDenominator ||
             identity.probes == 0 ||
             identity.probes != identity.probeDenominator ||
             identity.probes != identity.orderedProbeIds ||
             !identity.generationContentDeepValid) {
    error = "trajectory_inset_content_inventory_identity_invalid";
  }
  return error.empty();
}

bool validateTrajectoryInsetAtomicResourceIsolation(
    const TrajectoryInsetAtomicRequest &request,
    const TrajectoryInsetAtomicResources &resources, std::string &error) {
  const std::vector<std::uint64_t> insetResources = resources.handles();
  if (std::find(insetResources.begin(), insetResources.end(), 0) !=
      insetResources.end()) {
    error = "trajectory_inset_resource_inventory_incomplete";
    return false;
  }
  auto uniqueResources = insetResources;
  std::sort(uniqueResources.begin(), uniqueResources.end());
  if (std::adjacent_find(uniqueResources.begin(), uniqueResources.end()) !=
      uniqueResources.end()) {
    error = "trajectory_inset_framebuffer_feedback";
    return false;
  }
  for (std::size_t index = 0; index < request.forbiddenMainResourceCount;
       ++index) {
    const auto &main = request.forbiddenMainResources[index];
    if (main.handle == 0) {
      error = "trajectory_inset_main_resource_inventory_invalid";
      return false;
    }
    const std::uint64_t expectedKind =
        main.kind == TrajectoryInsetAtomicRequest::ExternalResourceKind::Texture
            ? 1u
            : 6u;
    if ((main.handle >> 32u) != expectedKind) {
      error = "trajectory_inset_main_resource_kind_invalid";
      return false;
    }
    if (std::find(insetResources.begin(), insetResources.end(), main.handle) !=
        insetResources.end()) {
      error = "trajectory_inset_attachment_alias";
      return false;
    }
  }
  if (!resources.framebufferAttachmentMappingVerified) {
    error = "trajectory_inset_framebuffer_attachment_mapping_invalid";
    return false;
  }
  return true;
}

bool validateTrajectoryInsetFrozenPaletteColors(
    const std::vector<TrajectoryInsetFrozenColorPair> &pairs,
    std::string &error) {
  if (pairs.empty()) {
    error = "trajectory_inset_palette_color_inventory_empty";
    return false;
  }
  for (const auto &pair : pairs) {
    if (pair.expected[3] != 1.0f || pair.observed[3] != 1.0f ||
        pair.expected != pair.observed) {
      error = "trajectory_inset_palette_role_color_mismatch";
      return false;
    }
  }
  return true;
}

const char *trajectoryInsetAtomicStageName(TrajectoryInsetAtomicStage s) {
  switch (s) {
  case TrajectoryInsetAtomicStage::None:
    return "none";
  case TrajectoryInsetAtomicStage::FormatSampleQuery:
    return "format_sample_query";
  case TrajectoryInsetAtomicStage::ColorAllocation:
    return "color_allocation";
  case TrajectoryInsetAtomicStage::SemanticR32uiAllocation:
    return "semantic_r32ui_allocation";
  case TrajectoryInsetAtomicStage::GeometryUpload:
    return "geometry_upload";
  case TrajectoryInsetAtomicStage::FramebufferAttachComplete:
    return "framebuffer_attach_complete";
  case TrajectoryInsetAtomicStage::ColorClear:
    return "color_clear";
  case TrajectoryInsetAtomicStage::SemanticClear:
    return "semantic_clear";
  case TrajectoryInsetAtomicStage::TableDraw:
    return "table_draw";
  case TrajectoryInsetAtomicStage::TrajectoryDraw:
    return "trajectory_draw";
  case TrajectoryInsetAtomicStage::ColorResolve:
    return "color_resolve";
  case TrajectoryInsetAtomicStage::PerSampleSemanticResolve:
    return "per_sample_semantic_resolve";
  case TrajectoryInsetAtomicStage::FixedPaletteToneEncode:
    return "fixed_palette_tone_encode";
  case TrajectoryInsetAtomicStage::GenerationFrameSerialCheck:
    return "generation_frame_serial_check";
  case TrajectoryInsetAtomicStage::AtomicComposite:
    return "atomic_composite";
  case TrajectoryInsetAtomicStage::StateRestore:
    return "state_restore";
  }
  return "unknown";
}
std::vector<TrajectoryInsetAtomicStage>
orderedTrajectoryInsetAtomicFailureStages() {
  return {kStages.begin(), kStages.end()};
}
std::vector<std::uint64_t> TrajectoryInsetAtomicResources::handles() const {
  return {colorAttachment,
          semanticAttachment,
          framebuffer,
          resolvedColor,
          resolvedSemantic,
          toneSrgba,
          atomicCompositeStaging,
          colorResolveFramebuffer,
          semanticResolveFramebuffer,
          toneFramebuffer,
          atomicCompositeStagingFramebuffer,
          geometryBuffer,
          vertexArray,
          fullscreenVertexArray,
          programSet,
          semanticResolveSingleProgram,
          semanticResolveMsaaProgram,
          toneProgram};
}
TrajectoryInsetGlState TrajectoryInsetGlState::fixture() {
  TrajectoryInsetGlState s;
  s.drawFramebuffer = 11;
  s.readFramebuffer = 12;
  s.viewport[0] = 3;
  s.viewport[1] = 4;
  s.viewport[2] = 1900;
  s.viewport[3] = 1000;
  s.scissorEnabled = true;
  s.scissorBox[0] = 5;
  s.scissorBox[1] = 6;
  s.scissorBox[2] = 1800;
  s.scissorBox[3] = 900;
  s.blendEnabled = true;
  s.indexedBlendEnabled = {{true, false, true, false, true, false, true, false}};
  s.ditherEnabled = true;
  s.sampleAlphaToOneEnabled = true;
  s.colorWriteMask[0] = true;
  s.colorWriteMask[1] = false;
  s.colorWriteMask[2] = true;
  s.colorWriteMask[3] = false;
  s.blendEquationRgb = 1;
  s.blendEquationAlpha = 2;
  s.blendSrcRgb = 3;
  s.blendDstRgb = 4;
  s.blendSrcAlpha = 5;
  s.blendDstAlpha = 6;
  s.depthTestEnabled = true;
  s.depthWriteEnabled = true;
  s.depthFunction = 7;
  s.cullEnabled = true;
  s.frontFace = 8;
  s.framebufferSrgb = true;
  s.activeTexture = 9;
  s.texture2d = 10;
  s.texture2dMsaa = 13;
  s.sampler = 14;
  s.touchedTextureUnit = 7;
  s.touchedTexture2d = 24;
  s.touchedTexture2dMsaa = 25;
  s.touchedSampler = 26;
  s.program = 15;
  s.vertexArray = 16;
  s.arrayBuffer = 17;
  s.elementBuffer = 18;
  s.uniformBuffer = 19;
  s.pixelPackBuffer = 20;
  s.pixelUnpackBuffer = 21;
  s.drawBuffers = {{22, 24, 0, 0, 0, 0, 0, 0}};
  s.drawBufferCount = 2;
  s.readBuffer = 23;
  s.packAlignment = 1;
  s.unpackAlignment = 2;
  return s;
}
bool TrajectoryInsetGlState::operator==(const TrajectoryInsetGlState &o) const {
  return drawFramebuffer == o.drawFramebuffer &&
         readFramebuffer == o.readFramebuffer &&
         std::equal(std::begin(viewport), std::end(viewport),
                    std::begin(o.viewport)) &&
         scissorEnabled == o.scissorEnabled &&
         std::equal(std::begin(scissorBox), std::end(scissorBox),
                    std::begin(o.scissorBox)) &&
         blendEnabled == o.blendEnabled &&
         indexedBlendEnabled == o.indexedBlendEnabled &&
         ditherEnabled == o.ditherEnabled &&
         sampleAlphaToOneEnabled == o.sampleAlphaToOneEnabled &&
         std::equal(std::begin(colorWriteMask), std::end(colorWriteMask),
                    std::begin(o.colorWriteMask)) &&
         blendEquationRgb == o.blendEquationRgb &&
         blendEquationAlpha == o.blendEquationAlpha &&
         blendSrcRgb == o.blendSrcRgb && blendDstRgb == o.blendDstRgb &&
         blendSrcAlpha == o.blendSrcAlpha && blendDstAlpha == o.blendDstAlpha &&
         depthTestEnabled == o.depthTestEnabled &&
         depthWriteEnabled == o.depthWriteEnabled &&
         depthFunction == o.depthFunction && cullEnabled == o.cullEnabled &&
         frontFace == o.frontFace && framebufferSrgb == o.framebufferSrgb &&
         activeTexture == o.activeTexture && texture2d == o.texture2d &&
         texture2dMsaa == o.texture2dMsaa && sampler == o.sampler &&
         touchedTextureUnit == o.touchedTextureUnit &&
         touchedTexture2d == o.touchedTexture2d &&
         touchedTexture2dMsaa == o.touchedTexture2dMsaa &&
         touchedSampler == o.touchedSampler && program == o.program &&
         vertexArray == o.vertexArray && arrayBuffer == o.arrayBuffer &&
         elementBuffer == o.elementBuffer && uniformBuffer == o.uniformBuffer &&
         pixelPackBuffer == o.pixelPackBuffer &&
         pixelUnpackBuffer == o.pixelUnpackBuffer &&
         drawBuffers == o.drawBuffers && drawBufferCount == o.drawBufferCount &&
         readBuffer == o.readBuffer && packAlignment == o.packAlignment &&
         unpackAlignment == o.unpackAlignment;
}
TrajectoryInsetAtomicResult TrajectoryInsetAtomicTransaction::execute(
    const TrajectoryInsetAtomicRequest &r) {
  TrajectoryInsetAtomicResult o;
  o.sampleCount = r.samples;
  o.resourceGeneration = r.allocationGeneration;
  o.requestedInsetRetained = r.insetRequested;
  o.instrumentationEnabled = r.instrumentationEnabled;
  o.orderedStates = {"UNALLOCATED"};
  const auto saved = dispatch_.snapshotState();
  auto restore = [&] {
    o.stateRestoredAttempted = true;
    std::string e;
    o.stateRestored =
        dispatch_.restoreState(saved, e) && dispatch_.readError() == 0;
    if (!o.stateRestored)
      o.errorCode = e.empty() ? "trajectory_inset_state_restore_failed" : e;
  };
  auto invalidate = [&] {
    dispatch_.invalidate(o.resources, o.ledger);
    o.invalidated = true;
    o.committed = false;
    if (!o.stateRestoredAttempted || !(dispatch_.snapshotState() == saved))
      restore();
    o.orderedStates.push_back("INVALIDATED_NO_COMPOSITE");
  };
  if (dispatch_.readError()) {
    o.errorCode = "trajectory_inset_preexisting_gl_error";
    invalidate();
    return o;
  }
  if (!valid(r, o.errorCode)) {
    invalidate();
    return o;
  }
  o.resources.generation = r.allocationGeneration;
  o.resources.mappingGeneration = o.resources.probeGeneration =
      o.resources.denominatorGeneration = o.resources.tokenGeneration =
          o.resources.cacheGeneration = r.allocationGeneration;
  o.orderedStates.push_back("ALLOCATING");
  for (auto s : kStages) {
    if (s == TrajectoryInsetAtomicStage::StateRestore)
      break;
    std::string e;
    o.stageLedger.push_back(trajectoryInsetAtomicStageName(s));
    bool invoked = false;
    unsigned stageError = 0;
    try {
      invoked = dispatch_.invoke(s, r, o.resources, o.ledger, e);
      stageError = dispatch_.readError();
    } catch (const std::exception &exception) {
      e = std::string("trajectory_inset_") + trajectoryInsetAtomicStageName(s) +
          "_exception:" + exception.what();
    } catch (...) {
      e = std::string("trajectory_inset_") + trajectoryInsetAtomicStageName(s) +
          "_unknown_exception";
    }
    if (!invoked || stageError != 0) {
      o.errorCode = e.empty()
                        ? std::string("trajectory_inset_") +
                              trajectoryInsetAtomicStageName(s) + "_failed"
                        : e;
      invalidate();
      return o;
    }
    if (s == TrajectoryInsetAtomicStage::FramebufferAttachComplete) {
      std::string isolationError;
      if (!complete(o.resources) ||
          !validateTrajectoryInsetAtomicResourceIsolation(r, o.resources,
                                                          isolationError) ||
          o.resources.width != r.width || o.resources.height != r.height ||
          o.resources.samples != r.samples ||
          o.resources.semanticSamples !=
              (r.semanticSamples ? r.semanticSamples : r.samples)) {
        o.errorCode = isolationError.empty()
                          ? "trajectory_inset_resource_set_incomplete"
                          : isolationError;
        invalidate();
        return o;
      }
    }
    if (s == TrajectoryInsetAtomicStage::SemanticClear)
      o.orderedStates.push_back("READY_CLEARED");
    else if (s == TrajectoryInsetAtomicStage::TrajectoryDraw)
      o.orderedStates.push_back("DRAWN");
    else if (s == TrajectoryInsetAtomicStage::PerSampleSemanticResolve)
      o.orderedStates.push_back("COLOR_AND_SEMANTIC_RESOLVED");
    else if (s == TrajectoryInsetAtomicStage::FixedPaletteToneEncode)
      o.orderedStates.push_back("FIXED_PALETTE_TONED");
    else if (s == TrajectoryInsetAtomicStage::GenerationFrameSerialCheck) {
      const int expectedPairs =
          r.instrumentationEnabled
              ? static_cast<int>(r.instrumentationExpectedPairs)
              : 0;
      o.instrumentationParity =
          complete(o.resources) && o.ledger.offscreenColorWrites > 0 &&
          o.ledger.offscreenSemanticWrites > 0 &&
          o.ledger.instrumentationPairs == expectedPairs &&
          o.ledger.instrumentationColorPairs == expectedPairs &&
          o.ledger.instrumentationSemanticPairs == expectedPairs &&
          o.ledger.instrumentationCurrentTokenPairs == expectedPairs;
      o.diagnosticPairEmitted = r.instrumentationEnabled;
      if (!o.instrumentationParity) {
        o.errorCode = "trajectory_inset_instrumentation_parity_invalid";
        invalidate();
        return o;
      }
      o.orderedStates.push_back("COMPOSITE_ELIGIBLE");
    }
  }
  restore();
  if (!o.stateRestored) {
    invalidate();
    return o;
  }
  std::string e;
  o.stageLedger.push_back("atomic_publish_after_state_restore");
  bool published = false;
  try {
    published = dispatch_.publishAtomicComposite(r, o.resources, o.ledger, e);
  } catch (const std::exception &exception) {
    e = std::string("trajectory_inset_atomic_publish_exception:") +
        exception.what();
  } catch (...) {
    e = "trajectory_inset_atomic_publish_unknown_exception";
  }
  if (!published) {
    o.errorCode = e.empty() ? "trajectory_inset_atomic_publish_failed" : e;
    invalidate();
    return o;
  }
  o.committed = true;
  o.ok = true;
  o.orderedStates.push_back("COMPOSITED");
  return o;
}
} // namespace billiardgl::render::modern
