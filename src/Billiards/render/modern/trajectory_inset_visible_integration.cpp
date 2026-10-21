#include "render/modern/trajectory_inset_visible_integration.h"
#include "render/ui/product_ui_frame_coordinator.h"

#include "deterministic_hash.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>

namespace billiardgl::render::modern {
namespace {

template <typename Value>
void appendBits(std::string &output, const Value &value) {
  output.append(reinterpret_cast<const char *>(&value), sizeof(value));
}

void appendField(std::string &output, const std::string &value) {
  const std::uint64_t length = value.size();
  appendBits(output, length);
  output += value;
}

std::string
geometryDigest(const training::TrajectoryInsetDrawPackage &package) {
  std::string canonical;
  for (const auto &vertex : package.vertices) {
    appendBits(canonical, vertex.xPhysicalPx);
    appendBits(canonical, vertex.yPhysicalPx);
    for (const float channel : vertex.linearRgba)
      appendBits(canonical, channel);
    appendBits(canonical, vertex.insetOwnershipToken);
  }
  for (const auto &batch : package.semanticBatches) {
    appendField(canonical, batch.parentSourceKey);
    appendField(canonical, batch.primitiveId);
    appendField(canonical, batch.arcSegmentId);
    appendField(canonical, batch.insetMemberKey);
    appendBits(canonical, batch.track);
    appendBits(canonical, batch.role);
    appendBits(canonical, batch.firstVertex);
    appendBits(canonical, batch.vertexCount);
    appendBits(canonical, batch.insetOwnershipToken);
  }
  return sha256Hex(canonical);
}

std::string layoutDigest(const ui::TrajectoryInsetShellLayout &layout) {
  std::string canonical;
  const auto rect = [&](const product_hud::HudRect &value) {
    appendBits(canonical, value.x);
    appendBits(canonical, value.y);
    appendBits(canonical, value.width);
    appendBits(canonical, value.height);
  };
  appendField(canonical, layout.profileId);
  appendBits(canonical, layout.insetVisible);
  appendBits(canonical, layout.insetControlOn);
  appendBits(canonical, layout.worldOverlayVisible);
  appendBits(canonical, layout.worldOverlayControlOn);
  rect(layout.authoritativePanelRect);
  rect(layout.panelRect);
  rect(layout.plotCutoutRect);
  rect(layout.headerRect);
  rect(layout.titleRect);
  for (const auto &value : layout.orientationLabelRects)
    rect(value);
  for (const auto &value : layout.legendRects)
    rect(value);
  rect(layout.toggleRect);
  rect(layout.toggleLabelRect);
  rect(layout.focusRect);
  for (const auto &value : layout.borderRects)
    rect(value);
  appendField(canonical, layout.title);
  appendField(canonical, layout.toggleLabel);
  for (const auto &value : layout.orientationLabels)
    appendField(canonical, value);
  for (const auto &value : layout.legendEntries)
    appendField(canonical, value);
  return sha256Hex(canonical);
}

std::string
exclusionDigest(const ui::TrajectoryInsetShellRequest &request) {
  std::string canonical;
  appendBits(canonical, request.exclusions.size());
  for (const auto &exclusion : request.exclusions) {
    appendField(canonical, exclusion.id);
    appendBits(canonical, exclusion.active);
    appendBits(canonical, exclusion.rect.x);
    appendBits(canonical, exclusion.rect.y);
    appendBits(canonical, exclusion.rect.width);
    appendBits(canonical, exclusion.rect.height);
  }
  return sha256Hex(canonical);
}

void ledgerField(std::string &output, const std::string &value) {
  output += std::to_string(value.size()) + ":" + value + "\n";
}

std::string canonicalDoubleBits(double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::hex << std::setfill('0') << std::setw(16) << bits;
  return output.str();
}

std::array<std::string, 5>
orderedDigests(const training::TrajectoryInsetDrawPackage &package) {
  std::string parents, probes, events, contributors, intervals;
  for (const auto &key : package.orderedParentSourceKeys)
    ledgerField(parents, key);
  for (const auto &probe : package.probes) {
    for (const auto &value :
         {probe.probeId, probe.parentSourceKey, probe.insetMemberKey,
          probe.companionInsetMemberKey})
      ledgerField(probes, value);
    ledgerField(probes, std::to_string(probe.insetOwnershipToken));
    ledgerField(probes, std::to_string(probe.strokeXPhysicalPx));
    ledgerField(probes, std::to_string(probe.strokeYPhysicalPx));
    ledgerField(probes, std::to_string(probe.companionXPhysicalPx));
    ledgerField(probes, std::to_string(probe.companionYPhysicalPx));
    ledgerField(probes, std::to_string(probe.backgroundXPhysicalPx));
    ledgerField(probes, std::to_string(probe.backgroundYPhysicalPx));
  }
  for (const auto &event : package.predictedDashEvents) {
    ledgerField(events, event.eventKey);
    ledgerField(events, std::to_string(event.routeWideDashEventOrdinal));
    ledgerField(events, std::to_string(event.routeSpanOrdinal));
    ledgerField(events, canonicalDoubleBits(event.routeDistanceStartPx));
    ledgerField(events, canonicalDoubleBits(event.routeDistanceEndPx));
    for (std::size_t role = 0; role < event.orderedRoleMemberKeys.size();
         ++role) {
      ledgerField(events, event.orderedRoleMemberKeys[role]);
      ledgerField(events, std::to_string(event.orderedRoleTokens[role]));
    }
    for (const auto &contributor : event.orderedContributors) {
      for (const auto &value : {event.eventKey, contributor.parentSourceKey,
                                contributor.arcSegmentId,
                                std::to_string(contributor.routeOrderOrdinal)})
        ledgerField(contributors, value);
      ledgerField(contributors,
                  canonicalDoubleBits(contributor.sourceLocalTStart));
      ledgerField(contributors,
                  canonicalDoubleBits(contributor.sourceLocalTEnd));
      ledgerField(contributors,
                  canonicalDoubleBits(contributor.eventRouteDistanceStartPx));
      ledgerField(contributors,
                  canonicalDoubleBits(contributor.eventRouteDistanceEndPx));
    }
  }
  for (const auto &row : package.mappingRows) {
    if (row.orderedPredictedRouteIntervals.empty())
      continue;
    ledgerField(intervals, row.parentSourceKey);
    ledgerField(intervals,
                std::to_string(row.orderedPredictedRouteIntervals.size()));
    for (const auto &interval : row.orderedPredictedRouteIntervals) {
      ledgerField(intervals, interval.arcSegmentId);
      ledgerField(intervals, std::to_string(interval.routeOrderOrdinal));
      ledgerField(intervals, std::to_string(interval.routeSpanOrdinal));
      ledgerField(intervals, canonicalDoubleBits(interval.sourceLocalTStart));
      ledgerField(intervals, canonicalDoubleBits(interval.sourceLocalTEnd));
      ledgerField(intervals,
                  canonicalDoubleBits(interval.routeDistanceStartPx));
      ledgerField(intervals, canonicalDoubleBits(interval.routeDistanceEndPx));
      ledgerField(intervals, interval.dashEventKey);
      ledgerField(intervals, interval.nonRenderedReason);
    }
  }
  return {{sha256Hex(parents), sha256Hex(probes), sha256Hex(events),
           sha256Hex(contributors), sha256Hex(intervals)}};
}

bool acceptedScreen(ProductScreen screen) {
  return screen == ProductScreen::TrainingSession ||
         screen == ProductScreen::ReplayPlayback;
}

bool sameRect(const product_hud::HudRect &a, const product_hud::HudRect &b) {
  const auto close = [](float x, float y) { return std::abs(x - y) <= 1e-4f; };
  return close(a.x, b.x) && close(a.y, b.y) && close(a.width, b.width) &&
         close(a.height, b.height);
}

bool deepPackageValid(const training::TrajectoryInsetDrawPackage &package) {
  if (package.sourceGroupDenominator != package.mappingRows.size() ||
      package.semanticMemberDenominator != package.semanticBatches.size() ||
      package.probeDenominator != package.probes.size() ||
      package.orderedProbeIds.size() != package.probes.size() ||
      !package.cameraIndependent || !package.fixedPanelBackground ||
      !package.worldVisibilityIgnored ||
      !package.orientationLabelsExcludedFromDenominator)
    return false;
  for (const auto &batch : package.semanticBatches) {
    if (batch.parentSourceKey.empty() || batch.primitiveId.empty() ||
        batch.arcSegmentId.empty() || batch.insetMemberKey.empty() ||
        batch.insetOwnershipToken == 0 || batch.vertexCount == 0 ||
        batch.firstVertex > package.vertices.size() ||
        batch.vertexCount > package.vertices.size() - batch.firstVertex)
      return false;
    for (std::size_t index = batch.firstVertex;
         index < batch.firstVertex + batch.vertexCount; ++index)
      if (package.vertices[index].insetOwnershipToken !=
          batch.insetOwnershipToken)
        return false;
  }
  for (std::size_t index = 0; index < package.probes.size(); ++index)
    if (package.probes[index].probeId != package.orderedProbeIds[index])
      return false;
  return true;
}

bool exactExclusionInventory(const ui::TrajectoryInsetShellRequest &request) {
  constexpr std::array<const char *, 5> expected{{
      "player status HUD",
      "shot power and tip controls",
      "rules decision panel or modal",
      "caption and accessibility live-region band",
      "safe-area edges and product footer",
  }};
  if (request.exclusions.size() != expected.size())
    return false;
  for (std::size_t index = 0; index < expected.size(); ++index)
    if (request.exclusions[index].id != expected[index])
      return false;
  return true;
}

bool auditCurrentRequest(const TrajectoryInsetVisibleFrameRequest &request,
                         std::string &error) {
  if (!request.present || !request.sessionStable || request.transitioning ||
      request.errored || request.modalVisible || !request.insetToggleOn ||
      request.worldOverlayToggleOn || !acceptedScreen(request.screen)) {
    error = "trajectory_inset_visible_authority_invalid";
    return false;
  }
  if (request.sessionSerial == 0 || request.frameSerial == 0 ||
      request.layoutSerial == 0 ||
      request.sessionSerial != request.frameSerial ||
      request.layoutSerial != request.frameSerial ||
      (request.samples != 1 && request.samples != 2 && request.samples != 4)) {
    error = "trajectory_inset_visible_serial_or_sample_invalid";
    return false;
  }
  if (!request.package || !request.shellRequest || !request.layout ||
      !request.chrome) {
    error = "trajectory_inset_visible_input_missing";
    return false;
  }
  const auto &package = *request.package;
  if (!package.ok || !deepPackageValid(package) ||
      package.generationId != request.cp1GenerationId ||
      package.palette != request.palette ||
      orderedDigests(package) != request.cp1Digests ||
      geometryDigest(package) != request.cp1GeometryDigest) {
    error = "trajectory_inset_visible_cp1_identity_invalid";
    return false;
  }
  const auto rebuilt =
      ui::buildTrajectoryInsetShellLayout(*request.shellRequest);
  const auto layoutAudit = ui::auditTrajectoryInsetShellLayout(
      *request.shellRequest, *request.layout);
  const auto chromeAudit =
      ui::auditTrajectoryInsetChromeDrawList(*request.layout, *request.chrome);
  if (!exactExclusionInventory(*request.shellRequest) || !rebuilt.ok ||
      !layoutAudit.ok || !chromeAudit.ok ||
      request.layout->profileId != request.cp2ProfileId ||
      layoutDigest(*request.layout) != request.cp2LayoutDigest ||
      exclusionDigest(*request.shellRequest) != request.cp2ExclusionDigest ||
      !sameRect(rebuilt.plotCutoutRect, request.layout->plotCutoutRect) ||
      request.layout->trajectorySemanticMemberCount != 0 ||
      request.layout->trajectoryProbeCount != 0 ||
      request.layout->trajectoryDenominatorContribution != 0) {
    error = "trajectory_inset_visible_cp2_identity_invalid";
    return false;
  }
  const auto &plot = package.layout.plotRect;
  const auto &cutout = request.layout->plotCutoutRect;
  if (std::abs(plot.x - cutout.x) > 1e-4f ||
      std::abs(plot.y - cutout.y) > 1e-4f ||
      std::abs(plot.width - cutout.width) > 1e-4f ||
      std::abs(plot.height - cutout.height) > 1e-4f) {
    error = "trajectory_inset_visible_cp1_cp2_geometry_mismatch";
    return false;
  }
  return true;
}

bool exactPreparedIdentity(const TrajectoryInsetVisibleFrameRequest &request,
                           const TrajectoryInsetVisibleWorkingSet &working) {
  return working.preparation.ok && working.preparation.committedOffscreen &&
         !working.preparation.visibleCompositePerformed &&
         working.preparation.stateRestored &&
         working.preparation.frameSerial == request.frameSerial &&
         working.preparation.resourceGeneration == request.frameSerial &&
         working.cp3StagingResource != 0 &&
         working.preparedSessionSerial == request.sessionSerial &&
         working.preparedLayoutSerial == request.layoutSerial &&
         working.preparedCp1GenerationId == request.cp1GenerationId &&
         working.preparedCp1Digests == request.cp1Digests &&
         working.preparedCp2ProfileId == request.cp2ProfileId &&
         working.preparedCp1GeometryDigest == request.cp1GeometryDigest &&
         working.preparedCp2LayoutDigest == request.cp2LayoutDigest &&
         working.preparedCp2ExclusionDigest == request.cp2ExclusionDigest;
}

} // namespace

const char *trajectoryInsetVisibleStageName(TrajectoryInsetVisibleStage stage) {
  switch (stage) {
  case TrajectoryInsetVisibleStage::None:
    return "none";
  case TrajectoryInsetVisibleStage::FreezeCurrentIdentity:
    return "freeze_current_identity";
  case TrajectoryInsetVisibleStage::AuditCp1Cp2:
    return "audit_cp1_cp2";
  case TrajectoryInsetVisibleStage::PrepareCp3:
    return "prepare_cp3";
  case TrajectoryInsetVisibleStage::VerifyCompositeEligible:
    return "verify_composite_eligible";
  case TrajectoryInsetVisibleStage::StagePlot:
    return "stage_plot";
  case TrajectoryInsetVisibleStage::StageChrome:
    return "stage_chrome";
  case TrajectoryInsetVisibleStage::RestoreState:
    return "restore_state";
  case TrajectoryInsetVisibleStage::AtomicPresent:
    return "atomic_present";
  }
  return "unknown";
}

std::vector<TrajectoryInsetVisibleStage>
orderedTrajectoryInsetVisibleFailureStages() {
  return {TrajectoryInsetVisibleStage::FreezeCurrentIdentity,
          TrajectoryInsetVisibleStage::AuditCp1Cp2,
          TrajectoryInsetVisibleStage::PrepareCp3,
          TrajectoryInsetVisibleStage::VerifyCompositeEligible,
          TrajectoryInsetVisibleStage::StagePlot,
          TrajectoryInsetVisibleStage::StageChrome,
          TrajectoryInsetVisibleStage::RestoreState,
          TrajectoryInsetVisibleStage::AtomicPresent};
}

TrajectoryInsetVisibleResult TrajectoryInsetVisibleIntegration::execute(
    const TrajectoryInsetVisibleFrameRequest &request) {
  TrajectoryInsetVisibleResult result;
  reusableEligibility_ = false;
  if (request.frameSerial <= lastAttemptSerial_) {
    result.errorCode = "trajectory_inset_visible_frame_reused_or_stale";
    return result;
  }
  std::string error;
  if (!auditCurrentRequest(request, error)) {
    result.errorCode = error;
    return result;
  }
  lastAttemptSerial_ = request.frameSerial;
  const std::uint64_t saved = dispatch_.snapshotState();
  TrajectoryInsetVisibleWorkingSet working;
  bool restoreAttempted = false, restored = false;
  const auto restore = [&]() {
    if (restoreAttempted)
      return restored;
    restoreAttempted = true;
    std::string restoreError;
    result.orderedLedger.push_back("restore_state");
    try {
      restored = dispatch_.restoreState(saved, restoreError) &&
                 dispatch_.readRestoreError() == 0;
    } catch (const std::exception &exception) {
      restored = false;
      restoreError = std::string("trajectory_inset_visible_restore_exception:") +
                     exception.what();
    } catch (...) {
      restored = false;
      restoreError = "trajectory_inset_visible_restore_unknown_exception";
    }
    result.stateRestored = restored;
    if (!restored && result.errorCode.empty())
      result.errorCode = restoreError.empty()
                             ? "trajectory_inset_visible_restore_failed"
                             : restoreError;
    return restored;
  };
  const auto fail = [&](const std::string &failure) {
    if (result.errorCode.empty())
      result.errorCode = failure;
    dispatch_.invalidate(working);
    result.invalidated = true;
    restore();
    return result;
  };
  const std::array<TrajectoryInsetVisibleStage, 6> stages{
      {TrajectoryInsetVisibleStage::FreezeCurrentIdentity,
       TrajectoryInsetVisibleStage::AuditCp1Cp2,
       TrajectoryInsetVisibleStage::PrepareCp3,
       TrajectoryInsetVisibleStage::VerifyCompositeEligible,
       TrajectoryInsetVisibleStage::StagePlot,
       TrajectoryInsetVisibleStage::StageChrome}};
  for (const auto stage : stages) {
    result.orderedLedger.push_back(trajectoryInsetVisibleStageName(stage));
    std::string stageError;
    bool invoked = false;
    try {
      invoked = dispatch_.invoke(stage, request, working, stageError);
    } catch (const std::exception &exception) {
      stageError = std::string("trajectory_inset_visible_helper_exception:") +
                   exception.what();
    } catch (...) {
      stageError = "trajectory_inset_visible_unknown_helper_exception";
    }
    if (!invoked || dispatch_.readError(stage) != 0)
      return fail(stageError.empty()
                      ? std::string("trajectory_inset_visible_") +
                            trajectoryInsetVisibleStageName(stage) + "_failed"
                      : stageError);
    if (stage == TrajectoryInsetVisibleStage::PrepareCp3 &&
        !exactPreparedIdentity(request, working))
      return fail("trajectory_inset_visible_preparation_identity_invalid");
  }
  if (!restore())
    return fail(result.errorCode);
  result.orderedLedger.push_back("atomic_present");
  std::string presentError;
  bool presented = false;
  try {
    presented = dispatch_.presentAtomic(request, working, presentError);
  } catch (const std::exception &exception) {
    presentError = std::string("trajectory_inset_visible_present_exception:") +
                   exception.what();
  } catch (...) {
    presentError = "trajectory_inset_visible_present_unknown_exception";
  }
  if (!presented ||
      dispatch_.readError(TrajectoryInsetVisibleStage::AtomicPresent) != 0)
    return fail(presentError.empty()
                    ? "trajectory_inset_visible_atomic_present_failed"
                    : presentError);
  result.ok = result.presented = true;
  result.orderedLedger.push_back("sealed");
  reusableEligibility_ = true;
  return result;
}

void TrajectoryInsetVisibleIntegration::invalidate(
    TrajectoryInsetVisibleInvalidation reason) noexcept {
  dispatch_.invalidateLifecycle(reason);
  reusableEligibility_ = false;
}

const char *trajectoryInsetVisibleAtomicFinalStageName(
    TrajectoryInsetVisibleAtomicFinalStage stage) {
  switch (stage) {
  case TrajectoryInsetVisibleAtomicFinalStage::None: return "none";
  case TrajectoryInsetVisibleAtomicFinalStage::FreezeIdentity:
    return "freeze_identity";
  case TrajectoryInsetVisibleAtomicFinalStage::AllocateFinal:
    return "allocate_final_color_semantic_framebuffer";
  case TrajectoryInsetVisibleAtomicFinalStage::ValidateIsolation:
    return "validate_isolation";
  case TrajectoryInsetVisibleAtomicFinalStage::StagePlot: return "stage_plot";
  case TrajectoryInsetVisibleAtomicFinalStage::StageSemantic:
    return "stage_semantic";
  case TrajectoryInsetVisibleAtomicFinalStage::StageChrome:
    return "stage_chrome";
  case TrajectoryInsetVisibleAtomicFinalStage::RestoreState:
    return "restore_state";
  case TrajectoryInsetVisibleAtomicFinalStage::AtomicPresent:
    return "atomic_present";
  }
  return "unknown";
}

std::vector<TrajectoryInsetVisibleAtomicFinalStage>
orderedTrajectoryInsetVisibleAtomicFinalStages() {
  return {TrajectoryInsetVisibleAtomicFinalStage::FreezeIdentity,
          TrajectoryInsetVisibleAtomicFinalStage::AllocateFinal,
          TrajectoryInsetVisibleAtomicFinalStage::ValidateIsolation,
          TrajectoryInsetVisibleAtomicFinalStage::StagePlot,
          TrajectoryInsetVisibleAtomicFinalStage::StageSemantic,
          TrajectoryInsetVisibleAtomicFinalStage::StageChrome,
          TrajectoryInsetVisibleAtomicFinalStage::RestoreState,
          TrajectoryInsetVisibleAtomicFinalStage::AtomicPresent};
}

bool TrajectoryInsetVisibleAtomicFinalResources::complete() const noexcept {
  const auto valid = [](const TrajectoryInsetFinalResourceKey &key,
                        TrajectoryInsetFinalResourceKind kind) {
    return key.kind == kind && key.handle != 0;
  };
  return sessionSerial != 0 && frameSerial != 0 && layoutSerial != 0 &&
         preparationGeneration != 0 && generation != 0 &&
         !packageGeneration.empty() && width > 0 && height > 0 &&
         (samples == 1 || samples == 2 || samples == 4) &&
         cutout.width > 0.0f && cutout.height > 0.0f &&
         plotPhysicalCoverage.x == cutout.x &&
         plotPhysicalCoverage.y == cutout.y &&
         plotPhysicalCoverage.width == cutout.width &&
         plotPhysicalCoverage.height == cutout.height &&
         valid(cp3ColorStaging, TrajectoryInsetFinalResourceKind::Texture) &&
         valid(mainLdr, TrajectoryInsetFinalResourceKind::Texture) &&
         valid(mainSemantic, TrajectoryInsetFinalResourceKind::Texture) &&
         valid(finalColor, TrajectoryInsetFinalResourceKind::Texture) &&
         valid(finalSemantic, TrajectoryInsetFinalResourceKind::Texture) &&
         valid(finalFramebuffer,
               TrajectoryInsetFinalResourceKind::Framebuffer) &&
         finalSemanticFormat == "R32UI" && isolationValidated &&
         feedbackValidated && fixedPaletteSrgb && toneApplicationCount == 1 &&
         chromeAudited && !chromeRefillsCutout &&
         panelContainerExcludedFromTrajectoryDenominator &&
         panelContainerUiToken != 0 && plotPixels != 0 &&
         semanticTokens != 0 && chromePrimitives != 0;
}

TrajectoryInsetVisibleResult executeTrajectoryInsetVisibleAtomicFinal(
    ui::TrajectoryInsetVisibleCurrentFrame &current,
    TrajectoryInsetVisibleAtomicFinalDispatch &dispatch) {
  TrajectoryInsetVisibleResult result;
  if (!current.ok || current.consumed) {
    result.errorCode = current.consumed
                           ? "trajectory_inset_visible_final_frame_reused"
                           : "trajectory_inset_visible_final_frame_absent";
    return result;
  }
  current.consumed = true;
  TrajectoryInsetVisibleAtomicFinalResources resources;
  const auto fail = [&](const std::string &error) {
    result.errorCode = error.empty()
                           ? "trajectory_inset_visible_final_stage_failed"
                           : error;
    dispatch.invalidate(resources);
    result.invalidated = true;
    return result;
  };
  const auto stages = orderedTrajectoryInsetVisibleAtomicFinalStages();
  for (const auto stage : stages) {
    if (stage == TrajectoryInsetVisibleAtomicFinalStage::AtomicPresent)
      break;
    std::string error;
    bool ok = false;
    try {
      ok = dispatch.invoke(stage, current, resources, error);
    } catch (const std::exception &exception) {
      error = std::string("trajectory_inset_visible_final_exception:") +
              exception.what();
    } catch (...) {
      error = "trajectory_inset_visible_final_unknown_exception";
    }
    if (!ok || dispatch.readError(stage) != 0)
      return fail(error);
  }
  const auto sameRect = [](const product_hud::HudRect &first,
                           const product_hud::HudRect &second) {
    return first.x == second.x && first.y == second.y &&
           first.width == second.width && first.height == second.height;
  };
  const std::array<TrajectoryInsetFinalResourceKey, 5> textureKeys{{
      resources.finalColor, resources.finalSemantic,
      resources.cp3ColorStaging, resources.mainLdr, resources.mainSemantic}};
  bool textureKeysPairwiseDistinct = true;
  for (std::size_t first = 0; first < textureKeys.size(); ++first)
    for (std::size_t second = first + 1; second < textureKeys.size(); ++second)
      textureKeysPairwiseDistinct =
          textureKeysPairwiseDistinct && textureKeys[first] != textureKeys[second];
  if (resources.sessionSerial != current.request.sessionSerial ||
      resources.frameSerial != current.request.frameSerial ||
      resources.layoutSerial != current.request.layoutSerial ||
      resources.packageGeneration != current.request.cp1GenerationId ||
      resources.preparationGeneration != current.request.frameSerial ||
      resources.samples != current.request.samples ||
      resources.width != static_cast<int>(
                             std::lround(current.layout.plotCutoutRect.width)) ||
      resources.height != static_cast<int>(
                              std::lround(current.layout.plotCutoutRect.height)) ||
      !sameRect(resources.cutout, current.layout.plotCutoutRect) ||
      !resources.complete() || !textureKeysPairwiseDistinct ||
      resources.finalFramebuffer.handle == resources.finalColor.handle ||
      resources.finalFramebuffer.handle == resources.finalSemantic.handle) {
    return fail("trajectory_inset_visible_final_contract_invalid");
  }
  for (const auto &batch : current.package.semanticBatches)
    if (batch.insetOwnershipToken == resources.panelContainerUiToken)
      return fail("trajectory_inset_visible_panel_token_alias");
  std::string preflightError;
  bool preflighted = false;
  try {
    preflighted = dispatch.preflightPresent(resources, preflightError);
  } catch (const std::exception &exception) {
    preflightError =
        std::string("trajectory_inset_visible_present_preflight_exception:") +
        exception.what();
  } catch (...) {
    preflightError =
        "trajectory_inset_visible_present_preflight_unknown_exception";
  }
  if (!preflighted ||
      dispatch.readError(TrajectoryInsetVisibleAtomicFinalStage::AtomicPresent) !=
          0)
    return fail(preflightError);
  // All failure-prone work and the final error query are complete. This call
  // is state-neutral, contains no GL operation, cannot throw, and performs the
  // one irreversible main-LDR/panel-container publish.
  dispatch.publishAtomicNoexcept(resources);
  result.ok = result.presented = result.stateRestored = true;
  return result;
}

const char *trajectoryInsetVisibleFinalGlStageName(
    TrajectoryInsetVisibleFinalGlStage stage) {
  switch (stage) {
  case TrajectoryInsetVisibleFinalGlStage::None: return "none";
  case TrajectoryInsetVisibleFinalGlStage::FreezeIdentity:
    return "freeze_identity";
  case TrajectoryInsetVisibleFinalGlStage::Allocate:
    return "allocate_final_color_semantic_framebuffer";
  case TrajectoryInsetVisibleFinalGlStage::Attach: return "attach_final";
  case TrajectoryInsetVisibleFinalGlStage::ValidateIsolation:
    return "validate_isolation";
  case TrajectoryInsetVisibleFinalGlStage::CopyProductLdr:
    return "copy_product_ldr";
  case TrajectoryInsetVisibleFinalGlStage::CompositeInset:
    return "composite_inset";
  case TrajectoryInsetVisibleFinalGlStage::CompositeSemantic:
    return "composite_semantic";
  case TrajectoryInsetVisibleFinalGlStage::CompositeChrome:
    return "composite_chrome";
  case TrajectoryInsetVisibleFinalGlStage::FinalFramebufferCheck:
    return "final_framebuffer_check";
  case TrajectoryInsetVisibleFinalGlStage::RestoreState:
    return "restore_state";
  case TrajectoryInsetVisibleFinalGlStage::FinalErrorQuery:
    return "final_error_query";
  }
  return "unknown";
}

std::vector<TrajectoryInsetVisibleFinalGlStage>
orderedTrajectoryInsetVisibleFinalGlStages() {
  return {TrajectoryInsetVisibleFinalGlStage::FreezeIdentity,
          TrajectoryInsetVisibleFinalGlStage::Allocate,
          TrajectoryInsetVisibleFinalGlStage::Attach,
          TrajectoryInsetVisibleFinalGlStage::ValidateIsolation,
          TrajectoryInsetVisibleFinalGlStage::CopyProductLdr,
          TrajectoryInsetVisibleFinalGlStage::CompositeInset,
          TrajectoryInsetVisibleFinalGlStage::CompositeSemantic,
          TrajectoryInsetVisibleFinalGlStage::CompositeChrome,
          TrajectoryInsetVisibleFinalGlStage::FinalFramebufferCheck,
          TrajectoryInsetVisibleFinalGlStage::RestoreState,
          TrajectoryInsetVisibleFinalGlStage::FinalErrorQuery};
}

bool TrajectoryInsetVisibleFinalGlResources::complete() const noexcept {
  const auto exact = [](const TrajectoryInsetFinalResourceKey &key,
                        TrajectoryInsetFinalResourceKind kind) {
    return key.kind == kind && key.handle != 0;
  };
  return exact(finalColor, TrajectoryInsetFinalResourceKind::Texture) &&
         exact(finalInsetSemantic,
               TrajectoryInsetFinalResourceKind::Texture) &&
         exact(finalFramebuffer,
               TrajectoryInsetFinalResourceKind::Framebuffer) &&
         finalColor != finalInsetSemantic &&
         finalInsetSemanticFormat == "R32UI" && width > 0 && height > 0 &&
         (cp3InputSamples == 1 || cp3InputSamples == 2 ||
          cp3InputSamples == 4) && finalOutputSamples == 1 &&
         finalColorMetadataSamples == 1 && finalSemanticMetadataSamples == 1 &&
         attachmentsExact &&
         isolationExact && feedbackFree && copiedProductLdr &&
         insetColorComposited && insetSemanticRetainedIndependent &&
         chromeComposited && cutoutNotRefilled &&
         finalFramebufferComplete && finalErrorQueryComplete &&
         toneApplicationCount == 1 && productChromeAlreadyInCurrentLdr &&
         additionalChromeDrawWrites == 0 && verifiedContentOpCount == 4 &&
         !orderedContentOpDigest.empty();
}

TrajectoryInsetVisibleFinalGlResult
prepareTrajectoryInsetVisibleFinalGlStaging(
    const TrajectoryInsetVisibleFinalGlInput &input,
    TrajectoryInsetVisibleFinalGlDispatch &dispatch) {
  TrajectoryInsetVisibleFinalGlResult result;
  const auto texture = [](const TrajectoryInsetFinalResourceKey &key) {
    return key.kind == TrajectoryInsetFinalResourceKind::Texture &&
           key.handle != 0;
  };
  if (!input.current || !input.current->ok || input.current->consumed ||
      input.sessionSerial == 0 || input.frameSerial == 0 ||
      input.layoutSerial == 0 || input.packageGeneration.empty() ||
      input.preparationGeneration == 0 || input.width <= 0 ||
      input.height <= 0 || input.outputWidth <= input.width ||
      input.outputHeight <= input.height ||
      (input.cp3InputSamples != 1 && input.cp3InputSamples != 2 &&
       input.cp3InputSamples != 4) || input.finalOutputSamples != 1 ||
      input.cutout.width <= 0.0f || input.cutout.height <= 0.0f ||
      !texture(input.cp3ColorStaging) || !texture(input.cp3InsetSemantic) ||
      !texture(input.currentProductLdr) ||
      !texture(input.mainWorldSemantic) ||
      input.cp3InsetSemanticFormat != "GL_RGBA32UI" ||
      input.cp3InsetSemanticGeneration != input.preparationGeneration ||
      input.cp3InsetSemanticChannel != 0 ||
      input.outputWidth != input.current->shellRequest.physicalWidth ||
      input.outputHeight != input.current->shellRequest.physicalHeight ||
      input.cutout.x != input.current->layout.plotCutoutRect.x ||
      input.cutout.y != input.current->layout.plotCutoutRect.y ||
      input.cutout.width != input.current->layout.plotCutoutRect.width ||
      input.cutout.height != input.current->layout.plotCutoutRect.height ||
      input.cp3ColorStaging == input.currentProductLdr ||
      input.cp3InsetSemantic == input.cp3ColorStaging ||
      input.cp3InsetSemantic == input.currentProductLdr ||
      input.cp3ColorStaging == input.mainWorldSemantic ||
      input.currentProductLdr == input.mainWorldSemantic) {
    result.errorCode = "trajectory_inset_final_gl_input_absent_or_invalid";
    return result;
  }
  if (!dispatch.contextUsable()) {
    result.adapterContextPoisoned = true;
    result.errorCode = "trajectory_inset_final_gl_context_poisoned";
    return result;
  }

  std::uint64_t saved = 0;
  try {
    saved = dispatch.captureState();
  } catch (...) {
    result.errorCode = "trajectory_inset_final_gl_capture_exception";
    return result;
  }
  bool restoreAttempted = false;
  auto restoreExactlyOnce = [&]() {
    if (restoreAttempted)
      return result.stateRestored;
    restoreAttempted = true;
    std::string error;
    try {
      result.stateRestored = dispatch.restoreState(saved, error);
    } catch (...) {
      result.stateRestored = false;
    }
    if (!result.stateRestored) {
      result.hardRestoreFailure = true;
      result.adapterContextPoisoned = true;
    }
    return result.stateRestored;
  };
  auto fail = [&](const std::string &error) {
    restoreExactlyOnce();
    dispatch.destroy(result.resources);
    if (!dispatch.contextUsable())
      result.adapterContextPoisoned = true;
    result.errorCode = error.empty() ? "trajectory_inset_final_gl_failed"
                                     : error;
    return result;
  };

  const auto rectEqual = [](const product_hud::HudRect &first,
                            const product_hud::HudRect &second) {
    return first.x == second.x && first.y == second.y &&
           first.width == second.width && first.height == second.height;
  };
  std::string opCanonical;
  const auto executeAndVerify = [&](const TrajectoryInsetVisibleFinalContentOp &op,
                                    std::string &error) {
    TrajectoryInsetVisibleFinalContentOpReceipt receipt;
    bool ok = false;
    try {
      ok = dispatch.executeContentOp(op, receipt, error);
    } catch (const std::exception &exception) {
      error = std::string("trajectory_inset_final_content_exception:") +
              exception.what();
    } catch (...) {
      error = "trajectory_inset_final_content_unknown_exception";
    }
    if (!ok || receipt.glError != 0 || receipt.kind != op.kind ||
        receipt.actualSource != op.source ||
        receipt.actualDestination != op.destination ||
        receipt.actualDestinationAttachment != op.destinationAttachment ||
        !rectEqual(receipt.actualSourceRect, op.sourceRect) ||
        !rectEqual(receipt.actualDestinationRect, op.destinationRect) ||
        receipt.actualSourceFormat != op.sourceFormat ||
        receipt.actualSourceGeneration != op.sourceGeneration ||
        receipt.actualIntegerSourceChannel != op.integerSourceChannel ||
        receipt.integerShaderConversionToR32ui !=
            op.integerShaderConversionToR32ui ||
        receipt.actualWriteCount != 1) {
      if (error.empty())
        error = "trajectory_inset_final_content_receipt_mismatch";
      return false;
    }
    appendBits(opCanonical, op.kind);
    appendBits(opCanonical, receipt.actualSource.kind);
    appendBits(opCanonical, receipt.actualSource.handle);
    appendBits(opCanonical, receipt.actualDestination.kind);
    appendBits(opCanonical, receipt.actualDestination.handle);
    appendBits(opCanonical, receipt.actualDestinationAttachment);
    appendBits(opCanonical, receipt.actualSourceRect);
    appendBits(opCanonical, receipt.actualDestinationRect);
    appendField(opCanonical, receipt.actualSourceFormat);
    appendBits(opCanonical, receipt.actualSourceGeneration);
    appendBits(opCanonical, receipt.actualIntegerSourceChannel);
    appendBits(opCanonical, receipt.integerShaderConversionToR32ui);
    ++result.resources.verifiedContentOpCount;
    switch (op.kind) {
    case TrajectoryInsetVisibleFinalContentOpKind::ClearFinalSemanticR32ui:
      break;
    case TrajectoryInsetVisibleFinalContentOpKind::CopyCurrentProductLdr:
      result.resources.copiedProductLdr = true;
      break;
    case TrajectoryInsetVisibleFinalContentOpKind::CompositeCp3InsetColor:
      result.resources.insetColorComposited = true;
      break;
    case TrajectoryInsetVisibleFinalContentOpKind::CopyCp3InsetSemantic:
      result.resources.insetSemanticRetainedIndependent = true;
      break;
    }
    return true;
  };

  const product_hud::HudRect full{
      0.0f, 0.0f, static_cast<float>(input.outputWidth),
      static_cast<float>(input.outputHeight)};
  const product_hud::HudRect plotLocal{
      0.0f, 0.0f, static_cast<float>(input.width),
      static_cast<float>(input.height)};

  for (const auto stage : orderedTrajectoryInsetVisibleFinalGlStages()) {
    if (stage == TrajectoryInsetVisibleFinalGlStage::RestoreState) {
      if (!restoreExactlyOnce())
        return fail("trajectory_inset_final_gl_restore_failed");
      if (dispatch.readError(stage) != 0) {
        dispatch.poisonContext();
        result.hardRestoreFailure = result.adapterContextPoisoned = true;
        return fail("trajectory_inset_final_gl_restore_error");
      }
      continue;
    }
    std::string error;
    bool ok = false;
    try {
      ok = dispatch.invoke(stage, input, result.resources, error);
    } catch (const std::exception &exception) {
      error = std::string("trajectory_inset_final_gl_exception:") +
              exception.what();
    } catch (...) {
      error = "trajectory_inset_final_gl_unknown_exception";
    }
    if (!ok || dispatch.readError(stage) != 0)
      return fail(error);
    std::vector<TrajectoryInsetVisibleFinalContentOp> operations;
    if (stage == TrajectoryInsetVisibleFinalGlStage::ValidateIsolation)
      operations.push_back({
          TrajectoryInsetVisibleFinalContentOpKind::ClearFinalSemanticR32ui,
          {}, result.resources.finalFramebuffer, 1, {}, full, {}, 0, 0,
          false, 0});
    if (stage == TrajectoryInsetVisibleFinalGlStage::CopyProductLdr)
      operations.push_back({
          TrajectoryInsetVisibleFinalContentOpKind::CopyCurrentProductLdr,
          input.currentProductLdr, result.resources.finalFramebuffer, 0,
          full, full});
    if (stage == TrajectoryInsetVisibleFinalGlStage::CompositeInset)
      operations.push_back({
          TrajectoryInsetVisibleFinalContentOpKind::CompositeCp3InsetColor,
          input.cp3ColorStaging, result.resources.finalFramebuffer, 0,
          plotLocal, input.cutout});
    if (stage == TrajectoryInsetVisibleFinalGlStage::CompositeSemantic)
      operations.push_back({
          TrajectoryInsetVisibleFinalContentOpKind::CopyCp3InsetSemantic,
          input.cp3InsetSemantic, result.resources.finalFramebuffer, 1,
          plotLocal, input.cutout, input.cp3InsetSemanticFormat,
          input.cp3InsetSemanticGeneration, input.cp3InsetSemanticChannel,
          true, 0});
    for (const auto &operation : operations)
      if (!executeAndVerify(operation, error))
        return fail(error);
  }
  result.resources.orderedContentOpDigest = sha256Hex(opCanonical);

  const std::array<TrajectoryInsetFinalResourceKey, 7> resources{{
      input.cp3ColorStaging, input.cp3InsetSemantic, input.currentProductLdr,
      input.mainWorldSemantic,
      result.resources.finalColor, result.resources.finalInsetSemantic,
      result.resources.finalFramebuffer}};
  for (std::size_t first = 0; first < resources.size(); ++first)
    for (std::size_t second = first + 1; second < resources.size(); ++second)
      if (resources[first] == resources[second])
        return fail("trajectory_inset_final_gl_resource_alias");
  if (!result.resources.complete() ||
      result.resources.width != input.outputWidth ||
      result.resources.height != input.outputHeight ||
      result.resources.cp3InputSamples != input.cp3InputSamples ||
      result.resources.finalOutputSamples != 1)
    return fail("trajectory_inset_final_gl_resource_contract_invalid");

  // trajectory_inset_terminal_publish_no_gl: all GL, state restoration, FBO
  // validation and the last error query have completed successfully.
  dispatch.terminalPublishOutputHandleNoexcept(result.resources);
  result.ok = true;
  return result;
}

void invalidateTrajectoryInsetVisibleFinalGlStaging(
    TrajectoryInsetVisibleFinalGlInvalidation,
    const TrajectoryInsetVisibleFinalGlResult &result,
    TrajectoryInsetVisibleFinalGlDispatch &dispatch) noexcept {
  auto resources = result.resources;
  dispatch.destroy(resources);
}

} // namespace billiardgl::render::modern
