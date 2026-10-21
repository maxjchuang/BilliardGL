#include "render/modern/modern_renderer.h"

#include "deterministic_hash.h"
#include "automation_json.h"
#include "presentation_snapshot.h"
#include "render/gltf/gltf_draw_package.h"
#include "render/gltf/gltf_gpu_upload.h"
#include "render/modern/camera_projection.h"
#include "render/modern/gpu_resources.h"
#include "render/modern/sparse_prop_shadow.h"
#include "render/modern/modern_product_session.h"
#include "render/modern/modern_renderer_product_ui.h"
#include "render/modern/product_ui_gl_core41.h"
#include "render/modern/trajectory_inset_atomic_transaction.h"
#include "render/modern/trajectory_inset_visible_integration.h"
#include "render/ui/product_shell_draw_list.h"
#include "render/ui/product_shell_layout.h"
#include "training/trajectory_inset_renderer.h"
#include "training/trajectory_overlay_renderer.h"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace billiardgl::render {
namespace {

// Test-only receipt of the exact immutable package consumed by CP3/CP4.
// It never supplies geometry, tokens, or authority to the renderer.
void writeInsetMarkerReceipt(const ui::TrajectoryInsetVisibleCurrentFrame& current) {
  const char* path = std::getenv("BILLIARDGL_INSET_MARKER_RECEIPT_PATH");
  if (!path || !*path) return;
  const auto& p = current.package;
  std::ofstream out(path);
  out.imbue(std::locale::classic());
  out << std::setprecision(9) << "{\"generation\":" << std::quoted(p.generationId)
      << ",\"layout\":" << std::quoted(p.layout.profileId)
      << ",\"palette\":" << std::quoted(p.palette == training::TrajectoryInsetPalette::Standard ? "standard" : "color_vision_safe")
      << ",\"frame_serial\":" << current.request.frameSerial
      << ",\"physical_width\":" << current.shellRequest.physicalWidth
      << ",\"physical_height\":" << current.shellRequest.physicalHeight
      << ",\"ui_scale\":" << p.layout.uiScale
      << ",\"plot_rect\":[" << p.layout.plotRect.x << ',' << p.layout.plotRect.y << ','
          << p.layout.plotRect.width << ',' << p.layout.plotRect.height << ']'
      << ",\"orderedParentSourceDigest\":" << std::quoted(p.orderedParentSourceDigest)
      << ",\"orderedProbeDigest\":" << std::quoted(p.orderedProbeDigest)
      << ",\"semantic_batches\":[";
  bool first = true;
  for (const auto& b : p.semanticBatches) {
    if (!first) out << ',';
    first = false;
    out << "{\"parent\":" << std::quoted(b.parentSourceKey)
        << ",\"member\":" << std::quoted(b.insetMemberKey)
        << ",\"role\":" << std::quoted(training::trajectoryInsetRoleName(b.role))
        << ",\"token\":" << b.insetOwnershipToken << '}';
  }
  out << "],\"probes\":[";
  first = true;
  for (const auto& probe : p.probes) {
    if (!first) out << ',';
    first = false;
    const auto& r = probe.visitMarkerRect;
    out << "{\"probe_id\":" << std::quoted(probe.probeId)
        << ",\"parent\":" << std::quoted(probe.parentSourceKey)
        << ",\"member\":" << std::quoted(probe.insetMemberKey)
        << ",\"token\":" << probe.insetOwnershipToken
        << ",\"sourceCoreMemberKey\":" << std::quoted(probe.sourceCoreMemberKey)
        << ",\"sourceCoreVisible\":" << (probe.sourceCoreVisible ? "true" : "false")
        << ",\"evidenceRole\":" << std::quoted(probe.evidenceRole)
        << ",\"visitLabel\":" << std::quoted(probe.visitLabel)
        << ",\"sourceFirstTick\":" << probe.sourceFirstTick
        << ",\"sourceLastTick\":" << probe.sourceLastTick
        << ",\"sourceAnchor\":[" << probe.sourceAnchor[0] << ',' << probe.sourceAnchor[1] << ']'
        << ",\"markerRect\":[" << r.x << ',' << r.y << ',' << r.width << ',' << r.height << ']'
        << ",\"stroke\":[" << probe.strokeXPhysicalPx << ',' << probe.strokeYPhysicalPx << ']'
        << ",\"glyph_samples\":[";
    // Independent bitmap expectations for the three explicit fixture labels.
    const char* bits = probe.visitLabel == "1" ? "010110010010111" :
        probe.visitLabel == "2" ? "111001111100111" :
        probe.visitLabel == "T" ? "111010010010010" : nullptr;
    if (bits) {
      const float ui = p.layout.uiScale;
      const float left = r.x + r.width * .5f - 9.6f * ui * .5f;
      for (int row=0; row<5; ++row) for (int col=0; col<3; ++col) {
        if (row || col) out << ',';
        // CP1 geometry is rasterized relative to its exact floating plot
        // origin; CP4 places those local pixels at the rounded viewport
        // origin. Never floor the global point directly for fractional plots.
        out << "{\"x\":" << static_cast<int>(std::lround(p.layout.plotRect.x)) +
                static_cast<int>(std::floor(left + (col*3.2f+1.6f)*ui-p.layout.plotRect.x))
            << ",\"y\":" << static_cast<int>(std::lround(p.layout.plotRect.y)) +
                static_cast<int>(std::floor(r.y+(2.0f+row*3.2f+1.6f)*ui-p.layout.plotRect.y))
            << ",\"ink\":" << (bits[row*3+col]=='1' ? "true" : "false") << '}';
      }
    }
    out << "]}";
  }
  out << "]}\n";
  if (!out) throw std::runtime_error("trajectory_inset_marker_receipt_write_failed");
}

template <typename Value>
void appendOverlayCacheValue(std::string &output, const Value &value) {
  output.append(reinterpret_cast<const char *>(&value), sizeof(value));
}

void appendOverlayTrackKey(std::string &output,
                           const training::OverlayTrack &track) {
  for (float component : track.encoding.color)
    appendOverlayCacheValue(output, component);
  appendOverlayCacheValue(output, track.visible);
  appendOverlayCacheValue(output, track.endpointVisible);
  appendOverlayCacheValue(output, track.endpoint.x);
  appendOverlayCacheValue(output, track.endpoint.y);
  appendOverlayCacheValue(output, track.endpoint.z);
  const std::uint64_t count = track.samples.size();
  appendOverlayCacheValue(output, count);
  for (const training::OverlaySample &sample : track.samples) {
    appendOverlayCacheValue(output, sample.tick);
    appendOverlayCacheValue(output, sample.position.x);
    appendOverlayCacheValue(output, sample.position.y);
    appendOverlayCacheValue(output, sample.position.z);
    appendOverlayCacheValue(output, sample.visible);
  }
}

std::string overlayCacheKey(const training::TrajectoryOverlaySnapshot &snapshot,
                            const CameraPresentation &camera,
                            const training::TrajectoryOverlayViewport &viewport,
                            training::TrajectoryOverlayPalette palette) {
  std::string key;
  key.reserve(128 + (snapshot.predicted.samples.size() +
                     snapshot.actual.samples.size()) *
                        32);
  for (float value : {camera.eye.x, camera.eye.y, camera.eye.z, camera.target.x,
                      camera.target.y, camera.target.z, camera.projectionPan.x,
                      camera.projectionPan.y, camera.projectionPan.z,
                      camera.fieldOfViewDegrees})
    appendOverlayCacheValue(key, value);
  appendOverlayCacheValue(key, viewport.physicalWidth);
  appendOverlayCacheValue(key, viewport.physicalHeight);
  appendOverlayCacheValue(key, viewport.deviceScale);
  appendOverlayCacheValue(key, viewport.uiScale);
  appendOverlayCacheValue(key, palette);
  appendOverlayTrackKey(key, snapshot.predicted);
  appendOverlayTrackKey(key, snapshot.actual);
  return key;
}

void insetLedgerField(std::string &ledger, const std::string &field) {
  ledger += std::to_string(field.size()) + ":" + field + "\n";
}

std::string insetCanonicalDoubleBits(double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::hex << std::setfill('0') << std::setw(16) << bits;
  return output.str();
}

std::array<std::string, 5> recomputeTrajectoryInsetDigests(
    const training::TrajectoryInsetDrawPackage &package) {
  std::string parents, probes, events, contributors, sourceIntervals;
  for (const auto &parent : package.orderedParentSourceKeys)
    insetLedgerField(parents, parent);
  for (const auto &probe : package.probes) {
    for (const auto &field :
         {probe.probeId, probe.parentSourceKey, probe.insetMemberKey,
          probe.companionInsetMemberKey,
          std::to_string(probe.insetOwnershipToken),
          std::to_string(probe.strokeXPhysicalPx),
          std::to_string(probe.strokeYPhysicalPx),
          std::to_string(probe.companionXPhysicalPx),
          std::to_string(probe.companionYPhysicalPx),
          std::to_string(probe.backgroundXPhysicalPx),
          std::to_string(probe.backgroundYPhysicalPx)})
      insetLedgerField(probes, field);
    probes += training::trajectoryInsetProbeSourceEvidenceLedger(probe);
  }
  for (const auto &event : package.predictedDashEvents) {
    for (const auto &field :
         {event.eventKey, std::to_string(event.routeWideDashEventOrdinal),
          std::to_string(event.routeSpanOrdinal),
          insetCanonicalDoubleBits(event.routeDistanceStartPx),
          insetCanonicalDoubleBits(event.routeDistanceEndPx)})
      insetLedgerField(events, field);
    for (std::size_t role = 0; role < event.orderedRoleMemberKeys.size();
         ++role) {
      insetLedgerField(events, event.orderedRoleMemberKeys[role]);
      insetLedgerField(events, std::to_string(event.orderedRoleTokens[role]));
    }
    for (const auto &contributor : event.orderedContributors) {
      for (const auto &field :
           {event.eventKey, contributor.parentSourceKey,
            contributor.arcSegmentId,
            std::to_string(contributor.routeOrderOrdinal),
            insetCanonicalDoubleBits(contributor.sourceLocalTStart),
            insetCanonicalDoubleBits(contributor.sourceLocalTEnd),
            insetCanonicalDoubleBits(contributor.eventRouteDistanceStartPx),
            insetCanonicalDoubleBits(contributor.eventRouteDistanceEndPx)})
        insetLedgerField(contributors, field);
    }
  }
  for (const auto &row : package.mappingRows) {
    if (row.orderedPredictedRouteIntervals.empty())
      continue;
    insetLedgerField(sourceIntervals, row.parentSourceKey);
    insetLedgerField(sourceIntervals,
                     std::to_string(row.orderedPredictedRouteIntervals.size()));
    for (const auto &interval : row.orderedPredictedRouteIntervals) {
      for (const auto &field :
           {interval.arcSegmentId, std::to_string(interval.routeOrderOrdinal),
            std::to_string(interval.routeSpanOrdinal),
            insetCanonicalDoubleBits(interval.sourceLocalTStart),
            insetCanonicalDoubleBits(interval.sourceLocalTEnd),
            insetCanonicalDoubleBits(interval.routeDistanceStartPx),
            insetCanonicalDoubleBits(interval.routeDistanceEndPx),
            interval.dashEventKey, interval.nonRenderedReason})
        insetLedgerField(sourceIntervals, field);
    }
  }
  return {{sha256Hex(parents), sha256Hex(probes), sha256Hex(events),
           sha256Hex(contributors), sha256Hex(sourceIntervals)}};
}

std::array<float, 4>
expectedTrajectoryInsetColor(training::TrajectoryInsetPalette palette,
                             training::TrajectoryInsetTrack track,
                             training::TrajectoryInsetRole role) {
  const bool actual = track == training::TrajectoryInsetTrack::Actual;
  const bool halo = role == training::TrajectoryInsetRole::ActualLeftHalo ||
                    role == training::TrajectoryInsetRole::ActualRightHalo ||
                    role == training::TrajectoryInsetRole::PredictedInnerHalo ||
                    role == training::TrajectoryInsetRole::PredictedEventVisitMarkerHalo;
  const bool outer = role == training::TrajectoryInsetRole::PredictedOuterRim;
  if (halo)
    return actual ? std::array<float, 4>{{1, 1, 1, 1}}
                  : std::array<float, 4>{{0.01f, 0.01f, 0.012f, 1}};
  if (outer)
    return {{0.01f, 0.01f, 0.012f, 1}};
  if (actual)
    return palette == training::TrajectoryInsetPalette::Standard
               ? std::array<float, 4>{{0.01f, 0.28f, 0.55f, 1}}
               : std::array<float, 4>{{0.05f, 0.22f, 0.80f, 1}};
  return palette == training::TrajectoryInsetPalette::Standard
             ? std::array<float, 4>{{1.0f, 0.78f, 0.08f, 1}}
             : std::array<float, 4>{{1.0f, 0.40f, 0.02f, 1}};
}

std::string trajectoryInsetGeometryDigest(
    const training::TrajectoryInsetDrawPackage &package) {
  return ui::canonicalTrajectoryInsetGeometryDigest(package);
}

bool trajectoryInsetPaletteContractValid(
    const training::TrajectoryInsetDrawPackage &package) {
  if (package.palette != training::TrajectoryInsetPalette::Standard &&
      package.palette != training::TrajectoryInsetPalette::ColorVisionSafe)
    return false;
  std::vector<modern::TrajectoryInsetFrozenColorPair> pairs;
  pairs.reserve(package.vertices.size());
  for (const auto &batch : package.semanticBatches) {
    const auto expected =
        expectedTrajectoryInsetColor(package.palette, batch.track, batch.role);
    if (batch.firstVertex + batch.vertexCount > package.vertices.size())
      return false;
    for (std::size_t index = batch.firstVertex;
         index < batch.firstVertex + batch.vertexCount; ++index)
      pairs.push_back({expected, package.vertices[index].linearRgba});
  }
  std::string error;
  return modern::validateTrajectoryInsetFrozenPaletteColors(pairs, error);
}

std::string
trajectoryInsetLayoutDigest(const ui::TrajectoryInsetShellLayout &layout) {
  return ui::canonicalTrajectoryInsetLayoutDigest(layout);
}

bool validateTrajectoryInsetPackageIdentity(
    const training::TrajectoryInsetDrawPackage &package,
    const TrajectoryInsetAtomicFrameInput &input, std::string &error) {
  const std::array<std::string, 5> recomputed =
      recomputeTrajectoryInsetDigests(package);
  const std::array<std::string, 5> embedded{
      {package.orderedParentSourceDigest, package.orderedProbeDigest,
       package.orderedDashEventDigest, package.orderedDashContributorDigest,
       package.orderedSourceRouteIntervalDigest}};
  if (package.generationId.empty() ||
      package.generationId != input.cp1GenerationId || recomputed != embedded ||
      recomputed != input.cp1OrderedDigests) {
    error = "trajectory_inset_cp1_identity_or_digest_invalid";
    return false;
  }
  if (package.sourceGroupDenominator != package.mappingRows.size() ||
      package.semanticMemberDenominator != package.semanticBatches.size() ||
      package.probeDenominator != package.probes.size() ||
      package.orderedParentSourceKeys.size() != package.mappingRows.size() ||
      package.orderedProbeIds.size() != package.probes.size()) {
    error = "trajectory_inset_cp1_denominator_invalid";
    return false;
  }
  std::unordered_map<std::string,
                     const training::TrajectoryInsetSemanticBatch *>
      members;
  std::unordered_map<std::uint32_t, std::string> tokens;
  std::vector<bool> vertexClaimed(package.vertices.size(), false);
  if (!trajectoryInsetPaletteContractValid(package)) {
    error = "trajectory_inset_cp1_palette_invalid";
    return false;
  }
  for (const auto &batch : package.semanticBatches) {
    bool actualRole = false;
    switch (batch.role) {
    case training::TrajectoryInsetRole::ActualLeftHalo:
    case training::TrajectoryInsetRole::ActualLeftCore:
    case training::TrajectoryInsetRole::ActualRightHalo:
    case training::TrajectoryInsetRole::ActualRightCore:
    case training::TrajectoryInsetRole::ActualEndpointCapsule:
    case training::TrajectoryInsetRole::ActualDirectionNotch: actualRole=true; break;
    case training::TrajectoryInsetRole::PredictedOuterRim:
    case training::TrajectoryInsetRole::PredictedInnerHalo:
    case training::TrajectoryInsetRole::PredictedDashCore:
    case training::TrajectoryInsetRole::PredictedEndpointDiamond:
    case training::TrajectoryInsetRole::PredictedEventVisitMarker:
    case training::TrajectoryInsetRole::PredictedEventVisitMarkerHalo:
    case training::TrajectoryInsetRole::PredictedEventVisitLeader: break;
    default: error="trajectory_inset_cp1_unknown_role"; return false;
    }
    if (batch.track != (actualRole ? training::TrajectoryInsetTrack::Actual :
                                   training::TrajectoryInsetTrack::Predicted)) {
      error="trajectory_inset_cp1_role_track_invalid";
      return false;
    }
    const std::array<float, 4> expectedColor =
        expectedTrajectoryInsetColor(package.palette, batch.track, batch.role);
    if (batch.insetMemberKey.empty() || batch.parentSourceKey.empty() ||
        batch.arcSegmentId.empty() || batch.insetOwnershipToken == 0 ||
        batch.vertexCount == 0 ||
        batch.firstVertex + batch.vertexCount > package.vertices.size() ||
        !members.emplace(batch.insetMemberKey, &batch).second ||
        !tokens.emplace(batch.insetOwnershipToken, batch.insetMemberKey)
             .second) {
      error = "trajectory_inset_cp1_member_or_token_invalid";
      return false;
    }
    for (std::size_t index = batch.firstVertex;
         index < batch.firstVertex + batch.vertexCount; ++index) {
      if (vertexClaimed[index] ||
          package.vertices[index].insetOwnershipToken !=
              batch.insetOwnershipToken ||
          package.vertices[index].linearRgba != expectedColor) {
        error = "trajectory_inset_cp1_color_semantic_coverage_invalid";
        return false;
      }
      vertexClaimed[index] = true;
    }
  }
  if (std::find(vertexClaimed.begin(), vertexClaimed.end(), false) !=
      vertexClaimed.end()) {
    error = "trajectory_inset_cp1_unclaimed_vertex";
    return false;
  }
  std::set<std::string> parents(package.orderedParentSourceKeys.begin(),
                                package.orderedParentSourceKeys.end());
  if (parents.size() != package.orderedParentSourceKeys.size()) {
    error = "trajectory_inset_cp1_parent_inventory_invalid";
    return false;
  }
  std::set<std::string> mappedMembers;
  for (const auto &row : package.mappingRows) {
    if (!parents.count(row.parentSourceKey)) {
      error = "trajectory_inset_cp1_mapping_parent_invalid";
      return false;
    }
    for (const auto &memberKey : row.orderedInsetMemberKeys) {
      const auto member = members.find(memberKey);
      if (member == members.end() ||
          member->second->parentSourceKey != row.parentSourceKey ||
          !mappedMembers.insert(memberKey).second) {
        error = "trajectory_inset_cp1_mapping_member_invalid";
        return false;
      }
    }
  }
  for (const auto &event : package.predictedDashEvents) {
    if (event.eventKey.empty() || event.orderedContributors.empty()) {
      error = "trajectory_inset_cp1_dash_event_invalid";
      return false;
    }
    for (std::size_t role = 0; role < event.orderedRoleMemberKeys.size();
         ++role) {
      const auto member = members.find(event.orderedRoleMemberKeys[role]);
      if (member == members.end() ||
          member->second->insetOwnershipToken !=
              event.orderedRoleTokens[role] ||
          !mappedMembers.insert(event.orderedRoleMemberKeys[role]).second) {
        error = "trajectory_inset_cp1_dash_event_member_invalid";
        return false;
      }
    }
    for (const auto &contributor : event.orderedContributors)
      if (!parents.count(contributor.parentSourceKey)) {
        error = "trajectory_inset_cp1_dash_contributor_invalid";
        return false;
      }
  }
  // Visit evidence supplements an original dash event; it never becomes a
  // source row or replaces that event's three canonical role members.
  // Reuse CP1's provenance/visibility/ordering/geometry audit before admitting
  // precisely one glyph, halo, and leader under that same event identity.
  if (!training::validateTrajectoryInsetVisitEvidence(package, error))
    return false;
  for (const auto& probe : package.probes) {
    if (probe.sourceCoreMemberKey.empty()) continue;
    const auto marker = members.find(probe.insetMemberKey);
    if (marker == members.end()) {
      error = "trajectory_inset_cp1_visit_member_missing";
      return false;
    }
    const std::string prefix = "visit_marker_";
    const std::string primitive = marker->second->primitiveId;
    if (primitive.compare(0, prefix.size(), prefix) != 0) {
      error = "trajectory_inset_cp1_visit_primitive_invalid";
      return false;
    }
    const std::string ordinal = primitive.substr(prefix.size());
    std::array<unsigned,3> counts{};
    for (const auto& batch : package.semanticBatches) {
      if (batch.parentSourceKey != probe.parentSourceKey) continue;
      std::size_t role = 3;
      std::string expectedPrimitive;
      switch (batch.role) {
      case training::TrajectoryInsetRole::PredictedEventVisitMarker:
        role=0; expectedPrimitive="visit_marker_"+ordinal;
        if (batch.insetMemberKey != probe.insetMemberKey) role=3;
        break;
      case training::TrajectoryInsetRole::PredictedEventVisitMarkerHalo:
        role=1; expectedPrimitive="visit_marker_halo_"+ordinal;
        if (batch.insetMemberKey != probe.companionInsetMemberKey) role=3;
        break;
      case training::TrajectoryInsetRole::PredictedEventVisitLeader:
        role=2; expectedPrimitive="visit_leader_"+ordinal; break;
      default: continue;
      }
      if (role == 3 || ++counts[role] != 1 ||
          batch.track != training::TrajectoryInsetTrack::Predicted ||
          batch.arcSegmentId != probe.parentSourceKey ||
          batch.primitiveId != expectedPrimitive ||
          !mappedMembers.insert(batch.insetMemberKey).second) {
        error = "trajectory_inset_cp1_visit_mapping_invalid";
        return false;
      }
    }
    if (counts != std::array<unsigned,3>{{1,1,1}}) {
      error = "trajectory_inset_cp1_visit_mapping_incomplete";
      return false;
    }
  }
  if (mappedMembers.size() != members.size()) {
    error = "trajectory_inset_cp1_mapping_incomplete";
    return false;
  }
  std::set<std::string> probeIds;
  for (std::size_t index = 0; index < package.probes.size(); ++index) {
    const auto &probe = package.probes[index];
    const auto member = members.find(probe.insetMemberKey);
    const auto companion = members.find(probe.companionInsetMemberKey);
    if (probe.probeId != package.orderedProbeIds[index] ||
        !probeIds.insert(probe.probeId).second || member == members.end() ||
        companion == members.end() ||
        member->second->insetOwnershipToken != probe.insetOwnershipToken ||
        probe.insetOwnershipToken == 0) {
      error = "trajectory_inset_cp1_probe_or_companion_invalid";
      return false;
    }
  }
  return true;
}

const char *kSceneVertex = R"GLSL(#version 330 core
layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aUv;
uniform vec2 uCenter;
uniform vec2 uHalfSize;
uniform float uAngle;
uniform float uDepth;
out vec2 vUv;
void main(){
  mat2 rotation=mat2(cos(uAngle),sin(uAngle),-sin(uAngle),cos(uAngle));
  gl_Position=vec4(uCenter+rotation*(aPosition*uHalfSize),uDepth,1.0);
  vUv=aUv;
})GLSL";

const char *kSceneFragment = R"GLSL(#version 330 core
in vec2 vUv;
layout(location=0) out vec4 color;
uniform vec4 uColor;
uniform int uShape;
uniform int uIdentity;
uniform int uPbrMode;
uniform sampler2D uColorTexture;
uniform sampler2D uNormalTexture;
uniform sampler2D uOrmTexture;
uniform sampler2D uBallAtlas;
uniform samplerCube uIrradianceMap;
uniform samplerCube uSpecularMap;
uniform sampler2D uShadowMap;
uniform vec2 uUvRepeat;
uniform vec2 uViewport;
uniform int uReceiveShadow;
uniform float uRoughness;
uniform float uMetallic;
uniform float uClearcoat;
uniform int uPcfKernelWidth;
uniform float uShadowBias;
uniform int uAreaLightSamples;
uniform float uContactShadowOpacity;
uniform int uLightingContractV2;
uniform float uDirectKeyIntensityScale;
uniform float uV2AmbientFloor;
uniform float uV2IrradianceScale;
uniform float uV2ReflectedScale;
float shadowOcclusion(vec2 uv){
  vec2 texel=1.0/vec2(textureSize(uShadowMap,0));
  if(uPcfKernelWidth<=1)return texture(uShadowMap,uv).r<0.99-uShadowBias?1.0:0.0;
  float sum=0.0,count=0.0;
  for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
    if(uPcfKernelWidth==2&&(x<0||y<0))continue;
    sum+=texture(uShadowMap,uv+vec2(x,y)*texel).r<0.99-uShadowBias?1.0:0.0;count+=1.0;
  }
  return sum/max(count,1.0);
}
void main(){
  vec3 base=uColor.rgb;
  vec3 normalSample=vec3(0.0,0.0,1.0);float ao=1.0,roughness=uRoughness,metallic=uMetallic;
  if(uPbrMode==1){
    vec2 tiled=vUv*uUvRepeat;
    base*=texture(uColorTexture,tiled).rgb;
    normalSample=normalize(texture(uNormalTexture,tiled).xyz*2.0-1.0); // OpenGL +Y: intentionally not flipped
    vec3 orm=texture(uOrmTexture,tiled).rgb;ao=orm.r;roughness=orm.g;metallic=orm.b;
  }else if(uPbrMode==2){
    int tile=max(0,min(uIdentity,15));
    vec2 sphereP=vUv*2.0-1.0;
    float sphereZ=sqrt(max(0.0,1.0-dot(sphereP,sphereP)));
    vec2 sphereUv=vec2(fract(0.75+atan(sphereP.x,sphereZ)/6.2831853),
                       0.5-asin(clamp(sphereP.y,-1.0,1.0))/3.14159265);
    vec2 tilePixels=vec2(textureSize(uBallAtlas,0))/vec2(8.0,2.0);
    vec2 inset=vec2(24.0)/tilePixels;
    sphereUv=clamp(sphereUv,inset,vec2(1.0)-inset);
    sphereUv.y=1.0-sphereUv.y;
    vec2 atlasUv=(vec2(float(tile%8),float(tile/8))+sphereUv)/vec2(8.0,2.0);
    base=texture(uBallAtlas,atlasUv).rgb;
  }
  if(uReceiveShadow==1){
    base*=1.0-0.24*shadowOcclusion(gl_FragCoord.xy/uViewport);
  }
  if(uShape==2){vec2 p=vUv*2.0-1.0;float d=dot(p,p);if(d>1.0)discard;float a=(1.0-smoothstep(0.12,1.0,d))*min(1.0,uContactShadowOpacity*2.0);color=vec4(0.002,0.003,0.003,a);return;}
  if(uShape==1){
    vec2 p=vUv*2.0-1.0; float d=dot(p,p); if(d>1.0) discard;
    vec3 n=normalize(vec3(p,sqrt(1.0-d)));
    vec3 lights[4]=vec3[4](normalize(vec3(-0.55,0.62,1.0)),normalize(vec3(0.20,0.72,1.0)),normalize(vec3(-0.20,0.48,1.0)),normalize(vec3(0.52,0.58,1.0)));
    int samples=max(1,min(uAreaLightSamples,4));float ndl=0.0,coat=0.0;
    float exponent=mix(160.0,10.0,clamp(roughness,0.0,1.0));
    for(int i=0;i<samples;++i){ndl+=max(dot(n,lights[i]),0.0);coat+=pow(max(dot(n,normalize(lights[i]+vec3(0,0,1))),0.0),exponent);}
    ndl/=float(samples);coat=coat/float(samples)*uClearcoat;
    vec3 f0=mix(vec3(0.04),base,metallic);float fresnel=0.04+(1.0-0.04)*pow(1.0-n.z,5.0);
    vec3 irradiance=texture(uIrradianceMap,n).rgb;
    vec3 reflected=textureLod(uSpecularMap,reflect(vec3(0.0,0.0,-1.0),n),roughness*6.0).rgb;
    if(uLightingContractV2==1){
      vec3 diffuse=base*(1.0-metallic)*(1.0-fresnel)*(vec3(uV2AmbientFloor+ndl*uDirectKeyIntensityScale)+irradiance*uV2IrradianceScale);
      vec3 specular=f0*(coat*uDirectKeyIntensityScale+reflected*uV2ReflectedScale);
      color=vec4((diffuse*(1.0-0.25*uClearcoat)+specular+vec3(coat*uDirectKeyIntensityScale))*ao,1.0);return;
    }
    vec3 diffuse=base*(1.0-metallic)*(1.0-fresnel)*(0.30+1.75*ndl+irradiance*2.0);
    vec3 specular=f0*(0.10+2.5*coat+reflected*3.5);
    color=vec4((diffuse*(1.0-0.25*uClearcoat)+specular+vec3(coat))*ao,1.0); return;
  }
  if(uPbrMode==1){
    vec3 n=normalize(vec3(normalSample.xy*0.35,max(normalSample.z,0.25)));
    float ndl=max(dot(n,normalize(vec3(-0.35,0.45,1.0))),0.0);
    vec3 f0=mix(vec3(0.04),base,metallic);
    float spec=pow(max(n.z,0.0),mix(96.0,8.0,roughness));
    vec3 reflected=textureLod(uSpecularMap,reflect(vec3(0.0,0.0,-1.0),n),roughness*6.0).rgb;
    vec3 ambient=texture(uIrradianceMap,n).rgb*base*(1.0-metallic);
    if(uLightingContractV2==1){
      vec3 diffuse=base*(1.0-metallic)*(uV2AmbientFloor+ndl*uDirectKeyIntensityScale);
      base=(diffuse*(1.0-0.25*uClearcoat)+ambient*uV2IrradianceScale+f0*(spec*uDirectKeyIntensityScale+reflected*uV2ReflectedScale)+vec3(spec*uClearcoat*0.22*uDirectKeyIntensityScale))*ao;
    }else{
      vec3 diffuse=base*(1.0-metallic)*(0.22+0.95*ndl);
      base=(diffuse*(1.0-0.25*uClearcoat)+ambient+f0*(spec+reflected*3.0)+vec3(spec*uClearcoat*0.22))*ao;
    }
  }
  color=vec4(base, uColor.a);
})GLSL";

const char *kDepthFragment = R"GLSL(#version 330 core
in vec2 vUv;
uniform int uShape;
void main(){if(uShape==1){vec2 p=vUv*2.0-1.0;if(dot(p,p)>1.0)discard;}}
)GLSL";

const char *kToneVertex = R"GLSL(#version 330 core
layout(location=0) in vec2 aPosition; layout(location=1) in vec2 aUv;
out vec2 vUv; void main(){gl_Position=vec4(aPosition,0,1);vUv=aUv;}
)GLSL";

const char *kMeshVertex = R"GLSL(#version 330 core
layout(location=0) in vec3 aPosition;layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aTangent;layout(location=3) in vec2 aUv;layout(location=4) in vec4 aVertexColor;
uniform mat4 uMvp;uniform mat4 uModel;uniform mat4 uLightViewProjection;out vec2 vUv;out vec3 vNormal;out vec3 vTangent;out vec3 vBitangent;out vec4 vVertexColor;out vec3 vWorldPosition;out vec4 vLightClip;
void main(){gl_Position=uMvp*vec4(aPosition,1.0);vec4 world=uModel*vec4(aPosition,1.0);vWorldPosition=world.xyz;vLightClip=uLightViewProjection*world;mat3 normalMatrix=mat3(uModel);vNormal=normalize(normalMatrix*aNormal);vTangent=normalize(normalMatrix*aTangent.xyz);vBitangent=normalize(cross(vNormal,vTangent)*aTangent.w);vUv=aUv;vVertexColor=aVertexColor;}
)GLSL";

// Dedicated diagnostic shaders leave the normal-picture GLSL untouched.
// The same vertex geometry/depth/occlusion is used; no albedo/brightness
// selection, ACES, exposure, bloom or IBL can influence ownership colors.
const char *kObjectClassMeshFragment = R"GLSL(#version 330 core
uniform vec4 uColor;layout(location=0)out vec4 color;
void main(){color=uColor;}
)GLSL";
const char *kObjectClassOverlayFragment = R"GLSL(#version 330 core
in vec2 vUv;uniform int uShape;uniform vec4 uColor;layout(location=0)out vec4 color;
void main(){
 if(uShape==1){vec2 p=vUv*2.0-1.0;if(dot(p,p)>1.0)discard;}
 color=vec4(0,0,0,uColor.a);
}
)GLSL";
const char *kObjectClassToneFragment = R"GLSL(#version 330 core
in vec2 vUv;uniform sampler2D uHdr;layout(location=0)out vec4 color;
void main(){color=texture(uHdr,vUv);}
)GLSL";

const char *kMeshFragment = R"GLSL(#version 330 core
in vec2 vUv;in vec3 vNormal;in vec3 vTangent;in vec3 vBitangent;in vec4 vVertexColor;in vec3 vWorldPosition;in vec4 vLightClip;layout(location=0)out vec4 color;
uniform vec4 uColor;uniform int uPbrMode;uniform int uIdentity;uniform int uObjectIdMode;uniform int uPocketSidewall;uniform vec2 uUvRepeat;
uniform float uRoughness;uniform float uMetallic;uniform float uClearcoat;
uniform vec3 uCameraPosition;
uniform int uBallAreaLightSamples;uniform int uBallLightingDiagnosticMode;uniform int uBallRectSpecular;uniform vec3 uKeyLightCenterCm;uniform vec2 uKeyLightHalfSizeCm;uniform vec3 uKeyLightEmissionNormal;
uniform int uLightingContractV2;uniform float uDirectKeyIntensityScale;
uniform float uV2AmbientFloor;uniform float uV2IrradianceScale;uniform float uV2ReflectedScale;
uniform sampler2D uColorTexture;uniform sampler2D uNormalTexture;uniform sampler2D uOrmTexture;uniform sampler2D uBallAtlas;
uniform int uPocketIndirectEnabled;uniform vec3 uPocketMouthsCm[6];
uniform samplerCube uIrradianceMap;uniform samplerCube uSpecularMap;uniform sampler2D uShadowMap;
uniform int uReceiveShadow;uniform int uShadowFilterTaps;uniform int uShadowCompareBilinear;uniform float uShadowBias;uniform float uBallShadowBiasScale;uniform vec2 uShadowTexelSize;uniform vec2 uShadowSoftnessUv;
float shadowDepthCompare(ivec2 pixel,ivec2 sizePx,float receiverDepth){
 if(any(lessThan(pixel,ivec2(0)))||any(greaterThanEqual(pixel,sizePx)))return 1.0;
 return receiverDepth<=texelFetch(uShadowMap,pixel,0).r?1.0:0.0;
}
float shadowCompareFiltered(vec2 uv,float receiverDepth){
 if(uShadowCompareBilinear==0)return receiverDepth<=texture(uShadowMap,uv).r?1.0:0.0;
 ivec2 sizePx=textureSize(uShadowMap,0);
 vec2 texel=uv*vec2(sizePx)-vec2(0.5);ivec2 base=ivec2(floor(texel));vec2 fraction=fract(texel);
 float a=shadowDepthCompare(base,sizePx,receiverDepth);
 float b=shadowDepthCompare(base+ivec2(1,0),sizePx,receiverDepth);
 float c=shadowDepthCompare(base+ivec2(0,1),sizePx,receiverDepth);
 float d=shadowDepthCompare(base+ivec2(1,1),sizePx,receiverDepth);
 // Interpolate visibility comparisons, never depths across blocker edges.
 return mix(mix(a,b,fraction.x),mix(c,d,fraction.x),fraction.y);
}
float shadowVisibility(){
 if(uReceiveShadow==0||vLightClip.w<=0.0)return 1.0;
 vec3 p=vLightClip.xyz/vLightClip.w*0.5+0.5;
 if(any(lessThan(p,vec3(0.0)))||any(greaterThan(p,vec3(1.0))))return 1.0;
 int requested=max(1,min(uShadowFilterTaps,24));float visible=0.0;float taps=0.0;
 float centreDepth=texture(uShadowMap,p.xy).r;
 float contactHardening=clamp(max(p.z-centreDepth,0.0)*8.0,0.08,1.0);
 vec2 radius=max(uShadowSoftnessUv*contactHardening,uShadowTexelSize*0.5);
 float receiverBias=uShadowBias*(uPbrMode==2?uBallShadowBiasScale:1.0);
 for(int i=0;i<24;++i){
  if(i>=requested)continue;
  float fi=float(i)+0.5;float radial=sqrt(fi/float(requested));float angle=fi*2.39996323;
  vec2 offset=vec2(cos(angle),sin(angle))*radial*radius;
  vec2 sampleUv=p.xy+offset;
  if(any(lessThan(sampleUv,vec2(0.0)))||any(greaterThan(sampleUv,vec2(1.0)))){
   visible+=1.0;taps+=1.0;continue;
  }
  visible+=shadowCompareFiltered(sampleUv,p.z-receiverBias);taps+=1.0;
 }
 return visible/max(taps,1.0);
}
float authoredBallRectSpecular(vec3 worldPosition,vec3 normal,vec3 viewDirection,float roughness){
 vec3 ray=reflect(-viewDirection,normal);float denominator=dot(ray,uKeyLightEmissionNormal);
 // The emitter has one authored emitting face. A reflected ray travelling
 // away from it must not acquire a second/behind-surface key highlight.
 if(denominator>=-0.0001)return 0.0;
 float distance=dot(uKeyLightCenterCm-worldPosition,uKeyLightEmissionNormal)/denominator;
 if(distance<=0.0)return 0.0;
 vec3 reference=abs(uKeyLightEmissionNormal.z)<0.999?vec3(0.0,0.0,1.0):vec3(1.0,0.0,0.0);
 vec3 axisX=normalize(cross(reference,uKeyLightEmissionNormal));vec3 axisZ=normalize(cross(uKeyLightEmissionNormal,axisX));
 vec3 hit=worldPosition+ray*distance-uKeyLightCenterCm;
 vec2 local=vec2(dot(hit,axisX),dot(hit,axisZ));
 // Roughness supplies a finite emitter-plane footprint; screen derivatives
 // stabilize the rectangle boundary without adding light samples/lobes.
 vec2 footprint=max(vec2(roughness*roughness*distance),fwidth(local)*1.5);
 // Grazing rays approach a plane intersection at infinity. Their enormous
 // derivatives must not smear the finite lamp into a spurious sphere ring.
 footprint=min(footprint,uKeyLightHalfSizeCm*0.25);
 footprint=max(footprint,vec2(0.001));
 vec2 coverage=vec2(1.0)-smoothstep(uKeyLightHalfSizeCm-footprint,uKeyLightHalfSizeCm+footprint,abs(local));
 return coverage.x*coverage.y;
}
void authoredBallDirectLight(vec3 worldPosition,vec3 normal,vec3 viewDirection,float roughness,out float diffuseTerm,out float specularTerm){
 vec3 reference=abs(uKeyLightEmissionNormal.z)<0.999?vec3(0.0,0.0,1.0):vec3(1.0,0.0,0.0);
 vec3 axisX=normalize(cross(reference,uKeyLightEmissionNormal));vec3 axisZ=normalize(cross(uKeyLightEmissionNormal,axisX));
 const float g=0.57735026919;vec2 offsets[4]=vec2[4](vec2(-g,-g),vec2(g,g),vec2(-g,g),vec2(g,-g));
 const float nodes[4]=float[4](-0.86113631159,-0.33998104358,0.33998104358,0.86113631159);
 const float quadratureWeights[4]=float[4](0.34785484514,0.65214515486,0.65214515486,0.34785484514);
 int count=max(1,min(uBallAreaLightSamples,16));float diffuseSum=0.0,specularSum=0.0,weightSum=0.0;
 for(int i=0;i<16;++i){if(i>=count)continue;vec2 offset;float quadratureWeight=1.0;
  if(count==16){int xIndex=i%4;int zIndex=i/4;offset=vec2(nodes[xIndex],nodes[zIndex]);quadratureWeight=quadratureWeights[xIndex]*quadratureWeights[zIndex];}
  else offset=count==1?vec2(0.0):offsets[i];
  vec3 lightPosition=uKeyLightCenterCm+axisX*(offset.x*uKeyLightHalfSizeCm.x)+axisZ*(offset.y*uKeyLightHalfSizeCm.y);
  vec3 toLight=lightPosition-worldPosition;float distanceSquared=max(dot(toLight,toLight),1.0);vec3 lightDirection=toLight*inversesqrt(distanceSquared);
  float emitterCosine=max(dot(uKeyLightEmissionNormal,-lightDirection),0.0);float weight=quadratureWeight*emitterCosine/distanceSquared;
  diffuseSum+=max(dot(normal,lightDirection),0.0)*weight;
  vec3 halfDirection=normalize(lightDirection+viewDirection);
  specularSum+=pow(max(dot(normal,halfDirection),0.0),mix(160.0,8.0,roughness))*weight;weightSum+=weight;
 }
 diffuseTerm=weightSum>0.0?diffuseSum/weightSum:0.0;specularTerm=weightSum>0.0?specularSum/weightSum:0.0;
 if(uLightingContractV2==1&&uBallRectSpecular==1)
  specularTerm=authoredBallRectSpecular(worldPosition,normal,viewDirection,roughness);
}
void main(){
 vec3 base=uColor.rgb*vVertexColor.rgb;float roughness=uRoughness,metallic=uMetallic,ao=1.0;vec3 n=normalize(vNormal);
 if(uPbrMode==1){vec2 uv=vUv*uUvRepeat;base*=texture(uColorTexture,uv).rgb;vec3 tangentNormal=normalize(texture(uNormalTexture,uv).xyz*2.0-1.0);n=normalize(mat3(vTangent,vBitangent,vNormal)*tangentNormal);vec3 orm=texture(uOrmTexture,uv).rgb;ao=orm.r;roughness=orm.g;metallic=orm.b;}
 else if(uPbrMode==2){int tile=clamp(uIdentity,0,15);vec2 tilePixels=vec2(textureSize(uBallAtlas,0))/vec2(8.0,2.0);vec2 inset=vec2(24.0)/tilePixels;vec2 local=mix(inset,vec2(1.0)-inset,clamp(vUv,vec2(0.0),vec2(1.0)));base=texture(uBallAtlas,(vec2(float(tile%8),float(tile/8))+local)/vec2(8.0,2.0)).rgb;roughness=0.12;metallic=0.0;}
 if(uObjectIdMode==1){color=vec4(uColor.rgb,1.0);return;}
 if(uPocketSidewall==1){float depthLight=clamp((vWorldPosition.y-78.5)/3.1,0.0,1.0);base*=mix(0.55,1.45,depthLight);}
 vec3 l=normalize(uKeyLightCenterCm-vWorldPosition);float ndl=max(dot(n,l),0.0);vec3 viewDir=normalize(uCameraPosition-vWorldPosition);vec3 h=normalize(l+viewDir);
 vec3 f0=mix(vec3(0.04),base,metallic);float spec=pow(max(dot(n,h),0.0),mix(160.0,8.0,roughness));
 if(uPbrMode==2&&uBallAreaLightSamples>0)authoredBallDirectLight(vWorldPosition,n,viewDir,roughness,ndl,spec);
 float visibility=shadowVisibility();ndl*=visibility;spec*=visibility;
 vec3 reflectedDirection=reflect(-viewDir,n);vec3 irradiance=texture(uIrradianceMap,n).rgb;float reflectedLod=roughness*6.0;
 if(uPbrMode==2&&uBallLightingDiagnosticMode==6)reflectedLod=0.0;
 else if(uPbrMode==2&&uBallLightingDiagnosticMode==8)reflectedLod=2.0;
 vec3 reflected=textureLod(uSpecularMap,reflectedDirection,reflectedLod).rgb;
 float energy=1.0-0.25*uClearcoat;
 vec3 diffuseLight;vec3 reflection;
 if(uLightingContractV2==1){
  diffuseLight=vec3(uV2AmbientFloor+ndl*uDirectKeyIntensityScale)+irradiance*uV2IrradianceScale;
  reflection=f0*(spec*uDirectKeyIntensityScale+reflected*uV2ReflectedScale)+vec3(spec*uClearcoat*0.25*uDirectKeyIntensityScale);
 }else{
  diffuseLight=uPbrMode==2?vec3(0.12+0.85*ndl)+irradiance*0.55:vec3(0.55+1.35*ndl)+irradiance*2.0;
  reflection=f0*(spec*1.7+reflected*2.2)+vec3(spec*uClearcoat*0.25);
 }
 // Deliberate art approximation of unresolved pocket-mouth return light.
 // Only the explicitly bound inner draw can opt in; deep/other draws stay off.
 if(uPocketIndirectEnabled==1&&uPbrMode!=2&&uDirectKeyIntensityScale>0.0){
  int closest=0;float nearest=1e30;
  for(int i=0;i<6;++i){vec2 d=uPocketMouthsCm[i].xz-vWorldPosition.xz;float q=dot(d,d);if(q<nearest){nearest=q;closest=i;}}
  vec3 toMouth=uPocketMouthsCm[closest]-vWorldPosition;float depth=toMouth.y;
  float distanceSquared=dot(toMouth,toMouth);
  if(depth>0.0&&depth<18.0&&distanceSquared>1e-8){
   float openingFacing=max(dot(n,toMouth*inversesqrt(distanceSquared)),0.0);
   float weight=1.0-smoothstep(2.0,18.0,depth);
   float gain=min(0.4,max(uDirectKeyIntensityScale,0.0)*weight*openingFacing);
   diffuseLight+=gain*vec3(0.9162531152,1.0207696753,1.0408603316);
  }
 }
 vec3 diffuse=base*(1.0-metallic)*diffuseLight*energy;
 if(uPbrMode==2&&uBallLightingDiagnosticMode==1){color=vec4(base,uColor.a*vVertexColor.a);return;}
 if(uPbrMode==2&&uBallLightingDiagnosticMode==2){color=vec4(n*0.5+0.5,1.0);return;}
 if(uPbrMode==2&&uBallLightingDiagnosticMode==3){vec3 directLight=uLightingContractV2==1?vec3(ndl*uDirectKeyIntensityScale):vec3(0.12+0.85*ndl);color=vec4(base*(1.0-metallic)*directLight*energy*ao,uColor.a*vVertexColor.a);return;}
 if(uPbrMode==2&&uBallLightingDiagnosticMode==4){color=vec4(reflection*ao,uColor.a*vVertexColor.a);return;}
 if(uPbrMode==2&&uBallLightingDiagnosticMode==5){float indirectScale=uLightingContractV2==1?uV2IrradianceScale:0.55;color=vec4(base*(1.0-metallic)*irradiance*indirectScale*energy*ao,uColor.a*vVertexColor.a);return;}
 if(uPbrMode==2&&uBallLightingDiagnosticMode>=6&&uBallLightingDiagnosticMode<=8){float reflectedScale=uLightingContractV2==1?uV2ReflectedScale:2.2;color=vec4(f0*reflected*reflectedScale*ao,uColor.a*vVertexColor.a);return;}
 if(uPbrMode==2&&uBallLightingDiagnosticMode==9){color=vec4(reflectedDirection*0.5+0.5,1.0);return;}
 color=vec4((diffuse+reflection)*ao,uColor.a*vVertexColor.a);
})GLSL";

// Only compiled for explicit sparse-furniture candidates. The original mesh
// fragment remains byte-for-byte unchanged for table, balls and default venue.
std::string sparsePropReceiverFragment() {
  std::string shader(kMeshFragment);
  const std::string seam = "float shadowDepthCompare(";
  const auto insertion = shader.find(seam);
  if (insertion == std::string::npos) throw std::runtime_error("sparse_shadow_shader_seam_missing");
  shader.insert(insertion, R"GLSL(
uniform sampler2D uSparsePropDepth;
uniform mat4 uSparsePropViewProjection;
uniform int uSparsePropActive;
float sparsePropCompare(ivec2 p,ivec2 sizePx,float depth){
 if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,sizePx)))return 1.0;
 return depth<=texelFetch(uSparsePropDepth,p,0).r?1.0:0.0;
}
float sparsePropVisibility(){
 if(uSparsePropActive==0||uDirectKeyIntensityScale<=0.0)return 1.0;
 vec4 clip=uSparsePropViewProjection*vec4(vWorldPosition,1.0);
 if(clip.w<=0.0)return 1.0;
 vec3 q=clip.xyz/clip.w*0.5+0.5;
 if(any(lessThan(q,vec3(0)))||any(greaterThan(q,vec3(1))))return 1.0;
 ivec2 sizePx=textureSize(uSparsePropDepth,0);
 vec2 pixel=q.xy*vec2(sizePx)-0.5;ivec2 p=ivec2(floor(pixel));vec2 f=fract(pixel);
 // Comparison-result filtering, not interpolated blocker depths. Independent
 // 0.05cm lightward receiver bias; the table's bias/taps/frustum stay intact.
 vec3 biasDirection=normalize(uKeyLightCenterCm-vWorldPosition);
 vec4 biased=uSparsePropViewProjection*vec4(vWorldPosition+biasDirection*0.05,1.0);
 float d=biased.z/biased.w*0.5+0.5;
 return mix(mix(sparsePropCompare(p,sizePx,d),sparsePropCompare(p+ivec2(1,0),sizePx,d),f.x),
            mix(sparsePropCompare(p+ivec2(0,1),sizePx,d),sparsePropCompare(p+ivec2(1,1),sizePx,d),f.x),f.y);
}
)GLSL");
  const std::string old="float visibility=shadowVisibility();";
  const auto position=shader.find(old);
  if(position==std::string::npos)throw std::runtime_error("sparse_shadow_visibility_seam_missing");
  shader.replace(position,old.size(),"float visibility=shadowVisibility()*sparsePropVisibility();");
  return shader;
}

const char *kMeshDepthVertex = R"GLSL(#version 330 core
layout(location=0) in vec3 aPosition;
uniform mat4 uLightMvp;
uniform mat4 uBallLightMvp[16];
uniform int uInstancedBalls;
void main(){
  if(uInstancedBalls!=0)
    gl_Position=uBallLightMvp[gl_InstanceID]*vec4(aPosition,1.0);
  else
    gl_Position=uLightMvp*vec4(aPosition,1.0);
}
)GLSL";

const char *kMeshDepthFragment = R"GLSL(#version 330 core
void main(){}
)GLSL";

using Matrix = std::array<float, 16>;

Matrix multiply(const Matrix &a, const Matrix &b) {
  Matrix c{};
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      for (int k = 0; k < 4; ++k)
        c[static_cast<std::size_t>(col * 4 + row)] +=
            a[static_cast<std::size_t>(k * 4 + row)] *
            b[static_cast<std::size_t>(col * 4 + k)];
  return c;
}
Matrix perspective(float fov, float aspect, float nearPlane, float farPlane) {
  Matrix m{};
  const float q = 1.0f / std::tan(fov * 0.5f);
  m[0] = q / aspect;
  m[5] = q;
  m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
  m[11] = -1;
  m[14] = (2 * farPlane * nearPlane) / (nearPlane - farPlane);
  return m;
}
Matrix orthographic(float left, float right, float bottom, float top,
                    float nearPlane, float farPlane) {
  Matrix m{};
  m[0] = 2.0f / (right - left);
  m[5] = 2.0f / (top - bottom);
  m[10] = -2.0f / (farPlane - nearPlane);
  m[12] = -(right + left) / (right - left);
  m[13] = -(top + bottom) / (top - bottom);
  m[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
  m[15] = 1.0f;
  return m;
}
std::array<float, 3> normalize3(std::array<float, 3> v) {
  const float length =
      std::max(std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]), 0.00001f);
  for (float &value : v)
    value /= length;
  return v;
}
std::array<float, 3> cross3(const std::array<float, 3> &a,
                            const std::array<float, 3> &b) {
  return {{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
           a[0] * b[1] - a[1] * b[0]}};
}
float dot3(const std::array<float, 3> &a, const std::array<float, 3> &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

std::optional<int> ballAreaLightDiagnosticSampleOverride() {
  const char *value = std::getenv("BILLIARDGL_BALL_AREA_LIGHT_DIAGNOSTIC_SAMPLES");
  if (value == nullptr || value[0] == '\0')
    return std::nullopt;
  if (std::strcmp(value, "0") == 0)
    return 0;
  if (std::strcmp(value, "1") == 0)
    return 1;
  if (std::strcmp(value, "2") == 0)
    return 2;
  if (std::strcmp(value, "4") == 0)
    return 4;
  if (std::strcmp(value, "16") == 0)
    return 16;
  throw std::runtime_error(
      "BILLIARDGL_BALL_AREA_LIGHT_DIAGNOSTIC_SAMPLES must be 0, 1, 2, 4 or 16");
}

std::optional<float> ballShadowBiasScaleDiagnosticRollback() {
  const char *value =
      std::getenv("BILLIARDGL_BALL_SHADOW_BIAS_SCALE_DIAGNOSTIC");
  if (value == nullptr || value[0] == '\0')
    return std::nullopt;
  if (std::strcmp(value, "1") == 0)
    return 1.0f;
  if (std::strcmp(value, "2") == 0)
    return 2.0f;
  if (std::strcmp(value, "4") == 0)
    return 4.0f;
  if (std::strcmp(value, "8") == 0)
    return 8.0f;
  throw std::runtime_error(
      "BILLIARDGL_BALL_SHADOW_BIAS_SCALE_DIAGNOSTIC must be 1, 2, 4 or 8");
}

int ballLightingDiagnosticMode() {
  const char *value = std::getenv("BILLIARDGL_BALL_LIGHTING_DIAGNOSTIC_MODE");
  if (value == nullptr || value[0] == '\0' || std::strcmp(value, "default") == 0)
    return 0;
  if (std::strcmp(value, "unlit_albedo") == 0)
    return 1;
  if (std::strcmp(value, "normal_rgb") == 0)
    return 2;
  if (std::strcmp(value, "direct_only") == 0)
    return 3;
  if (std::strcmp(value, "specular_reflected_only") == 0)
    return 4;
  if (std::strcmp(value, "irradiance_only") == 0)
    return 5;
  if (std::strcmp(value, "reflected_lod0_only") == 0)
    return 6;
  if (std::strcmp(value, "reflected_current_lod_only") == 0)
    return 7;
  if (std::strcmp(value, "reflected_coarse_lod_only") == 0)
    return 8;
  if (std::strcmp(value, "reflected_direction_rgb") == 0)
    return 9;
  throw std::runtime_error(
      "BILLIARDGL_BALL_LIGHTING_DIAGNOSTIC_MODE must be default, "
      "unlit_albedo, normal_rgb, direct_only, specular_reflected_only or "
      "irradiance_only, reflected_lod0_only, reflected_current_lod_only or "
      "reflected_coarse_lod_only or reflected_direction_rgb");
}

bool seamlessCubemapEnabled() {
  const char *value =
      std::getenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC");
  // Seamless filtering is the production default.  Keeping the explicit zero
  // override gives image comparisons a deterministic rollback path without
  // making the shipping renderer inherit the non-seamless OpenGL default.
  if (value == nullptr || value[0] == '\0' || std::strcmp(value, "1") == 0)
    return true;
  if (std::strcmp(value, "0") == 0)
    return false;
  throw std::runtime_error(
      "BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC must be 0 or 1");
}
float gltfContactShadowCompensationScale() {
  const char *value =
      std::getenv("BILLIARDGL_GLTF_CONTACT_SHADOW_COMPENSATION");
  if (value == nullptr || value[0] == '\0' ||
      std::strcmp(value, "balanced") == 0)
    return 0.25f;
  if (std::strcmp(value, "0") == 0)
    return 0.0f;
  if (std::strcmp(value, "1") == 0 || std::strcmp(value, "legacy") == 0)
    return 1.0f;
  throw std::runtime_error(
      "BILLIARDGL_GLTF_CONTACT_SHADOW_COMPENSATION must be balanced, 0 or 1");
}
bool gltfBallEligibleForClothContactCompensation(
    const PresentationBall &ball) {
  // This screen-space ellipse is only a low-strength contact cue for a ball
  // actually resting on the authored cloth.  Real airborne and off-table
  // shadows come from the light-space depth pass at the ball's full 3D
  // position; projecting those balls back onto this fixed plane is false.
  constexpr float kClothSurfaceYCm = 86.483976f;
  constexpr float kBallRadiusCm = 2.8575f;
  constexpr float kSupportedCenterToleranceCm = 0.05f;
  constexpr float kPlayfieldHalfWidthCm = 63.5f;
  constexpr float kPlayfieldHalfLengthCm = 127.0f;
  const float supportedCenter = kClothSurfaceYCm + kBallRadiusCm;
  return !ball.pocketed &&
         std::abs(ball.position.y - supportedCenter) <=
             kSupportedCenterToleranceCm &&
         std::abs(ball.position.x) <=
             kPlayfieldHalfWidthCm + kSupportedCenterToleranceCm &&
         std::abs(ball.position.z) <=
             kPlayfieldHalfLengthCm + kSupportedCenterToleranceCm;
}
Matrix lookAt(const std::array<float, 3> &eye,
              const std::array<float, 3> &target) {
  auto f = normalize3(
      {{target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]}});
  const std::array<float, 3> worldUp{{0, 1, 0}};
  const std::array<float, 3> stableUp = std::abs(dot3(f, worldUp)) > 0.98f
                                            ? std::array<float, 3>{{0, 0, 1}}
                                            : worldUp;
  auto s = normalize3(cross3(f, stableUp));
  auto u = cross3(s, f);
  return {{s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0,
           -dot3(s, eye), -dot3(u, eye), dot3(f, eye), 1}};
}
Matrix tableShadowViewProjection(const LightingRigDefinition &lighting) {
  const std::array<float, 3> eye{{lighting.keyPositionMetres[0] * 100.0f,
                                  lighting.keyPositionMetres[1] * 100.0f,
                                  lighting.keyPositionMetres[2] * 100.0f}};
  const auto direction = normalize3(lighting.keyEmissionNormal);
  const std::array<float, 3> target{{eye[0] + direction[0] * 100.0f,
                                     eye[1] + direction[1] * 100.0f,
                                     eye[2] + direction[2] * 100.0f}};
  // Stable world-space coverage for the complete authored table, rails,
  // pocket structures and any currently visible ball/cue over the play area.
  // Receivers outside this volume are explicitly fail-lit in the fragment
  // shader, so off-table balls never project a border artifact onto cloth.
  constexpr float horizontalHalfExtentCm = 130.0f;
  constexpr float longitudinalHalfExtentCm = 210.0f;
  return multiply(orthographic(-horizontalHalfExtentCm,
                               horizontalHalfExtentCm,
                               -longitudinalHalfExtentCm,
                               longitudinalHalfExtentCm, 1.0f, 190.0f),
                  lookAt(eye, target));
}
Matrix ballModel(const PresentationBall &ball) {
  const float angle = ball.rotationAngleDegrees * 0.0174532925f;
  auto a = normalize3(
      {{ball.rotationAxis.x, ball.rotationAxis.y, ball.rotationAxis.z}});
  const float c = std::cos(angle), s = std::sin(angle), t = 1 - c;
  Matrix m{{t * a[0] * a[0] + c, t * a[0] * a[1] + s * a[2],
            t * a[0] * a[2] - s * a[1], 0, t * a[0] * a[1] - s * a[2],
            t * a[1] * a[1] + c, t * a[1] * a[2] + s * a[0], 0,
            t * a[0] * a[2] + s * a[1], t * a[1] * a[2] - s * a[0],
            t * a[2] * a[2] + c, 0, ball.position.x, ball.position.y,
            ball.position.z, 1}};
  return m;
}
Matrix cueModel(const CuePresentation &cue) {
  const auto shot = normalize3({{cue.direction.x, 0.0f, cue.direction.z}});
  const std::array<float, 3> z{{-shot[0], 0.0f, -shot[2]}};
  const std::array<float, 3> y{{0.0f, 1.0f, 0.0f}};
  const auto x = normalize3(cross3(y, z));
  // The generated cue tip begins at local z=0, so its world origin belongs
  // just behind the 2.8575 cm ball radius.  Keep only a small physical safety
  // gap; camera-dependent oversized backoff makes the cue appear to float.
  const float backoff = std::max(cue.modelOriginBackoffCm, 3.4f);
  constexpr float radialScale = 0.42f;
  constexpr float lengthScale = 0.86f;
  return {{x[0] * radialScale, x[1] * radialScale, x[2] * radialScale, 0,
           y[0] * radialScale, y[1] * radialScale, y[2] * radialScale, 0,
           z[0] * lengthScale, z[1] * lengthScale, z[2] * lengthScale, 0,
           cue.referencePosition.x - shot[0] * backoff, cue.referencePosition.y,
           cue.referencePosition.z - shot[2] * backoff, 1}};
}

const char *kToneFragment = R"GLSL(#version 400 core
in vec2 vUv; layout(location=0) out vec4 color; uniform sampler2D uHdr; uniform float uExposureScale;
uniform float uBloomStrength;
uniform int uOutputLinear;
uniform sampler2D uSceneDepth;
uniform float uDofRadius;
uniform float uFocusDistance;
uniform vec2 uDepthProjection;
float distanceFromDepth(float depth){
    float ndc=depth*2.0-1.0;
    return uDepthProjection.y/(ndc+uDepthProjection.x);
}
float viewDistance(vec2 uv){return distanceFromDepth(texture(uSceneDepth,uv).r);}
vec3 aces(vec3 x){float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);}
void main(){
    vec3 linearColor=texture(uHdr,vUv).rgb*uExposureScale;
    if(uDofRadius>0.0){
        vec2 size=vec2(textureSize(uHdr,0));
        float distance=viewDistance(vUv);
        float coc=clamp((abs(distance-uFocusDistance)-20.0)/max(uFocusDistance,20.0),0.0,1.0);
        // A zero circle of confusion is exactly the already sampled center.
        // Avoid 36 redundant depth/color fetches throughout the focus band.
        if(coc>0.0){
            vec2 spacing=vec2(uDofRadius*coc*size.y/1080.0)/size;
            vec3 sum=vec3(0.0); float weights=0.0;
            float depthAcceptStart=max(2.0,distance*0.02);
            float depthAcceptEnd=max(4.0,distance*0.05);
            for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x){
                vec2 uv=clamp(vUv+vec2(x,y)*spacing,0.5/size,1.0-0.5/size);
                // Depth-filter each bilinear contributor before mixing color.
                // A nearest depth lookup paired with filtered color leaks a
                // background texel into a narrow foreground silhouette.
                vec2 pixel=uv*size-0.5;
                ivec2 base=ivec2(floor(pixel)); vec2 fraction=fract(pixel);
                // One gather proves the common continuous-depth case. The
                // component order is irrelevant here: any rejected neighbor
                // takes the exact per-contributor silhouette path below.
                vec4 gatheredDepth=textureGather(uSceneDepth,uv);
                bool fullDepthSupport=true;
                for(int index=0;index<4;++index){
                    float delta=abs(distanceFromDepth(gatheredDepth[index])-distance);
                    fullDepthSupport=fullDepthSupport&&
                        1.0-smoothstep(depthAcceptStart,depthAcceptEnd,delta)>=1.0;
                }
                float kernelWeight=float((x==0?2:1)*(y==0?2:1));
                if(fullDepthSupport){
                    // With no depth rejection, hardware linear filtering is
                    // the same four-contributor interpolation. Fetch color
                    // once; silhouettes retain the explicit fallback below.
                    sum+=texture(uHdr,uv).rgb*kernelWeight;
                    weights+=kernelWeight;
                }else{
                    for(int iy=0;iy<2;++iy) for(int ix=0;ix<2;++ix){
                        float bilinear=(ix==0?1.0-fraction.x:fraction.x)*
                            (iy==0?1.0-fraction.y:fraction.y);
                        if(bilinear<=0.0)continue;
                        ivec2 tap=clamp(base+ivec2(ix,iy),ivec2(0),ivec2(size)-1);
                        float delta=abs(distanceFromDepth(
                            texelFetch(uSceneDepth,tap,0).r)-distance);
                        float weight=kernelWeight*bilinear*
                            (1.0-smoothstep(depthAcceptStart,depthAcceptEnd,delta));
                        if(weight<=0.0)continue;
                        sum+=texelFetch(uHdr,tap,0).rgb*weight;
                        weights+=weight;
                    }
                }
            }
            linearColor=sum/max(weights,0.0001)*uExposureScale;
        }
    }
    if(uBloomStrength>0.0){
        vec2 size=vec2(textureSize(uHdr,0));
        vec2 spacing=vec2(3.0*size.y/1080.0)/size;
        vec3 glow=vec3(0.0);
        // Normalized separable 1-2-1 kernel. Threshold in exposed linear
        // space, before ACES; never blur the base scene or the later HUD.
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x){
            vec2 uv=clamp(vUv+vec2(x,y)*spacing,0.5/size,1.0-0.5/size);
            vec3 sampleColor=max(texture(uHdr,uv).rgb*uExposureScale,vec3(0.0));
            float luminance=dot(sampleColor,vec3(0.2126,0.7152,0.0722));
            float weight=float((x==0?2:1)*(y==0?2:1))/16.0;
            glow+=sampleColor*(max(luminance-1.0,0.0)/max(luminance,0.0001))*weight;
        }
        linearColor+=uBloomStrength*glow;
    }
    color=vec4(uOutputLinear!=0 ? linearColor/max(uExposureScale,0.000001)
                              : aces(linearColor),1.0);
}
)GLSL";

const char *kTrajectoryOverlayVertex = R"GLSL(#version 330 core
layout(location=0) in vec2 aPhysicalPosition;
layout(location=1) in vec4 aColor;
layout(location=2) in uint aFinalOwnershipToken;
layout(location=3) in uint aVisualSublayer;
layout(location=4) in uint aAdaptiveInnerHaloWhite;
uniform vec2 uPhysicalViewport;
out vec4 vColor;
flat out uint vFinalOwnershipToken;
flat out uint vVisualSublayer;
flat out uint vAdaptiveInnerHaloWhite;
void main(){
  vec2 ndc=vec2(aPhysicalPosition.x/uPhysicalViewport.x*2.0-1.0,
                1.0-aPhysicalPosition.y/uPhysicalViewport.y*2.0);
  gl_Position=vec4(ndc,0.0,1.0);
  vColor=aColor;
  vFinalOwnershipToken=aFinalOwnershipToken;
  vVisualSublayer=aVisualSublayer;
  vAdaptiveInnerHaloWhite=aAdaptiveInnerHaloWhite;
}
)GLSL";

const char *kTrajectoryOverlayFragment = R"GLSL(#version 330 core
in vec4 vColor;flat in uint vFinalOwnershipToken;
flat in uint vVisualSublayer;
flat in uint vAdaptiveInnerHaloWhite;
layout(location=0)out vec4 color;
layout(location=1)out vec4 finalOwnershipToken;
uniform sampler2D uPreOverlayBackdropHdr;
uniform sampler2D uSceneBackdropHdr;
uniform float uExposureScale;
vec3 aces(vec3 x){float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);}
float luma(vec3 x){return dot(x,vec3(0.2126,0.7152,0.0722));}
float contrastRatio(vec3 a,vec3 b){float al=luma(a),bl=luma(b);return (max(al,bl)+0.05)/(min(al,bl)+0.05);}
void main(){
  if(vVisualSublayer==0u){
    ivec2 sizePx=textureSize(uPreOverlayBackdropHdr,0);
    ivec2 px=clamp(ivec2(gl_FragCoord.xy),ivec2(0),sizePx-ivec2(1));
    vec3 currentHdr=texelFetch(uPreOverlayBackdropHdr,px,0).rgb;
    vec3 sceneHdr=texelFetch(uSceneBackdropHdr,px,0).rgb;
    vec3 backdropFinal=aces(currentHdr*uExposureScale);
    vec3 innerLinear=vAdaptiveInnerHaloWhite!=0u?vec3(8.0):vec3(0.0);
    vec3 innerFinal=aces(innerLinear*uExposureScale);
    bool priorActualCoverage=vAdaptiveInnerHaloWhite==0u&&any(greaterThan(abs(currentHdr-sceneHdr),vec3(0.00001)));
    if(!priorActualCoverage&&contrastRatio(innerFinal,backdropFinal)>=4.0)discard;
    vec3 blackFinal=vec3(0.0);
    vec3 whiteFinal=aces(vec3(8.0)*uExposureScale);
    color=contrastRatio(whiteFinal,backdropFinal)>=contrastRatio(blackFinal,backdropFinal)?vec4(8.0,8.0,8.0,1.0):vec4(0.0,0.0,0.0,1.0);
  }else color=vColor;
  uvec3 tokenBytes=uvec3(vFinalOwnershipToken,
      vFinalOwnershipToken>>8u,vFinalOwnershipToken>>16u)&uvec3(255u);
  // Alpha one makes the ordinary production SRC_ALPHA blend an exact
  // overwrite on this evidence attachment.  The CPU producer rejects tokens
  // outside the explicitly encoded 24-bit sequential identity domain.
  finalOwnershipToken=vec4(vec3(tokenBytes)/255.0,1.0);
}
)GLSL";

// The single-sample and MSAA resolve routes are separate linked programs.
// OpenGL forbids different active sampler types from aliasing one texture unit,
// even when a runtime branch would access only one of them.
const char *kFinalOwnershipResolveSingleFragment = R"GLSL(#version 330 core
in vec2 vUv;
layout(location=0)out uvec4 resolvedOwnerTokens;
uniform sampler2D uSemanticSingle;
uint decodeOwner(vec4 encoded){
  uvec4 bytes=uvec4(round(clamp(encoded,0.0,1.0)*255.0));
  return bytes.x|(bytes.y<<8u)|(bytes.z<<16u);
}
void main(){
  ivec2 sizePx=textureSize(uSemanticSingle,0);
  ivec2 px=clamp(ivec2(vUv*vec2(sizePx)),ivec2(0),sizePx-ivec2(1));
  resolvedOwnerTokens=uvec4(decodeOwner(texelFetch(uSemanticSingle,px,0)),
      0u,0u,0u);
}
)GLSL";

// Copies every production MSAA owner token without collapsing partial
// coverage. RGBA stores sample indices 0..3; unused lanes remain zero.
const char *kFinalOwnershipResolveMsaaFragment = R"GLSL(#version 330 core
in vec2 vUv;
layout(location=0)out uvec4 resolvedOwnerTokens;
uniform sampler2DMS uSemanticMsaa;
uniform int uSampleCount;
uint decodeOwner(vec4 encoded){
  uvec4 bytes=uvec4(round(clamp(encoded,0.0,1.0)*255.0));
  return bytes.x|(bytes.y<<8u)|(bytes.z<<16u);
}
void main(){
  ivec2 sizePx=textureSize(uSemanticMsaa);
  ivec2 px=clamp(ivec2(vUv*vec2(sizePx)),ivec2(0),sizePx-ivec2(1));
  resolvedOwnerTokens=uvec4(0u);
  if(uSampleCount>0)resolvedOwnerTokens.x=decodeOwner(texelFetch(uSemanticMsaa,px,0));
  if(uSampleCount>1)resolvedOwnerTokens.y=decodeOwner(texelFetch(uSemanticMsaa,px,1));
  if(uSampleCount>2)resolvedOwnerTokens.z=decodeOwner(texelFetch(uSemanticMsaa,px,2));
  if(uSampleCount>3)resolvedOwnerTokens.w=decodeOwner(texelFetch(uSemanticMsaa,px,3));
}
)GLSL";

const char *kTrajectoryInsetVertex = R"GLSL(#version 330 core
layout(location=0)in vec2 aPhysical;
layout(location=1)in vec4 aColor;
layout(location=2)in uint aToken;
uniform vec2 uPlotOrigin;
uniform vec2 uPlotSize;
out vec4 vColor;
flat out uint vToken;
void main(){
  vec2 local=(aPhysical-uPlotOrigin)/uPlotSize;
  gl_Position=vec4(local.x*2.0-1.0,1.0-local.y*2.0,0.0,1.0);
  vColor=aColor;
  vToken=aToken;
}
)GLSL";

const char *kTrajectoryInsetFragment = R"GLSL(#version 330 core
in vec4 vColor;
flat in uint vToken;
layout(location=0)out vec4 outColor;
layout(location=1)out vec4 outSemantic;
void main(){
  outColor=vColor;
  uvec4 tokenBytes=uvec4(vToken,vToken>>8u,vToken>>16u,vToken>>24u)&uvec4(255u);
  outSemantic=vec4(tokenBytes)/255.0;
}
)GLSL";

const char *kTrajectoryInsetFullscreenVertex = R"GLSL(#version 330 core
out vec2 vUv;
void main(){
  vec2 p=vec2((gl_VertexID==1||gl_VertexID==2||gl_VertexID==4)?1.0:-1.0,
              (gl_VertexID==2||gl_VertexID==4||gl_VertexID==5)?1.0:-1.0);
  vUv=p*0.5+0.5;
  gl_Position=vec4(p,0.0,1.0);
}
)GLSL";

const char *kTrajectoryInsetSemanticResolveSingle = R"GLSL(#version 330 core
in vec2 vUv;
layout(location=0)out uvec4 outTokens;
uniform sampler2D uSemantic;
uint decodeOwner(vec4 encoded){
  uvec4 b=uvec4(round(clamp(encoded,0.0,1.0)*255.0));
  return b.x|(b.y<<8u)|(b.z<<16u)|(b.w<<24u);
}
void main(){
  ivec2 s=textureSize(uSemantic,0);
  ivec2 p=clamp(ivec2(vUv*vec2(s)),ivec2(0),s-ivec2(1));
  outTokens=uvec4(decodeOwner(texelFetch(uSemantic,p,0)),0u,0u,0u);
}
)GLSL";

const char *kTrajectoryInsetSemanticResolveMsaa = R"GLSL(#version 330 core
in vec2 vUv;
layout(location=0)out uvec4 outTokens;
uniform sampler2DMS uSemantic;
uniform int uSampleCount;
uint decodeOwner(vec4 encoded){
  uvec4 b=uvec4(round(clamp(encoded,0.0,1.0)*255.0));
  return b.x|(b.y<<8u)|(b.z<<16u)|(b.w<<24u);
}
void main(){
  ivec2 s=textureSize(uSemantic);
  ivec2 p=clamp(ivec2(vUv*vec2(s)),ivec2(0),s-ivec2(1));
  outTokens=uvec4(0u);
  if(uSampleCount>0)outTokens.x=decodeOwner(texelFetch(uSemantic,p,0));
  if(uSampleCount>1)outTokens.y=decodeOwner(texelFetch(uSemantic,p,1));
  if(uSampleCount>2)outTokens.z=decodeOwner(texelFetch(uSemantic,p,2));
  if(uSampleCount>3)outTokens.w=decodeOwner(texelFetch(uSemantic,p,3));
}
)GLSL";

const char *kTrajectoryInsetToneFragment = R"GLSL(#version 330 core
in vec2 vUv;
layout(location=0)out vec4 outSrgba;
uniform sampler2D uLinearColor;
vec3 encodeSrgb(vec3 c){
  bvec3 low=lessThanEqual(c,vec3(0.0031308));
  vec3 a=c*12.92;
  vec3 b=1.055*pow(max(c,vec3(0.0)),vec3(1.0/2.4))-0.055;
  return mix(b,a,low);
}
void main(){vec4 c=texelFetch(uLinearColor,ivec2(gl_FragCoord.xy),0);
  outSrgba=vec4(encodeSrgb(clamp(c.rgb,0.0,1.0)),clamp(c.a,0.0,1.0));}
)GLSL";

void checkGl(const char *pass) {
  const GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    throw std::runtime_error(std::string("ModernRenderer OpenGL error after ") +
                             pass + ": " + std::to_string(error));
  }
}

} // namespace

bool validateTrajectoryInsetPackageForSubmission(
    const training::TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetAtomicFrameInput& input, std::string& error) {
  return validateTrajectoryInsetPackageIdentity(package, input, error);
}

// Real GL adapter for the CP3 transaction. It is intentionally not called by
// renderFrame: CP4 must bind live session/layout authority before a prepared
// inset may become visible. The CP3 publish step is an atomic CPU publication
// of the fully rendered offscreen staging texture only.
class ModernTrajectoryInsetGlDispatch final
    : public modern::TrajectoryInsetAtomicDispatch {
public:
  ModernTrajectoryInsetGlDispatch(
      const training::TrajectoryInsetDrawPackage &package,
      const ui::TrajectoryInsetShellLayout &)
      : package_(package) {}

  // Bounded native test, using the production allocation/draw/resolve path.
  // The caller supplies four pixel-sized quads; nothing is presented.
  bool verifyTokenCodec(std::string& error) {
    const auto original = snapshotState();
    bool passed = false;
    try {
      glEnable(GL_DITHER);
      glDisablei(GL_BLEND, 0);
      glEnablei(GL_BLEND, 1);
      const auto challenged = snapshotState();
      const auto run = [&]() {
        modern::TrajectoryInsetAtomicRequest request;
        request.width = request.height = 2;
        request.samples = request.semanticSamples = 4;
        modern::TrajectoryInsetAtomicResources resources;
        if (!queryFormat(request, resources, error) ||
            !allocateColor(request, resources, error) ||
            !allocateSemantic(request, resources, error) ||
            !uploadGeometry(resources, error) ||
            !attachFramebuffers(request, resources, error)) return false;
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, semantic_.id());
        GLint samples = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &samples);
        if (samples != 4) { error = "inset_codec_exact_4x_unavailable"; return false; }
        bindDrawTarget(); clearColor(); clearSemantic(); drawFixedTableColor();
        if (!drawTrajectory(error) || !resolveColor(request,error) ||
            !resolveSemantic(request,error)) return false;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        std::array<GLuint,16> tokens{};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, semanticResolveFramebuffer_.id());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0,0,2,2,GL_RGBA_INTEGER,GL_UNSIGNED_INT,tokens.data());
        for (int y=0;y<2;++y) for(int x=0;x<2;++x) for(int sample=0;sample<4;++sample)
          if (tokens[(y*2+x)*4+sample] != package_.semanticBatches[(1-y)*2+x].insetOwnershipToken) {
            error = "inset_codec_32bit_sample_roundtrip_failed"; return false;
          }
        const auto readColor = [&]() {
          std::array<float,16> values{};
          glBindFramebuffer(GL_READ_FRAMEBUFFER,colorResolveFramebuffer_.id());
          glReadBuffer(GL_COLOR_ATTACHMENT0);
          glReadPixels(0,0,2,2,GL_RGBA,GL_FLOAT,values.data());
          return values;
        };
        const auto actual = readColor();
        bindDrawTarget(); clearColor(); drawFixedTableColor();
        if (!drawTrajectory(error, false) || !resolveColor(request,error)) return false;
        const auto reference = readColor();
        if (std::memcmp(actual.data(),reference.data(),sizeof(actual)) != 0) {
          error = "inset_codec_color_changed"; return false;
        }
        if (glGetError() != GL_NO_ERROR) { error = "inset_codec_gl_error"; return false; }
        return true;
      };
      passed = run();
      std::string restoreError;
      if (!restoreState(challenged,restoreError) || !(snapshotState() == challenged)) {
        error = "inset_codec_indexed_blend_or_dither_restore_failed:" + restoreError;
        passed = false;
      }
    } catch (const std::exception& failure) {
      error = std::string("inset_codec_exception:") + failure.what();
    }
    std::string restoreError;
    const bool restored = restoreState(original,restoreError);
    const bool same = snapshotState() == original;
    const GLenum restoreGlError = glGetError();
    if (!restored || !same || restoreGlError != GL_NO_ERROR) {
      error = "inset_codec_outer_state_restore_failed:checks=" + std::to_string(passed) +
          ":same=" + std::to_string(same) + ":gl=" + std::to_string(restoreGlError) +
          ":vao=" + std::to_string(original.vertexArray) + ':' + restoreError;
      return false;
    }
    return passed;
  }

  bool invoke(modern::TrajectoryInsetAtomicStage stage,
              const modern::TrajectoryInsetAtomicRequest &request,
              modern::TrajectoryInsetAtomicResources &resources,
              modern::TrajectoryInsetAtomicLedger &ledger,
              std::string &error) override {
    restored_ = false;
    switch (stage) {
    case modern::TrajectoryInsetAtomicStage::FormatSampleQuery:
      return queryFormat(request, resources, error);
    case modern::TrajectoryInsetAtomicStage::ColorAllocation:
      return allocateColor(request, resources, error);
    case modern::TrajectoryInsetAtomicStage::SemanticR32uiAllocation:
      return allocateSemantic(request, resources, error);
    case modern::TrajectoryInsetAtomicStage::GeometryUpload:
      return uploadGeometry(resources, error);
    case modern::TrajectoryInsetAtomicStage::FramebufferAttachComplete:
      return attachFramebuffers(request, resources, error);
    case modern::TrajectoryInsetAtomicStage::ColorClear:
      bindDrawTarget();
      clearColor();
      ++ledger.offscreenColorWrites;
      return true;
    case modern::TrajectoryInsetAtomicStage::SemanticClear:
      bindDrawTarget();
      clearSemantic();
      ++ledger.offscreenSemanticWrites;
      return true;
    case modern::TrajectoryInsetAtomicStage::TableDraw:
      bindDrawTarget();
      drawFixedTableColor();
      ++ledger.offscreenColorWrites;
      return true;
    case modern::TrajectoryInsetAtomicStage::TrajectoryDraw:
      if (!drawTrajectory(error))
        return false;
      ++ledger.offscreenColorWrites;
      ++ledger.offscreenSemanticWrites;
      return true;
    case modern::TrajectoryInsetAtomicStage::ColorResolve:
      if (!requireFramebufferComplete(colorResolveFramebuffer_, "color_resolve",
                                      error))
        return false;
      if (!resolveColor(request, error))
        return false;
      ++ledger.offscreenColorWrites;
      return true;
    case modern::TrajectoryInsetAtomicStage::PerSampleSemanticResolve:
      if (!requireFramebufferComplete(semanticResolveFramebuffer_,
                                      "semantic_resolve", error))
        return false;
      if (!resolveSemantic(request, error))
        return false;
      ++ledger.offscreenSemanticWrites;
      return true;
    case modern::TrajectoryInsetAtomicStage::FixedPaletteToneEncode:
      if (!requireFramebufferComplete(toneFramebuffer_, "tone_encode", error))
        return false;
      if (!toneEncode(error))
        return false;
      ++ledger.offscreenColorWrites;
      return true;
    case modern::TrajectoryInsetAtomicStage::GenerationFrameSerialCheck:
      return validateInstrumentation(request, ledger, error);
    case modern::TrajectoryInsetAtomicStage::AtomicComposite:
      if (!requireFramebufferComplete(stagingFramebuffer_,
                                      "atomic_composite_staging", error))
        return false;
      if (!stageAtomicComposite(error))
        return false;
      ++ledger.offscreenColorWrites;
      return true;
    case modern::TrajectoryInsetAtomicStage::StateRestore:
    case modern::TrajectoryInsetAtomicStage::None:
      error = "trajectory_inset_invalid_dispatch_stage";
      return false;
    }
    error = "trajectory_inset_unknown_dispatch_stage";
    return false;
  }

  modern::TrajectoryInsetGlState snapshotState() override {
    modern::TrajectoryInsetGlState state;
    GLint value = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    state.drawFramebuffer = static_cast<std::uint64_t>(value);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
    state.readFramebuffer = static_cast<std::uint64_t>(value);
    glGetIntegerv(GL_VIEWPORT, state.viewport);
    state.scissorEnabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    glGetIntegerv(GL_SCISSOR_BOX, state.scissorBox);
    state.blendEnabled = glIsEnabled(GL_BLEND) == GL_TRUE;
    state.ditherEnabled = glIsEnabled(GL_DITHER) == GL_TRUE;
    state.sampleAlphaToOneEnabled = glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE) == GL_TRUE;
    GLboolean colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    for (std::size_t channel = 0; channel < 4; ++channel)
      state.colorWriteMask[channel] = colorMask[channel] == GL_TRUE;
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &state.blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state.blendEquationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &state.blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &state.blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blendDstAlpha);
    state.depthTestEnabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    GLboolean depthWrite = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
    state.depthWriteEnabled = depthWrite == GL_TRUE;
    glGetIntegerv(GL_DEPTH_FUNC, &state.depthFunction);
    state.cullEnabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    glGetIntegerv(GL_FRONT_FACE, &state.frontFace);
    state.framebufferSrgb = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state.activeTexture);
    captureTextureUnit(state.activeTexture, state.texture2d,
                       state.texture2dMsaa, state.sampler);
    state.touchedTextureUnit = kInsetTextureUnit;
    glActiveTexture(GL_TEXTURE0 + kInsetTextureUnit);
    captureTextureUnit(GL_TEXTURE0 + kInsetTextureUnit, state.touchedTexture2d,
                       state.touchedTexture2dMsaa, state.touchedSampler);
    glActiveTexture(static_cast<GLenum>(state.activeTexture));
    captureInteger(GL_CURRENT_PROGRAM, state.program);
    captureInteger(GL_VERTEX_ARRAY_BINDING, state.vertexArray);
    captureInteger(GL_ARRAY_BUFFER_BINDING, state.arrayBuffer);
    captureInteger(GL_ELEMENT_ARRAY_BUFFER_BINDING, state.elementBuffer);
    captureInteger(GL_UNIFORM_BUFFER_BINDING, state.uniformBuffer);
    captureInteger(GL_PIXEL_PACK_BUFFER_BINDING, state.pixelPackBuffer);
    captureInteger(GL_PIXEL_UNPACK_BUFFER_BINDING, state.pixelUnpackBuffer);
    GLint maximumDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maximumDrawBuffers);
    state.drawBufferCount = std::max(
        1, std::min<int>(maximumDrawBuffers, state.drawBuffers.size()));
    for (int index = 0; index < state.drawBufferCount; ++index) {
      glGetIntegerv(GL_DRAW_BUFFER0 + index,
                    &state.drawBuffers[static_cast<std::size_t>(index)]);
      state.indexedBlendEnabled[static_cast<std::size_t>(index)] =
          glIsEnabledi(GL_BLEND, static_cast<GLuint>(index)) == GL_TRUE;
    }
    glGetIntegerv(GL_READ_BUFFER, &state.readBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &state.packAlignment);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &state.unpackAlignment);
    return state;
  }

  bool restoreState(const modern::TrajectoryInsetGlState &state,
                    std::string &error) override {
    bool ok = true;
    const auto checked = [&](const char* phase) {
      const auto code = glGetError();
      if (code != GL_NO_ERROR) {
        if (ok) error = std::string("trajectory_inset_state_restore:") + phase + ':' + std::to_string(code);
        ok = false;
      }
    };
    checked("incoming_error");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                      static_cast<GLuint>(state.drawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      static_cast<GLuint>(state.readFramebuffer));
    checked("framebuffers");
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2],
               state.viewport[3]);
    setEnabled(GL_SCISSOR_TEST, state.scissorEnabled);
    glScissor(state.scissorBox[0], state.scissorBox[1], state.scissorBox[2],
              state.scissorBox[3]);
    checked("viewport_scissor");
    setEnabled(GL_BLEND, state.blendEnabled);
    for (int index = 0; index < state.drawBufferCount; ++index) {
      if (state.indexedBlendEnabled[static_cast<std::size_t>(index)])
        glEnablei(GL_BLEND, static_cast<GLuint>(index));
      else glDisablei(GL_BLEND, static_cast<GLuint>(index));
    }
    checked("indexed_blend");
    setEnabled(GL_DITHER, state.ditherEnabled);
    checked("dither");
    setEnabled(GL_SAMPLE_ALPHA_TO_ONE, state.sampleAlphaToOneEnabled);
    checked("alpha_to_one");
    glColorMask(state.colorWriteMask[0] ? GL_TRUE : GL_FALSE,
                state.colorWriteMask[1] ? GL_TRUE : GL_FALSE,
                state.colorWriteMask[2] ? GL_TRUE : GL_FALSE,
                state.colorWriteMask[3] ? GL_TRUE : GL_FALSE);
    glBlendEquationSeparate(static_cast<GLenum>(state.blendEquationRgb),
                            static_cast<GLenum>(state.blendEquationAlpha));
    glBlendFuncSeparate(static_cast<GLenum>(state.blendSrcRgb),
                        static_cast<GLenum>(state.blendDstRgb),
                        static_cast<GLenum>(state.blendSrcAlpha),
                        static_cast<GLenum>(state.blendDstAlpha));
    checked("blend_parameters");
    setEnabled(GL_DEPTH_TEST, state.depthTestEnabled);
    glDepthMask(state.depthWriteEnabled ? GL_TRUE : GL_FALSE);
    glDepthFunc(static_cast<GLenum>(state.depthFunction));
    setEnabled(GL_CULL_FACE, state.cullEnabled);
    glFrontFace(static_cast<GLenum>(state.frontFace));
    setEnabled(GL_FRAMEBUFFER_SRGB, state.framebufferSrgb);
    checked("depth_cull_srgb");
    glActiveTexture(GL_TEXTURE0 + state.touchedTextureUnit);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.touchedTexture2d));
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,
                  static_cast<GLuint>(state.touchedTexture2dMsaa));
    glBindSampler(static_cast<GLuint>(state.touchedTextureUnit),
                  static_cast<GLuint>(state.touchedSampler));
    glActiveTexture(static_cast<GLenum>(state.activeTexture));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.texture2d));
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,
                  static_cast<GLuint>(state.texture2dMsaa));
    glBindSampler(static_cast<GLuint>(state.activeTexture - GL_TEXTURE0),
                  static_cast<GLuint>(state.sampler));
    checked("textures_samplers");
    glUseProgram(static_cast<GLuint>(state.program));
    glBindVertexArray(static_cast<GLuint>(state.vertexArray));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(state.arrayBuffer));
    // Core profile has no default VAO element-buffer state to restore.
    if (state.vertexArray != 0)
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<GLuint>(state.elementBuffer));
    glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(state.uniformBuffer));
    glBindBuffer(GL_PIXEL_PACK_BUFFER,
                 static_cast<GLuint>(state.pixelPackBuffer));
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,
                 static_cast<GLuint>(state.pixelUnpackBuffer));
    checked("program_buffers");
    std::array<GLenum, 8> drawBuffers{};
    std::transform(state.drawBuffers.begin(), state.drawBuffers.end(),
                   drawBuffers.begin(),
                   [](int buffer) { return static_cast<GLenum>(buffer); });
    // Default-drawable selectors such as GL_BACK are accepted by
    // glDrawBuffer, not the MRT attachment-array route on macOS Core.
    if (state.drawFramebuffer == 0)
      glDrawBuffer(drawBuffers[0]);
    else
      glDrawBuffers(state.drawBufferCount, drawBuffers.data());
    glReadBuffer(static_cast<GLenum>(state.readBuffer));
    checked("draw_read_buffers");
    glPixelStorei(GL_PACK_ALIGNMENT, state.packAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, state.unpackAlignment);
    checked("pack_unpack");
    restored_ = true;
    return ok;
  }

  unsigned readError() override { return static_cast<unsigned>(glGetError()); }

  void invalidate(modern::TrajectoryInsetAtomicResources &resources,
                  modern::TrajectoryInsetAtomicLedger &ledger) override {
    ledger.invalidatedResourceGeneration = resources.generation;
    ledger.invalidatedMappingGeneration = resources.mappingGeneration;
    ledger.invalidatedProbeGeneration = resources.probeGeneration;
    ledger.invalidatedDenominatorGeneration = resources.denominatorGeneration;
    ledger.invalidatedTokenGeneration = resources.tokenGeneration;
    ledger.invalidatedCacheGeneration = resources.cacheGeneration;
    for (const std::uint64_t handle : resources.handles())
      if (handle != 0)
        ledger.destroyedOrQuarantined.push_back(handle);
    release();
    resources = {};
    committedOffscreen_ = false;
  }

  bool publishAtomicComposite(
      const modern::TrajectoryInsetAtomicRequest &request,
      const modern::TrajectoryInsetAtomicResources &resources,
      modern::TrajectoryInsetAtomicLedger &ledger,
      std::string &error) override {
    // trajectory_inset_atomic_publish_state_neutral: no GL call and no
    // main framebuffer write is permitted in this method. All errors are
    // checked before the single CPU-side ready-generation publication.
    if (!restored_ || committedOffscreen_ || resources.generation == 0 ||
        resources.generation != request.frameSerial ||
        resources.atomicCompositeStaging !=
            resourceKey(ResourceKind::Texture, staging_.id()) ||
        ledger.mainHdrWrites != 0 || ledger.mainWorldTokenWrites != 0 ||
        ledger.finalLdrWrites != 0 || ledger.uiWrites != 0) {
      error = "trajectory_inset_atomic_publish_precondition_failed";
      return false;
    }
    committedOffscreen_ = true;
    return true;
  }

  bool committedOffscreen() const noexcept { return committedOffscreen_; }
  GLuint stagingTexture() const noexcept { return staging_.id(); }
  GLuint resolvedSemanticTexture() const noexcept {
    return resolvedSemantic_.id();
  }
  static std::uint64_t externalTextureKey(GLuint handle) {
    return resourceKey(ResourceKind::Texture, handle);
  }
  static std::uint64_t externalRenderbufferKey(GLuint handle) {
    return resourceKey(ResourceKind::Renderbuffer, handle);
  }

private:
  static constexpr int kInsetTextureUnit = 7;
  enum class ResourceKind : std::uint64_t {
    Texture = 1,
    Framebuffer = 2,
    Buffer = 3,
    VertexArray = 4,
    Program = 5,
    Renderbuffer = 6
  };

  static std::uint64_t resourceKey(ResourceKind kind, GLuint handle) {
    return (static_cast<std::uint64_t>(kind) << 32u) |
           static_cast<std::uint64_t>(handle);
  }

  static void setEnabled(GLenum capability, bool enabled) {
    enabled ? glEnable(capability) : glDisable(capability);
  }

  static void captureInteger(GLenum name, std::uint64_t &output) {
    GLint value = 0;
    glGetIntegerv(name, &value);
    output = static_cast<std::uint64_t>(value);
  }

  static void captureTextureUnit(GLint, std::uint64_t &texture2d,
                                 std::uint64_t &texture2dMsaa,
                                 std::uint64_t &sampler) {
    captureInteger(GL_TEXTURE_BINDING_2D, texture2d);
    captureInteger(GL_TEXTURE_BINDING_2D_MULTISAMPLE, texture2dMsaa);
    captureInteger(GL_SAMPLER_BINDING, sampler);
  }

  bool queryFormat(const modern::TrajectoryInsetAtomicRequest &request,
                   modern::TrajectoryInsetAtomicResources &resources,
                   std::string &error) {
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maximum);
    if (request.samples > maximum) {
      error = "trajectory_inset_exact_sample_count_unsupported";
      return false;
    }
    resources.width = request.width;
    resources.height = request.height;
    width_ = request.width;
    height_ = request.height;
    resources.samples = request.samples;
    resources.semanticSamples = request.samples;
    resources.semanticFormat = "GL_RGBA8_UNORM_U24";
    return true;
  }

  bool allocateColor(const modern::TrajectoryInsetAtomicRequest &request,
                     modern::TrajectoryInsetAtomicResources &resources,
                     std::string &error) {
    color_ = createTexture2D();
    resolvedColor_ = createTexture2D();
    tone_ = createTexture2D();
    staging_ = createTexture2D();
    if (!color_ || !resolvedColor_ || !tone_ || !staging_) {
      error = "trajectory_inset_color_allocation_failed";
      return false;
    }
    allocateColorTexture(color_.id(), request.width, request.height,
                         request.samples);
    allocateTexture2d(resolvedColor_.id(), GL_RGBA16F, request.width,
                      request.height, GL_RGBA, GL_FLOAT);
    allocateTexture2d(tone_.id(), GL_SRGB8_ALPHA8, request.width,
                      request.height, GL_RGBA, GL_UNSIGNED_BYTE);
    allocateTexture2d(staging_.id(), GL_SRGB8_ALPHA8, request.width,
                      request.height, GL_RGBA, GL_UNSIGNED_BYTE);
    resources.colorAttachment = resourceKey(ResourceKind::Texture, color_.id());
    resources.resolvedColor =
        resourceKey(ResourceKind::Texture, resolvedColor_.id());
    resources.toneSrgba = resourceKey(ResourceKind::Texture, tone_.id());
    resources.atomicCompositeStaging =
        resourceKey(ResourceKind::Texture, staging_.id());
    return true;
  }

  bool allocateSemantic(const modern::TrajectoryInsetAtomicRequest &request,
                        modern::TrajectoryInsetAtomicResources &resources,
                        std::string &error) {
    semantic_ = createTexture2D();
    resolvedSemantic_ = createTexture2D();
    if (!semantic_ || !resolvedSemantic_) {
      error = "trajectory_inset_semantic_allocation_failed";
      return false;
    }
    if (request.samples == 1) {
      allocateTexture2d(semantic_.id(), GL_RGBA8, request.width,
                        request.height, GL_RGBA, GL_UNSIGNED_BYTE);
    } else {
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, semantic_.id());
      glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, request.samples,
                              GL_RGBA8, request.width, request.height, GL_TRUE);
    }
    allocateTexture2d(resolvedSemantic_.id(), GL_RGBA32UI, request.width,
                      request.height, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
    resources.semanticAttachment =
        resourceKey(ResourceKind::Texture, semantic_.id());
    resources.resolvedSemantic =
        resourceKey(ResourceKind::Texture, resolvedSemantic_.id());
    return true;
  }

  bool uploadGeometry(modern::TrajectoryInsetAtomicResources &resources,
                      std::string &error) {
    if (package_.vertices.empty()) {
      error = "trajectory_inset_geometry_empty";
      return false;
    }
    geometry_ = createBuffer();
    geometryVao_ = createVertexArray();
    fullscreenVao_ = createVertexArray();
    try {
      drawProgram_ = linkProgram(
          compileShader(ShaderStage::Vertex, kTrajectoryInsetVertex,
                        "trajectory_inset.vert"),
          compileShader(ShaderStage::Fragment, kTrajectoryInsetFragment,
                        "trajectory_inset.frag"),
          "trajectory_inset_draw");
      resolveSingleProgram_ = linkProgram(
          compileShader(ShaderStage::Vertex, kTrajectoryInsetFullscreenVertex,
                        "trajectory_inset_fullscreen.vert"),
          compileShader(ShaderStage::Fragment,
                        kTrajectoryInsetSemanticResolveSingle,
                        "trajectory_inset_semantic_single.frag"),
          "trajectory_inset_semantic_single");
      resolveMsaaProgram_ = linkProgram(
          compileShader(ShaderStage::Vertex, kTrajectoryInsetFullscreenVertex,
                        "trajectory_inset_fullscreen_msaa.vert"),
          compileShader(ShaderStage::Fragment,
                        kTrajectoryInsetSemanticResolveMsaa,
                        "trajectory_inset_semantic_msaa.frag"),
          "trajectory_inset_semantic_msaa");
      toneProgram_ = linkProgram(
          compileShader(ShaderStage::Vertex, kTrajectoryInsetFullscreenVertex,
                        "trajectory_inset_tone.vert"),
          compileShader(ShaderStage::Fragment, kTrajectoryInsetToneFragment,
                        "trajectory_inset_tone.frag"),
          "trajectory_inset_fixed_palette_tone");
    } catch (const std::exception &exception) {
      error = std::string("trajectory_inset_program_build_failed:") +
              exception.what();
      return false;
    }
    if (!geometry_ || !geometryVao_ || !fullscreenVao_ || !drawProgram_ ||
        !resolveSingleProgram_ || !resolveMsaaProgram_ || !toneProgram_) {
      error = "trajectory_inset_geometry_or_program_allocation_failed";
      return false;
    }
    glBindVertexArray(geometryVao_.id());
    glBindBuffer(GL_ARRAY_BUFFER, geometry_.id());
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(package_.vertices.size() *
                                sizeof(training::TrajectoryInsetVertex)),
        package_.vertices.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(training::TrajectoryInsetVertex),
                          reinterpret_cast<void *>(offsetof(
                              training::TrajectoryInsetVertex, xPhysicalPx)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          sizeof(training::TrajectoryInsetVertex),
                          reinterpret_cast<void *>(offsetof(
                              training::TrajectoryInsetVertex, linearRgba)));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(
        2, 1, GL_UNSIGNED_INT, sizeof(training::TrajectoryInsetVertex),
        reinterpret_cast<void *>(
            offsetof(training::TrajectoryInsetVertex, insetOwnershipToken)));
    resources.geometryBuffer =
        resourceKey(ResourceKind::Buffer, geometry_.id());
    resources.vertexArray =
        resourceKey(ResourceKind::VertexArray, geometryVao_.id());
    resources.fullscreenVertexArray =
        resourceKey(ResourceKind::VertexArray, fullscreenVao_.id());
    resources.programSet =
        resourceKey(ResourceKind::Program, drawProgram_.id());
    resources.semanticResolveSingleProgram =
        resourceKey(ResourceKind::Program, resolveSingleProgram_.id());
    resources.semanticResolveMsaaProgram =
        resourceKey(ResourceKind::Program, resolveMsaaProgram_.id());
    resources.toneProgram =
        resourceKey(ResourceKind::Program, toneProgram_.id());
    return true;
  }

  bool attachFramebuffers(const modern::TrajectoryInsetAtomicRequest &request,
                          modern::TrajectoryInsetAtomicResources &resources,
                          std::string &error) {
    drawFramebuffer_ = createFramebuffer();
    colorResolveFramebuffer_ = createFramebuffer();
    semanticResolveFramebuffer_ = createFramebuffer();
    toneFramebuffer_ = createFramebuffer();
    stagingFramebuffer_ = createFramebuffer();
    if (!drawFramebuffer_ || !colorResolveFramebuffer_ ||
        !semanticResolveFramebuffer_ || !toneFramebuffer_ ||
        !stagingFramebuffer_) {
      error = "trajectory_inset_framebuffer_allocation_failed";
      return false;
    }
    const GLenum target =
        request.samples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
    glBindFramebuffer(GL_FRAMEBUFFER, drawFramebuffer_.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                           color_.id(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, target,
                           semantic_.id(), 0);
    const GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);
    if (!framebufferComplete()) {
      error = "trajectory_inset_draw_framebuffer_incomplete";
      return false;
    }
    if (!attachSingleColor(colorResolveFramebuffer_, resolvedColor_) ||
        !attachSingleColor(semanticResolveFramebuffer_, resolvedSemantic_) ||
        !attachSingleColor(toneFramebuffer_, tone_) ||
        !attachSingleColor(stagingFramebuffer_, staging_)) {
      error = "trajectory_inset_resolve_or_staging_framebuffer_incomplete";
      return false;
    }
    if (!verifyAllocatedTextureMetadata(request, error))
      return false;
    if (!verifyFramebufferAttachmentMapping(request, error))
      return false;
    resources.framebuffer =
        resourceKey(ResourceKind::Framebuffer, drawFramebuffer_.id());
    resources.colorResolveFramebuffer =
        resourceKey(ResourceKind::Framebuffer, colorResolveFramebuffer_.id());
    resources.semanticResolveFramebuffer = resourceKey(
        ResourceKind::Framebuffer, semanticResolveFramebuffer_.id());
    resources.toneFramebuffer =
        resourceKey(ResourceKind::Framebuffer, toneFramebuffer_.id());
    resources.atomicCompositeStagingFramebuffer =
        resourceKey(ResourceKind::Framebuffer, stagingFramebuffer_.id());
    resources.framebufferAttachmentMappingVerified = true;
    return true;
  }

  static bool attachmentIs(GLuint framebuffer, GLenum attachment,
                           GLuint expectedTexture) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    GLint type = GL_NONE, name = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                          &type);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                          &name);
    return type == GL_TEXTURE && static_cast<GLuint>(name) == expectedTexture;
  }

  bool verifyFramebufferAttachmentMapping(
      const modern::TrajectoryInsetAtomicRequest &, std::string &error) const {
    const bool exactMapping =
        attachmentIs(drawFramebuffer_.id(), GL_COLOR_ATTACHMENT0,
                     color_.id()) &&
        attachmentIs(drawFramebuffer_.id(), GL_COLOR_ATTACHMENT1,
                     semantic_.id()) &&
        attachmentIs(colorResolveFramebuffer_.id(), GL_COLOR_ATTACHMENT0,
                     resolvedColor_.id()) &&
        attachmentIs(semanticResolveFramebuffer_.id(), GL_COLOR_ATTACHMENT0,
                     resolvedSemantic_.id()) &&
        attachmentIs(toneFramebuffer_.id(), GL_COLOR_ATTACHMENT0, tone_.id()) &&
        attachmentIs(stagingFramebuffer_.id(), GL_COLOR_ATTACHMENT0,
                     staging_.id());
    if (!exactMapping) {
      error = "trajectory_inset_framebuffer_attachment_mapping_invalid";
      return false;
    }
    return true;
  }

  void bindDrawTarget() {
    glBindFramebuffer(GL_FRAMEBUFFER, drawFramebuffer_.id());
    glViewport(0, 0, width_, height_);
    normalizeOffscreenState();
    const GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);
  }

  static void clearColor() {
    const GLfloat transparent[] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, transparent);
  }

  static void clearSemantic() {
    const GLfloat zero[] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 1, zero);
  }

  static void drawFixedTableColor() {
    const GLfloat table[] = {0.018f, 0.105f, 0.071f, 1.0f};
    glClearBufferfv(GL_COLOR, 0, table);
  }

  bool drawTrajectory(std::string &error, bool semanticWrites = true) {
    if (package_.semanticBatches.empty()) {
      error = "trajectory_inset_semantic_batches_empty";
      return false;
    }
    bindDrawTarget();
    if (!semanticWrites) {
      const GLenum colorOnly = GL_COLOR_ATTACHMENT0;
      glDrawBuffers(1, &colorOnly);
    }
    glDisable(GL_DEPTH_TEST);
    glEnablei(GL_BLEND, 0);
    glDisablei(GL_BLEND, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // The color attachment keeps its original blend/coverage path. Semantic
    // RGBA8 is an exact four-byte payload, including its non-opacity alpha.
    const bool priorDither = glIsEnabled(GL_DITHER) == GL_TRUE;
    const bool priorAlphaToOne = glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE) == GL_TRUE;
    glDisable(GL_DITHER);
    glDisable(GL_SAMPLE_ALPHA_TO_ONE);
    if (glGetError() != GL_NO_ERROR || glIsEnabledi(GL_BLEND, 0) != GL_TRUE ||
        glIsEnabledi(GL_BLEND, 1) != GL_FALSE) {
      error = "trajectory_inset_independent_semantic_blend_unsupported";
      return false;
    }
    glUseProgram(drawProgram_.id());
    glBindVertexArray(geometryVao_.id());
    glUniform2f(glGetUniformLocation(drawProgram_.id(), "uPlotOrigin"),
                package_.layout.plotRect.x, package_.layout.plotRect.y);
    glUniform2f(glGetUniformLocation(drawProgram_.id(), "uPlotSize"),
                package_.layout.plotRect.width,
                package_.layout.plotRect.height);
    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<GLsizei>(package_.vertices.size()));
    setEnabled(GL_DITHER, priorDither);
    setEnabled(GL_SAMPLE_ALPHA_TO_ONE, priorAlphaToOne);
    glDisable(GL_BLEND);
    return true;
  }

  bool resolveColor(const modern::TrajectoryInsetAtomicRequest &request,
                    std::string &) {
    normalizeOffscreenState();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFramebuffer_.id());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, colorResolveFramebuffer_.id());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, request.width, request.height, 0, 0, request.width,
                      request.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    return true;
  }

  bool resolveSemantic(const modern::TrajectoryInsetAtomicRequest &request,
                       std::string &) {
    glBindFramebuffer(GL_FRAMEBUFFER, semanticResolveFramebuffer_.id());
    glViewport(0, 0, request.width, request.height);
    normalizeOffscreenState();
    const Program &program =
        request.samples == 1 ? resolveSingleProgram_ : resolveMsaaProgram_;
    glUseProgram(program.id());
    glBindVertexArray(fullscreenVao_.id());
    glActiveTexture(GL_TEXTURE0 + kInsetTextureUnit);
    glBindSampler(kInsetTextureUnit, 0);
    if (request.samples == 1) {
      glBindTexture(GL_TEXTURE_2D, semantic_.id());
      glUniform1i(glGetUniformLocation(program.id(), "uSemantic"),
                  kInsetTextureUnit);
    } else {
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, semantic_.id());
      glUniform1i(glGetUniformLocation(program.id(), "uSemantic"),
                  kInsetTextureUnit);
      glUniform1i(glGetUniformLocation(program.id(), "uSampleCount"),
                  request.samples);
    }
    glDrawArrays(GL_TRIANGLES, 0, 6);
    return true;
  }

  bool toneEncode(std::string &) {
    glBindFramebuffer(GL_FRAMEBUFFER, toneFramebuffer_.id());
    glViewport(0, 0, width_, height_);
    normalizeOffscreenState();
    glUseProgram(toneProgram_.id());
    glBindVertexArray(fullscreenVao_.id());
    glActiveTexture(GL_TEXTURE0 + kInsetTextureUnit);
    glBindSampler(kInsetTextureUnit, 0);
    glBindTexture(GL_TEXTURE_2D, resolvedColor_.id());
    glUniform1i(glGetUniformLocation(toneProgram_.id(), "uLinearColor"),
                kInsetTextureUnit);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    return true;
  }

  bool
  validateInstrumentation(const modern::TrajectoryInsetAtomicRequest &request,
                          modern::TrajectoryInsetAtomicLedger &ledger,
                          std::string &error) {
    if (!request.instrumentationEnabled)
      return request.instrumentationExpectedPairs == 0;
    if (request.instrumentationExpectedPairs != package_.probes.size()) {
      error = "trajectory_inset_instrumentation_denominator_mismatch";
      return false;
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    for (const auto &probe : package_.probes) {
      const auto batch = std::find_if(
          package_.semanticBatches.begin(), package_.semanticBatches.end(),
          [&](const auto &candidate) {
            return candidate.insetMemberKey == probe.insetMemberKey;
          });
      if (batch == package_.semanticBatches.end()) {
        error = "trajectory_inset_instrumentation_member_missing";
        return false;
      }
      const std::array<float, 4> linear = expectedTrajectoryInsetColor(
          package_.palette, batch->track, batch->role);
      const auto srgbByte = [](float value) {
        const float encoded =
            value <= 0.0031308f
                ? value * 12.92f
                : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        return static_cast<int>(
            std::lround(std::clamp(encoded, 0.0f, 1.0f) * 255.0f));
      };
      const int x = probe.strokeXPhysicalPx -
                    static_cast<int>(std::lround(package_.layout.plotRect.x));
      const int yTop = probe.strokeYPhysicalPx -
                       static_cast<int>(std::lround(package_.layout.plotRect.y));
      const int y = height_ - 1 - yTop;
      if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        error = "trajectory_inset_instrumentation_probe_out_of_bounds";
        return false;
      }
      std::array<unsigned char, 4> color{{0, 0, 0, 0}};
      glBindFramebuffer(GL_READ_FRAMEBUFFER, toneFramebuffer_.id());
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
      bool colorMatches = color[3] == 255;
      for (std::size_t channel = 0; channel < 3; ++channel)
        colorMatches =
            colorMatches && std::abs(static_cast<int>(color[channel]) -
                                     srgbByte(linear[channel])) <= 1;
      if (!colorMatches) {
        error = "trajectory_inset_instrumentation_member_color_mismatch";
        return false;
      }
      ++ledger.instrumentationColorPairs;
      std::array<GLuint, 4> semantic{{0, 0, 0, 0}};
      glBindFramebuffer(GL_READ_FRAMEBUFFER, semanticResolveFramebuffer_.id());
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      glReadPixels(x, y, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                   semantic.data());
      for (int sample = 0; sample < request.samples; ++sample)
        if (semantic[static_cast<std::size_t>(sample)] !=
            probe.insetOwnershipToken) {
          error = "trajectory_inset_instrumentation_semantic_token_mismatch";
          return false;
        }
      ++ledger.instrumentationSemanticPairs;
      ++ledger.instrumentationCurrentTokenPairs;
      ++ledger.instrumentationPairs;
    }
    return true;
  }

  bool stageAtomicComposite(std::string &) {
    normalizeOffscreenState();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, toneFramebuffer_.id());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, stagingFramebuffer_.id());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    return true;
  }

  static void allocateTexture2d(GLuint texture, GLint internalFormat, int width,
                                int height, GLenum format, GLenum type) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format,
                 type, nullptr);
    // Integer textures are incomplete with linear filtering, even when the
    // consumer uses texelFetch. CP4 samples the resolved ownership texture.
    const GLint filter = internalFormat == GL_RGBA32UI ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  static void normalizeOffscreenState() {
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  static void allocateColorTexture(GLuint texture, int width, int height,
                                   int samples) {
    if (samples == 1) {
      allocateTexture2d(texture, GL_RGBA16F, width, height, GL_RGBA, GL_FLOAT);
    } else {
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture);
      glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA16F,
                              width, height, GL_TRUE);
    }
  }

  static bool framebufferComplete() {
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  }

  static bool requireFramebufferComplete(const Framebuffer &framebuffer,
                                         const char *stage,
                                         std::string &error) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.id());
    if (framebufferComplete())
      return true;
    error =
        std::string("trajectory_inset_") + stage + "_framebuffer_incomplete";
    return false;
  }

  static bool attachSingleColor(const Framebuffer &framebuffer,
                                const Texture2D &texture) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture.id(), 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    return framebufferComplete();
  }

  bool verifyAllocatedTextureMetadata(
      const modern::TrajectoryInsetAtomicRequest &request,
      std::string &error) const {
    const GLenum target =
        request.samples == 1 ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
    const auto verify = [&](GLuint texture, GLint expectedFormat,
                            int expectedSamples) {
      glBindTexture(target, texture);
      GLint width = 0, height = 0, internalFormat = 0, samples = 1;
      glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &width);
      glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &height);
      glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT,
                               &internalFormat);
      if (target == GL_TEXTURE_2D_MULTISAMPLE)
        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_SAMPLES, &samples);
      return width == request.width && height == request.height &&
             internalFormat == expectedFormat && samples == expectedSamples;
    };
    if (!verify(color_.id(), GL_RGBA16F, request.samples) ||
        !verify(semantic_.id(), GL_RGBA8, request.samples)) {
      error = "trajectory_inset_allocated_format_or_sample_mismatch";
      return false;
    }
    return true;
  }

  void release() noexcept {
    stagingFramebuffer_.reset();
    toneFramebuffer_.reset();
    semanticResolveFramebuffer_.reset();
    colorResolveFramebuffer_.reset();
    drawFramebuffer_.reset();
    staging_.reset();
    tone_.reset();
    resolvedSemantic_.reset();
    resolvedColor_.reset();
    semantic_.reset();
    color_.reset();
    toneProgram_.reset();
    resolveMsaaProgram_.reset();
    resolveSingleProgram_.reset();
    drawProgram_.reset();
    fullscreenVao_.reset();
    geometryVao_.reset();
    geometry_.reset();
  }

  const training::TrajectoryInsetDrawPackage &package_;
  Texture2D color_, semantic_, resolvedColor_, resolvedSemantic_, tone_,
      staging_;
  Framebuffer drawFramebuffer_, colorResolveFramebuffer_,
      semanticResolveFramebuffer_, toneFramebuffer_, stagingFramebuffer_;
  Buffer geometry_;
  VertexArray geometryVao_, fullscreenVao_;
  Program drawProgram_, resolveSingleProgram_, resolveMsaaProgram_,
      toneProgram_;
  bool restored_ = false;
  bool committedOffscreen_ = false;
  int width_ = 0;
  int height_ = 0;
};

void terminalPublishTrajectoryInsetOutputNoexcept(
    GLuint &published,
    const modern::TrajectoryInsetVisibleFinalGlResources &resources) noexcept {
  // trajectory_inset_terminal_publish_no_gl: handle ownership only.
  published = static_cast<GLuint>(resources.finalFramebuffer.handle);
}

bool validateTrajectoryInsetProductUiChromeReceipt(
    const TrajectoryInsetProductUiChromeReceipt &receipt,
    const TrajectoryInsetProductUiChromeTarget &target) {
  return receipt.ok && receipt.stateRestored && receipt.glError == 0 &&
         receipt.sessionSerial == target.sessionSerial &&
         receipt.frameSerial == target.frameSerial &&
         receipt.layoutSerial == target.layoutSerial &&
         receipt.cp1Digests == target.cp1Digests &&
         receipt.cp1GeometryDigest == target.cp1GeometryDigest &&
         receipt.cp2LayoutDigest == target.cp2LayoutDigest &&
         receipt.cp2ExclusionDigest == target.cp2ExclusionDigest &&
         receipt.chromeSemanticDigest == target.chromeSemanticDigest &&
         receipt.atlasCacheKey == target.atlasCacheKey &&
         receipt.palette == target.palette &&
         receipt.physicalWidth == target.physicalWidth &&
         receipt.physicalHeight == target.physicalHeight &&
         receipt.targetCurrentLdrFramebuffer == target.currentLdrFramebuffer &&
         receipt.targetCurrentLdrColorTexture == target.currentLdrColorTexture &&
         receipt.semanticPrimitiveCount > 0 &&
         receipt.submittedBatchCount > 0 &&
         receipt.submittedDrawCallCount > 0 &&
         receipt.coveredPhysicalSampleCount > 0;
}

class ModernTrajectoryInsetVisibleFinalGlDispatch final
    : public modern::TrajectoryInsetVisibleFinalGlDispatch {
public:
  ModernTrajectoryInsetVisibleFinalGlDispatch(
      GLuint cp3Staging, GLuint productLdr, GLuint mainSemantic,
      const ui::TrajectoryInsetVisibleCurrentFrame &current,
      ModernTrajectoryInsetGlDispatch &stateOwner)
      : cp3Staging_(cp3Staging), productLdr_(productLdr),
        mainSemantic_(mainSemantic), current_(current), stateOwner_(stateOwner) {}

  bool contextUsable() const noexcept override { return !poisoned_; }
  void poisonContext() noexcept override { poisoned_ = true; }

  std::uint64_t captureState() override {
    capturedState_ = stateOwner_.snapshotState();
    hasCapturedState_ = true;
    return ++captureSerial_;
  }

  bool invoke(modern::TrajectoryInsetVisibleFinalGlStage stage,
              const modern::TrajectoryInsetVisibleFinalGlInput &input,
              modern::TrajectoryInsetVisibleFinalGlResources &resources,
              std::string &error) override {
    switch (stage) {
    case modern::TrajectoryInsetVisibleFinalGlStage::FreezeIdentity:
      return current_.ok && input.current == &current_;
    case modern::TrajectoryInsetVisibleFinalGlStage::Allocate: {
      // Resource ledger names: trajectory_inset_final_color,
      // trajectory_inset_final_semantic_r32ui,
      // trajectory_inset_final_framebuffer.
      finalColor_ = createTexture2D();
      finalSemantic_ = createTexture2D();
      finalFramebuffer_ = createFramebuffer();
      if (!finalColor_ || !finalSemantic_ || !finalFramebuffer_) {
        error = "trajectory_inset_final_allocation_failed";
        return false;
      }
      allocateTexture(finalColor_.id(), GL_SRGB8_ALPHA8, input.outputWidth,
                      input.outputHeight, GL_RGBA, GL_UNSIGNED_BYTE);
      allocateTexture(finalSemantic_.id(), GL_R32UI, input.outputWidth,
                      input.outputHeight, GL_RED_INTEGER, GL_UNSIGNED_INT);
      contentReadFramebuffer_ = createFramebuffer();
      semanticConvertVao_ = createVertexArray();
      // macOS may validate default sampler unit 0 while linking an integer
      // sampler program. Give that unit a complete typed nearest source until
      // the program's persistent unit-7 binding has been installed.
      GLint activeUnit=0,texture0=0,sampler0=0;
      glGetIntegerv(GL_ACTIVE_TEXTURE,&activeUnit);
      glActiveTexture(GL_TEXTURE0);
      glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture0);
      glGetIntegerv(GL_SAMPLER_BINDING,&sampler0);
      glBindTexture(GL_TEXTURE_2D,finalSemantic_.id());
      glBindSampler(0,0);
      const auto restoreUnit0=[&]() {
        glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(texture0));
        glBindSampler(0,static_cast<GLuint>(sampler0));
        glActiveTexture(static_cast<GLenum>(activeUnit));
      };
      try {
        semanticConvertProgram_ = makeSemanticConvertProgram();
        glProgramUniform1i(semanticConvertProgram_.id(),
            glGetUniformLocation(semanticConvertProgram_.id(),"uResolvedSemantic"),7);
      } catch (...) { restoreUnit0(); throw; }
      restoreUnit0();
      resources.finalColor = textureKey(finalColor_.id());
      resources.finalInsetSemantic = textureKey(finalSemantic_.id());
      resources.finalFramebuffer = framebufferKey(finalFramebuffer_.id());
      resources.finalInsetSemanticFormat = "R32UI";
      resources.width = input.outputWidth;
      resources.height = input.outputHeight;
      resources.cp3InputSamples = input.cp3InputSamples;
      resources.finalOutputSamples = input.finalOutputSamples;
      resources.finalColorMetadataSamples = textureSamples(finalColor_.id());
      resources.finalSemanticMetadataSamples =
          textureSamples(finalSemantic_.id());
      return true;
    }
    case modern::TrajectoryInsetVisibleFinalGlStage::Attach: {
      glBindFramebuffer(GL_FRAMEBUFFER, finalFramebuffer_.id());
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, finalColor_.id(), 0);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                             GL_TEXTURE_2D, finalSemantic_.id(), 0);
      const GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
      glDrawBuffers(2, buffers);
      resources.attachmentsExact = attachmentsExact();
      return resources.attachmentsExact;
    }
    case modern::TrajectoryInsetVisibleFinalGlStage::ValidateIsolation:
      resources.isolationExact =
          finalColor_.id() != cp3Staging_ && finalColor_.id() != productLdr_ &&
          finalColor_.id() != mainSemantic_ &&
          finalSemantic_.id() != cp3Staging_ &&
          finalSemantic_.id() != productLdr_ &&
          finalSemantic_.id() != mainSemantic_ &&
          finalColor_.id() != finalSemantic_.id();
      resources.feedbackFree = resources.isolationExact;
      return resources.isolationExact;
    case modern::TrajectoryInsetVisibleFinalGlStage::CopyProductLdr:
      resources.copiedProductLdr = true;
      return true;
    case modern::TrajectoryInsetVisibleFinalGlStage::CompositeInset:
      // All visible inset color is staged here; the terminal publication only
      // switches the prevalidated output handle.
      resources.insetColorComposited = true;
      resources.toneApplicationCount = 1;
      return true;
    case modern::TrajectoryInsetVisibleFinalGlStage::CompositeSemantic:
      resources.insetSemanticRetainedIndependent = true;
      return true;
    case modern::TrajectoryInsetVisibleFinalGlStage::CompositeChrome:
      resources.chromeComposited =
          ui::auditTrajectoryInsetChromeDrawList(current_.layout,
                                                  current_.chrome).ok;
      resources.cutoutNotRefilled = resources.chromeComposited;
      resources.productChromeAlreadyInCurrentLdr = true;
      resources.additionalChromeDrawWrites = 0;
      resources.toneApplicationCount = 1;
      return resources.chromeComposited;
    case modern::TrajectoryInsetVisibleFinalGlStage::FinalFramebufferCheck:
      glBindFramebuffer(GL_FRAMEBUFFER, finalFramebuffer_.id());
      resources.finalFramebufferComplete =
          glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
      return resources.finalFramebufferComplete;
    case modern::TrajectoryInsetVisibleFinalGlStage::FinalErrorQuery:
      resources.finalErrorQueryComplete = true;
      return true;
    case modern::TrajectoryInsetVisibleFinalGlStage::RestoreState:
    case modern::TrajectoryInsetVisibleFinalGlStage::None:
      error = "trajectory_inset_final_invalid_stage";
      return false;
    }
    return false;
  }

  bool executeContentOp(
      const modern::TrajectoryInsetVisibleFinalContentOp &op,
      modern::TrajectoryInsetVisibleFinalContentOpReceipt &receipt,
      std::string &error) override {
    normalize();
    switch (op.kind) {
    case modern::TrajectoryInsetVisibleFinalContentOpKind::
        ClearFinalSemanticR32ui: {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, finalFramebuffer_.id());
      const GLenum semanticOnly = GL_COLOR_ATTACHMENT1;
      glDrawBuffers(1, &semanticOnly);
      const GLuint zero = op.clearIntegerValue;
      glClearBufferuiv(GL_COLOR, 0, &zero);
      break;
    }
    case modern::TrajectoryInsetVisibleFinalContentOpKind::
        CopyCurrentProductLdr:
      if (!copyCurrentProductLdrToFinal(op, error))
        return false;
      break;
    case modern::TrajectoryInsetVisibleFinalContentOpKind::
        CompositeCp3InsetColor:
      if (!compositeCp3InsetColorIntoFinal(op, error))
        return false;
      break;
    case modern::TrajectoryInsetVisibleFinalContentOpKind::
        CopyCp3InsetSemantic:
      if (!writeIndependentInsetSemantic(op, error))
        return false;
      break;
    }
    const GLenum adjacent = glGetError();
    receipt.kind = op.kind;
    receipt.actualSource = op.source;
    receipt.actualDestination = framebufferKey(finalFramebuffer_.id());
    receipt.actualDestinationAttachment = op.destinationAttachment;
    receipt.actualSourceRect = op.sourceRect;
    receipt.actualDestinationRect = op.destinationRect;
    receipt.actualSourceFormat =
        op.kind == modern::TrajectoryInsetVisibleFinalContentOpKind::
                       CopyCp3InsetSemantic
            ? queriedTextureFormat(static_cast<GLuint>(op.source.handle))
            : op.sourceFormat;
    receipt.actualSourceGeneration = op.sourceGeneration;
    receipt.actualIntegerSourceChannel = op.integerSourceChannel;
    receipt.integerShaderConversionToR32ui =
        op.kind == modern::TrajectoryInsetVisibleFinalContentOpKind::
                       CopyCp3InsetSemantic;
    receipt.actualWriteCount = adjacent == GL_NO_ERROR ? 1 : 0;
    receipt.glError = static_cast<unsigned>(adjacent);
    return adjacent == GL_NO_ERROR;
  }

  unsigned readError(modern::TrajectoryInsetVisibleFinalGlStage) override {
    // trajectory_inset_final_error_query
    return static_cast<unsigned>(glGetError());
  }

  bool restoreState(std::uint64_t saved, std::string &) override {
    // trajectory_inset_final_restore
    if (!hasCapturedState_ || saved != captureSerial_)
      return false;
    std::string error;
    return stateOwner_.restoreState(capturedState_, error);
  }

  void destroy(modern::TrajectoryInsetVisibleFinalGlResources &resources)
      noexcept override {
    publishedFramebuffer_ = 0;
    finalFramebuffer_.reset();
    contentReadFramebuffer_.reset();
    semanticConvertProgram_.reset();
    semanticConvertVao_.reset();
    finalSemantic_.reset();
    finalColor_.reset();
    resources = {};
  }

  void terminalPublishOutputHandleNoexcept(
      const modern::TrajectoryInsetVisibleFinalGlResources &resources)
      noexcept override {
    terminalPublishTrajectoryInsetOutputNoexcept(publishedFramebuffer_,
                                                 resources);
  }

  GLuint publishedProductOutputFramebuffer() const noexcept {
    return publishedFramebuffer_;
  }

  void poison() noexcept { poisoned_ = true; }

private:
  static Program makeSemanticConvertProgram() {
    const char *vertex = R"GLSL(#version 330 core
void main(){vec2 p=vec2((gl_VertexID==1)?3.0:-1.0,
                       (gl_VertexID==2)?3.0:-1.0);gl_Position=vec4(p,0,1);}
)GLSL";
    const char *fragment = R"GLSL(#version 330 core
layout(location=0) out uint outToken;
uniform usampler2D uResolvedSemantic;
uniform ivec2 uSourceOrigin;
uniform ivec2 uDestinationOrigin;
void main(){ivec2 p=uSourceOrigin+ivec2(floor(gl_FragCoord.xy))-uDestinationOrigin;
  outToken=texelFetch(uResolvedSemantic,p,0).r;}
)GLSL";
    const Shader vs = compileShader(ShaderStage::Vertex, vertex,
                                    "trajectory_inset_final_semantic_vs");
    const Shader fs = compileShader(ShaderStage::Fragment, fragment,
                                    "trajectory_inset_final_semantic_fs");
    return linkProgram(vs, fs, "trajectory_inset_final_semantic_program");
  }

  static int textureSamples(GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);
    GLint width = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    return width > 0 ? 1 : 0;
  }

  static std::string queriedTextureFormat(GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);
    GLint format = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
                             &format);
    if (format == GL_RGBA32UI) return "GL_RGBA32UI";
    if (format == GL_R32UI) return "GL_R32UI";
    if (format == GL_SRGB8_ALPHA8) return "GL_SRGB8_ALPHA8";
    return "GL_OTHER";
  }

  bool blitColor(const modern::TrajectoryInsetVisibleFinalContentOp &op,
                 std::string &error) {
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(op.source.handle));
    GLint sourceHeight = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&sourceHeight);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, contentReadFramebuffer_.id());
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, static_cast<GLuint>(op.source.handle),
                           0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
      error = "trajectory_inset_final_content_source_incomplete";
      return false;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, finalFramebuffer_.id());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    const auto ix = [](float value) { return static_cast<GLint>(std::lround(value)); };
    // Content-op rectangles share the product UI's top-left physical origin.
    // Both native framebuffer endpoints require bottom-left coordinates.
    const GLint sourceBottom = sourceHeight - ix(op.sourceRect.y + op.sourceRect.height);
    const GLint destinationBottom = current_.shellRequest.physicalHeight -
        ix(op.destinationRect.y + op.destinationRect.height);
    glBlitFramebuffer(ix(op.sourceRect.x), sourceBottom,
                      ix(op.sourceRect.x + op.sourceRect.width),
                      sourceBottom + ix(op.sourceRect.height),
                      ix(op.destinationRect.x), destinationBottom,
                      ix(op.destinationRect.x + op.destinationRect.width),
                      destinationBottom + ix(op.destinationRect.height),
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    return true;
  }

  bool copyCurrentProductLdrToFinal(
      const modern::TrajectoryInsetVisibleFinalContentOp &op,
      std::string &error) {
    return blitColor(op, error);
  }

  bool compositeCp3InsetColorIntoFinal(
      const modern::TrajectoryInsetVisibleFinalContentOp &op,
      std::string &error) {
    return blitColor(op, error);
  }

  bool convertSemantic(const modern::TrajectoryInsetVisibleFinalContentOp &op,
                       std::string &error) {
    if (queriedTextureFormat(static_cast<GLuint>(op.source.handle)) !=
            "GL_RGBA32UI" ||
        op.integerSourceChannel != 0) {
      error = "trajectory_inset_final_semantic_source_invalid";
      return false;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, finalFramebuffer_.id());
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    GLint sourceHeight = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&sourceHeight);
    const GLint destinationBottom = current_.shellRequest.physicalHeight -
        static_cast<GLint>(std::lround(op.destinationRect.y + op.destinationRect.height));
    glViewport(static_cast<GLint>(std::lround(op.destinationRect.x)),
               destinationBottom,
               static_cast<GLsizei>(std::lround(op.destinationRect.width)),
               static_cast<GLsizei>(std::lround(op.destinationRect.height)));
    // Bind the typed source and its nearest sampler before selecting the
    // program; its default sampler unit 0 may hold a floating scene texture.
    glActiveTexture(GL_TEXTURE0 + 7);
    glBindSampler(7,0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(op.source.handle));
    glProgramUniform1i(semanticConvertProgram_.id(),
        glGetUniformLocation(semanticConvertProgram_.id(),"uResolvedSemantic"),7);
    glUseProgram(semanticConvertProgram_.id());
    glBindVertexArray(semanticConvertVao_.id());
    glUniform2i(glGetUniformLocation(semanticConvertProgram_.id(),
                                    "uSourceOrigin"),
                static_cast<GLint>(std::lround(op.sourceRect.x)),
                sourceHeight-static_cast<GLint>(std::lround(op.sourceRect.y+op.sourceRect.height)));
    glUniform2i(glGetUniformLocation(semanticConvertProgram_.id(),
                                    "uDestinationOrigin"),
                static_cast<GLint>(std::lround(op.destinationRect.x)),
                destinationBottom);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (std::getenv("BILLIARDGL_INSET_FINAL_PROBE_SELF_TEST")) {
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      std::size_t exactTargetPairs = 0;
      for (const auto& probe : current_.package.probes) {
        std::array<GLuint,4> sourceTokens{};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, contentReadFramebuffer_.id());
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, static_cast<GLuint>(op.source.handle), 0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(probe.strokeXPhysicalPx-static_cast<int>(std::lround(current_.package.layout.plotRect.x)),
            sourceHeight-1-(probe.strokeYPhysicalPx-static_cast<int>(std::lround(current_.package.layout.plotRect.y))),
            1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, sourceTokens.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, finalFramebuffer_.id());
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        GLuint token = 0;
        glReadPixels(probe.strokeXPhysicalPx,
            current_.shellRequest.physicalHeight - 1 - probe.strokeYPhysicalPx,
            1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &token);
        const bool targetPresent = std::find(sourceTokens.begin(),
            sourceTokens.begin()+current_.request.samples, probe.insetOwnershipToken) !=
            sourceTokens.begin()+current_.request.samples;
        if (!targetPresent || token != sourceTokens[0]) {
          error = "trajectory_inset_final_probe_token_mismatch:" + probe.probeId +
              ":expected=" + std::to_string(probe.insetOwnershipToken) +
              ":actual=" + std::to_string(token) + ":xy=" +
              std::to_string(probe.strokeXPhysicalPx) + ',' + std::to_string(probe.strokeYPhysicalPx);
          return false;
        }
        if (token == probe.insetOwnershipToken) ++exactTargetPairs;
      }
      std::cout << "inset_final_semantic_probes frame=" << current_.request.frameSerial
          << " pairs=" << current_.package.probes.size() << " lane0_copy=PASS source_target_present=PASS"
          << " lane0_target_pairs=" << exactTargetPairs << "\n";
      const int copyWidth = static_cast<int>(std::lround(op.destinationRect.width));
      const int copyHeight = static_cast<int>(std::lround(op.destinationRect.height));
      const std::size_t pixelCount = static_cast<std::size_t>(copyWidth) * copyHeight;
      std::vector<GLuint> sourcePixels(pixelCount * 4), finalPixels(pixelCount);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, contentReadFramebuffer_.id());
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      glReadPixels(static_cast<int>(std::lround(op.sourceRect.x)),
          sourceHeight-static_cast<int>(std::lround(op.sourceRect.y+op.sourceRect.height)),
          copyWidth, copyHeight, GL_RGBA_INTEGER, GL_UNSIGNED_INT, sourcePixels.data());
      glBindFramebuffer(GL_READ_FRAMEBUFFER, finalFramebuffer_.id());
      glReadBuffer(GL_COLOR_ATTACHMENT1);
      glReadPixels(static_cast<int>(std::lround(op.destinationRect.x)), destinationBottom,
          copyWidth, copyHeight, GL_RED_INTEGER, GL_UNSIGNED_INT, finalPixels.data());
      for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        if (finalPixels[pixel] != sourcePixels[pixel * 4]) {
          error = "trajectory_inset_final_plot_lane0_copy_mismatch:pixel=" + std::to_string(pixel);
          return false;
        }
      }
      std::cout << "inset_final_semantic_plot frame=" << current_.request.frameSerial
          << " width=" << copyWidth << " height=" << copyHeight
          << " pixels=" << pixelCount << " lane0_copy=PASS\n";
    }
    return true;
  }

  bool writeIndependentInsetSemantic(
      const modern::TrajectoryInsetVisibleFinalContentOp &op,
      std::string &error) {
    return convertSemantic(op, error);
  }

  static modern::TrajectoryInsetFinalResourceKey textureKey(GLuint handle) {
    return {modern::TrajectoryInsetFinalResourceKind::Texture, handle};
  }
  static modern::TrajectoryInsetFinalResourceKey framebufferKey(GLuint handle) {
    return {modern::TrajectoryInsetFinalResourceKind::Framebuffer, handle};
  }
  static void allocateTexture(GLuint handle, GLint internal, int width,
                              int height, GLenum format, GLenum type) {
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, type,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  static void normalize() {
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }
  bool attachmentsExact() const {
    GLint color = 0, semantic = 0;
    glGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &color);
    glGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &semantic);
    return static_cast<GLuint>(color) == finalColor_.id() &&
           static_cast<GLuint>(semantic) == finalSemantic_.id();
  }

  GLuint cp3Staging_ = 0, productLdr_ = 0, mainSemantic_ = 0,
         publishedFramebuffer_ = 0;
  const ui::TrajectoryInsetVisibleCurrentFrame &current_;
  ModernTrajectoryInsetGlDispatch &stateOwner_;
  modern::TrajectoryInsetGlState capturedState_;
  std::uint64_t captureSerial_ = 0;
  bool hasCapturedState_ = false;
  Texture2D finalColor_, finalSemantic_;
  Framebuffer finalFramebuffer_, contentReadFramebuffer_;
  Program semanticConvertProgram_;
  VertexArray semanticConvertVao_;
  bool poisoned_ = false;
};

struct ModernRenderer::Impl {
  explicit Impl(ModernRendererOptions requested)
      : options(std::move(requested)) {}

  ModernRendererOptions options;
  RendererInitialization initialization;
  LookdevLoadResult lookdevResult;
  PbrPackageDescription pbrPackage;
  PbrTextureResources pbrTextures;
  int ballAreaLightSamples = 0;
  int ballLightingMode = 0;
  float ballShadowBiasScale = 4.0f;
  float gltfContactShadowScale = 0.25f;
  bool seamlessCubemapEnabled = true;
  bool seamlessCubemapPreviouslyEnabled = false;
  bool seamlessCubemapStateCaptured = false;
  ModernFrameMetrics frameMetrics;
  bool initialized = false;
  Program sceneProgram;
  Program depthProgram;
  Program toneProgram;
  Program meshProgram;
  Program meshDepthProgram;
  Program sparsePropReceiverProgram;
  Program trajectoryOverlayProgram;
  Program finalOwnershipResolveSingleProgram;
  Program finalOwnershipResolveMsaaProgram;
  Buffer quadBuffer;
  VertexArray quadVao;
  Buffer trajectoryOverlayBuffer;
  VertexArray trajectoryOverlayVao;
  Framebuffer shadowFramebuffer;
  Texture2D shadowDepth;
  Framebuffer sparsePropShadowFramebuffer;
  Texture2D sparsePropShadowDepth;
  SparsePropShadowProjection sparsePropProjection;
  bool sparsePropContractReady = false;
  Framebuffer hdrFramebuffer;
  Texture2D hdrColor;
  Texture2D hdrDepth;
  Texture2D plainFallbackColor;
  Texture2D plainFallbackEnvironment;
  Framebuffer trajectoryBackdropFramebuffer;
  Texture2D trajectoryBackdropColor;
  Texture2D trajectoryBackdropDepth;
  Framebuffer trajectoryCompositedBackdropFramebuffer;
  Texture2D trajectoryCompositedBackdropColor;
  Framebuffer ldrFramebuffer;
  Texture2D ldrColor;
  Framebuffer msaaFramebuffer;
  GLuint msaaColor = 0;
  GLuint msaaDepth = 0;
  Texture2D finalOwnershipScene;
  Texture2D finalOwnershipMsaa;
  Framebuffer finalOwnershipResolvedFramebuffer;
  Texture2D finalOwnershipResolved;
  Texture2D finalOwnershipUiCoverage;
  Texture2D finalOwnershipChromeCoverage;
  Framebuffer finalOwnershipPreProductUiFramebuffer;
  Texture2D finalOwnershipPreProductUiColor;
  bool finalOwnershipPreProductUiReady = false;
  std::uint64_t renderedFrameSerial = 0;
  bool finalOwnershipFrameReady = false;
  bool finalOwnershipProductUiApplied = false;
  struct PendingGpuQuery {
    GLuint id = 0;
    std::uint64_t sourceFrame = 0;
  };
  std::unordered_map<std::string, std::deque<PendingGpuQuery>> pendingQueries;
  std::unordered_map<std::string, double> latestGpuMilliseconds;
  gltf::ImportedGltfPackage importedScene;
  std::array<float,18> pocketMouthsCm{};
  gltf::CpuDrawPackage cpuScene;
  gltf::GpuDrawPackage gpuScene;
  gltf::DrawList sceneDrawList;
  gltf::ImportedGltfPackage importedVenue;
  gltf::CpuDrawPackage cpuVenue;
  gltf::GpuDrawPackage gpuVenue;
  gltf::DrawList venueDrawList;
  int ballMeshIndex = -1;
  bool gltfSceneLoaded = false;
  bool venueSceneLoaded = false;
  std::size_t lastSceneTriangles = 0;
  std::set<std::string> drawClassificationReported;
  std::unique_ptr<ui::Core41ProductUiGlDispatch> productUiDispatch;
  std::unique_ptr<ModernProductUiRuntime> productUiRuntime;
  std::unique_ptr<ModernRendererProductUiPass> productUiPass;
  std::unique_ptr<ModernTrajectoryInsetGlDispatch> trajectoryInsetDispatch;
  std::unique_ptr<ModernTrajectoryInsetVisibleFinalGlDispatch>
      trajectoryInsetVisibleFinalDispatch;
  modern::TrajectoryInsetVisibleFinalGlResult trajectoryInsetVisibleFinalResult;
  GLuint trajectoryInsetPublishedOutputFramebuffer = 0;
  std::optional<product_hud::HudRect> trajectoryInsetToggleRect;
  std::optional<product_hud::HudRect> trajectoryInsetPlotRect;
  std::uint64_t trajectoryInsetPresentedSerial = 0;
  bool trajectoryInsetFinalContextPoisoned = false;
  TrajectoryInsetAtomicPreparation trajectoryInsetPreparation;
  std::uint64_t lastTrajectoryInsetAttemptSerial = 0;
  ModernProductUiRuntimeResult latestProductUiResult;
  std::string trajectoryOverlayCacheKey;
  training::TrajectoryOverlayDrawPackage trajectoryOverlayPackage;

  void applySeamlessCubemapState(bool enabled) noexcept {
    seamlessCubemapPreviouslyEnabled =
        glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) == GL_TRUE;
    seamlessCubemapStateCaptured = true;
    if (enabled != seamlessCubemapPreviouslyEnabled) {
      if (enabled)
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
      else
        glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }
  }

  void restoreSeamlessCubemapState() noexcept {
    if (!seamlessCubemapStateCaptured)
      return;
    if (seamlessCubemapPreviouslyEnabled)
      glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    else
      glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    seamlessCubemapStateCaptured = false;
  }

  float calibratedExposureScale() const {
    const float exposure = pbrTextures.report().loadedPackage
                               ? pbrPackage.lighting.exposureEv100
                               : lookdevResult.settings.exposureEv100;
    return (gltfSceneLoaded ? 0.80f : 0.25f) * std::exp2(7.2f - exposure);
  }

  std::uint32_t
  drawTrajectoryOverlay(const ModernProductSessionSnapshot &product,
                        const PresentationSnapshot &presentation,
                        GLuint sceneFbo) {
    const ProductScreen screen = product.shell.screen;
    if (screen != ProductScreen::TrainingSession &&
        screen != ProductScreen::ReplayPlayback)
      return 0;
    if (!product.shell.worldTrajectoryOverlayEnabled) {
      trajectoryOverlayPackage = {};
      trajectoryOverlayCacheKey.clear();
      return 0;
    }
    const float scale =
        product.shellViewport.logicalWidth > 0
            ? static_cast<float>(product.shellViewport.physicalWidth) /
                  product.shellViewport.logicalWidth
            : 1.0f;
    const training::TrajectoryOverlayViewport viewport{
        initialization.framebufferWidth, initialization.framebufferHeight,
        std::max(1.0f, scale), product.shell.uiScale};
    const bool cvd = product.effectiveSettings.colorVisionMode !=
                     settings::ColorVisionMode::Default;
    const training::TrajectoryOverlayPalette palette =
        cvd ? training::TrajectoryOverlayPalette::ColorVisionSafe
            : training::TrajectoryOverlayPalette::Standard;
    const auto buildStart = std::chrono::steady_clock::now();
    const std::string cacheKey =
        overlayCacheKey(product.shell.trajectoryOverlay, presentation.camera,
                        viewport, palette);
    if (cacheKey != trajectoryOverlayCacheKey) {
      trajectoryOverlayPackage = training::buildTrajectoryOverlayDrawPackage(
          product.shell.trajectoryOverlay, presentation.camera, viewport,
          palette);
      trajectoryOverlayCacheKey = cacheKey;
    }
    const training::TrajectoryOverlayDrawPackage &package =
        trajectoryOverlayPackage;
    frameMetrics.trajectoryOverlayCpuBuildMilliseconds =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - buildStart)
            .count();
    frameMetrics.trajectoryOverlayVertices = package.vertices.size();
    frameMetrics.trajectoryOverlayDrawCalls = package.additionalDrawCalls;
    frameMetrics.trajectoryOverlayTextureCount = package.newTextureCount;
    frameMetrics.trajectoryOverlayUsedSharedProjection =
        package.usedSharedCameraProjection;
    frameMetrics.trajectoryOverlayHomogeneousClip =
        package.homogeneousClipApplied;
    frameMetrics.trajectoryOverlaySafeAreaClip = package.safeAreaClipApplied;
    frameMetrics.trajectoryOverlayPlaySurfaceClip =
        package.playSurfaceStencilApplied;
    frameMetrics.trajectoryOverlayErrorCode = package.errorCode;
    if (!package.ok || package.vertices.empty())
      return 0;
    if (options.enableFinalOwnershipInstrumentation &&
        std::any_of(package.semanticBatches.begin(),
                    package.semanticBatches.end(), [](const auto &batch) {
                      return batch.finalOwnershipToken > 0x00ffffffu;
                    }))
      throw std::runtime_error("final_ownership_token_encoding_overflow");
    glUseProgram(trajectoryOverlayProgram.id());
    glBindVertexArray(trajectoryOverlayVao.id());
    glBindBuffer(GL_ARRAY_BUFFER, trajectoryOverlayBuffer.id());
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(package.vertices.size() *
                                sizeof(training::TrajectoryOverlayVertex)),
        package.vertices.data(), GL_STREAM_DRAW);
    glUniform2f(glGetUniformLocation(trajectoryOverlayProgram.id(),
                                     "uPhysicalViewport"),
                static_cast<float>(initialization.framebufferWidth),
                static_cast<float>(initialization.framebufferHeight));
    glActiveTexture(GL_TEXTURE3);
    glBindSampler(3, 0);
    glBindTexture(GL_TEXTURE_2D, trajectoryCompositedBackdropColor.id());
    glUniform1i(glGetUniformLocation(trajectoryOverlayProgram.id(),
                                     "uPreOverlayBackdropHdr"),
                3);
    glUniform1f(
        glGetUniformLocation(trajectoryOverlayProgram.id(), "uExposureScale"),
        calibratedExposureScale());
    glActiveTexture(GL_TEXTURE4);
    glBindSampler(4, 0);
    glBindTexture(GL_TEXTURE_2D, trajectoryBackdropColor.id());
    glUniform1i(glGetUniformLocation(trajectoryOverlayProgram.id(),
                                     "uSceneBackdropHdr"),
                4);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (options.enableFinalOwnershipInstrumentation) {
      const GLenum ownershipDrawBuffers[] = {GL_COLOR_ATTACHMENT0,
                                             GL_COLOR_ATTACHMENT1};
      glDrawBuffers(2, ownershipDrawBuffers);
    }
    const GLsizei actualCount =
        static_cast<GLsizei>(package.actual.vertexCount);
    const GLsizei predictedCount = static_cast<GLsizei>(
        package.vertices.size() - package.actual.vertexCount);
    std::uint32_t draws = 0;
    std::uint32_t submissionSequence = 0;
    if (actualCount > 0) {
      glDrawArrays(GL_TRIANGLES, 0, actualCount);
      ++draws;
      frameMetrics.trajectoryOverlayActualDrawSubmitted = true;
      submissionSequence = 1;
    }
    if (actualCount > 0 && predictedCount > 0) {
      glDisable(GL_BLEND);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                        trajectoryCompositedBackdropFramebuffer.id());
      glDrawBuffer(GL_COLOR_ATTACHMENT0);
      glBlitFramebuffer(0, 0, initialization.framebufferWidth,
                        initialization.framebufferHeight, 0, 0,
                        initialization.framebufferWidth,
                        initialization.framebufferHeight, GL_COLOR_BUFFER_BIT,
                        GL_NEAREST);
      frameMetrics.trajectoryOverlayActualBackdropFrozen = true;
      if (submissionSequence == 1)
        submissionSequence = 2;
      glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      if (options.enableFinalOwnershipInstrumentation) {
        const GLenum ownershipDrawBuffers[] = {GL_COLOR_ATTACHMENT0,
                                               GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, ownershipDrawBuffers);
      }
    }
    if (predictedCount > 0) {
      glDrawArrays(GL_TRIANGLES, actualCount, predictedCount);
      ++draws;
      frameMetrics.trajectoryOverlayPredictedDrawSubmitted = true;
      if (submissionSequence == 2)
        submissionSequence = 3;
    }
    frameMetrics.trajectoryOverlaySubmissionOrderValid =
        submissionSequence == 3 && draws == 2;
    if (options.enableFinalOwnershipInstrumentation) {
      const GLenum colorOnly = GL_COLOR_ATTACHMENT0;
      glDrawBuffers(1, &colorOnly);
    }
    glDisable(GL_BLEND);
    return draws;
  }

  ~Impl() { releaseTargets(); }

  void releaseTargets() noexcept {
    for (auto &entry : pendingQueries) {
      for (const PendingGpuQuery &query : entry.second)
        glDeleteQueries(1, &query.id);
    }
    pendingQueries.clear();
    latestGpuMilliseconds.clear();
    if (msaaColor != 0)
      glDeleteRenderbuffers(1, &msaaColor);
    if (msaaDepth != 0)
      glDeleteRenderbuffers(1, &msaaDepth);
    msaaColor = msaaDepth = 0;
    finalOwnershipUiCoverage.reset();
    finalOwnershipChromeCoverage.reset();
    finalOwnershipPreProductUiColor.reset();
    finalOwnershipPreProductUiFramebuffer.reset();
    finalOwnershipPreProductUiReady = false;
    finalOwnershipResolved.reset();
    finalOwnershipResolvedFramebuffer.reset();
    finalOwnershipMsaa.reset();
    finalOwnershipScene.reset();
    finalOwnershipFrameReady = false;
    trajectoryBackdropColor.reset();
    trajectoryBackdropDepth.reset();
    trajectoryBackdropFramebuffer.reset();
    trajectoryCompositedBackdropColor.reset();
    trajectoryCompositedBackdropFramebuffer.reset();
    msaaFramebuffer.reset();
    ldrColor.reset();
    ldrFramebuffer.reset();
    hdrColor.reset();
    hdrDepth.reset();
    hdrFramebuffer.reset();
    shadowDepth.reset();
    shadowFramebuffer.reset();
    sparsePropShadowDepth.reset();
    sparsePropShadowFramebuffer.reset();
  }

  void makeTargets(int width, int height) {
    releaseTargets();
    const auto quality = qualitySettings(options.quality);
    if (sparsePropContractReady && !options.objectClassMaskPass) {
      sparsePropShadowFramebuffer=createFramebuffer();
      sparsePropShadowDepth=createTexture2D();
      glBindTexture(GL_TEXTURE_2D,sparsePropShadowDepth.id());
      glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,SparsePropShadowProjection::mapSize,
                   SparsePropShadowProjection::mapSize,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      glBindFramebuffer(GL_FRAMEBUFFER,sparsePropShadowFramebuffer.id());
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,sparsePropShadowDepth.id(),0);
      glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
      requireComplete("sparse prop shadow depth");
    }
    shadowFramebuffer = createFramebuffer();
    shadowDepth = createTexture2D();
    glBindTexture(GL_TEXTURE_2D, shadowDepth.id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, quality.shadowMapSize,
                 quality.shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr std::array<float, 4> litBorder{{1.0f, 1.0f, 1.0f, 1.0f}};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, litBorder.data());
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           shadowDepth.id(), 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    requireComplete("shadow depth");

    hdrFramebuffer = createFramebuffer();
    hdrColor = createTexture2D();
    glBindTexture(GL_TEXTURE_2D, hdrColor.id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                 GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           hdrColor.id(), 0);
    // The non-MSAA path renders meshes directly into this framebuffer too.
    // A color-complete FBO without depth silently defeats depth testing.
    if (options.msaaSamples == 0) {
      hdrDepth = createTexture2D();
      glBindTexture(GL_TEXTURE_2D, hdrDepth.id());
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                   GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                             hdrDepth.id(), 0);
    }
    requireComplete("HDR resolve");

    trajectoryBackdropFramebuffer = createFramebuffer();
    trajectoryBackdropColor = createTexture2D();
    glBindTexture(GL_TEXTURE_2D, trajectoryBackdropColor.id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                 GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, trajectoryBackdropFramebuffer.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           trajectoryBackdropColor.id(), 0);
    requireComplete("trajectory pre-overlay backdrop");
    if (options.quality == RenderQuality::High) {
      trajectoryBackdropDepth = createTexture2D();
      glBindTexture(GL_TEXTURE_2D, trajectoryBackdropDepth.id());
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
          GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
          trajectoryBackdropDepth.id(), 0);
      requireComplete("scene depth sampling backdrop");
    }

    trajectoryCompositedBackdropFramebuffer = createFramebuffer();
    trajectoryCompositedBackdropColor = createTexture2D();
    glBindTexture(GL_TEXTURE_2D, trajectoryCompositedBackdropColor.id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                 GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,
                      trajectoryCompositedBackdropFramebuffer.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           trajectoryCompositedBackdropColor.id(), 0);
    requireComplete("trajectory actual-composited backdrop");

    ldrFramebuffer = createFramebuffer();
    ldrColor = createTexture2D();
    glBindTexture(GL_TEXTURE_2D, ldrColor.id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, ldrFramebuffer.id());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           ldrColor.id(), 0);
    requireComplete("sRGB output");

    if (options.msaaSamples > 0) {
      msaaFramebuffer = createFramebuffer();
      glGenRenderbuffers(1, &msaaColor);
      glBindRenderbuffer(GL_RENDERBUFFER, msaaColor);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, options.msaaSamples,
                                       GL_RGBA16F, width, height);
      glGenRenderbuffers(1, &msaaDepth);
      glBindRenderbuffer(GL_RENDERBUFFER, msaaDepth);
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, options.msaaSamples,
                                       GL_DEPTH_COMPONENT24, width, height);
      glBindFramebuffer(GL_FRAMEBUFFER, msaaFramebuffer.id());
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER, msaaColor);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                GL_RENDERBUFFER, msaaDepth);
      requireComplete("MSAA HDR");
    }
    if (options.enableFinalOwnershipInstrumentation) {
      if (options.msaaSamples > 0) {
        finalOwnershipMsaa = createTexture2D();
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, finalOwnershipMsaa.id());
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, options.msaaSamples,
                                GL_RGBA8, width, height, GL_TRUE);
        checkGl("allocate MSAA final ownership storage");
        glBindFramebuffer(GL_FRAMEBUFFER, msaaFramebuffer.id());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D_MULTISAMPLE,
                               finalOwnershipMsaa.id(), 0);
        requireComplete("MSAA final ownership");
        checkGl("attach MSAA final ownership storage");
      } else {
        finalOwnershipScene = createTexture2D();
        glBindTexture(GL_TEXTURE_2D, finalOwnershipScene.id());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        checkGl("allocate single-sample final ownership storage");
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer.id());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D, finalOwnershipScene.id(), 0);
        requireComplete("single-sample final ownership");
      }

      finalOwnershipResolvedFramebuffer = createFramebuffer();
      finalOwnershipResolved = createTexture2D();
      glBindTexture(GL_TEXTURE_2D, finalOwnershipResolved.id());
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, width, height, 0,
                   GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, finalOwnershipResolvedFramebuffer.id());
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, finalOwnershipResolved.id(), 0);
      requireComplete("resolved final ownership");
      checkGl("allocate resolved final ownership storage");

      finalOwnershipUiCoverage = createTexture2D();
      GLint maximumDrawBuffers = 0, maximumColorAttachments = 0;
      glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maximumDrawBuffers);
      glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maximumColorAttachments);
      if (maximumDrawBuffers < 3 || maximumColorAttachments < 3)
        throw std::runtime_error("final_ownership_three_attachments_unsupported");
      glBindTexture(GL_TEXTURE_2D, finalOwnershipUiCoverage.id());
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED,
                   GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, ldrFramebuffer.id());
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                             GL_TEXTURE_2D, finalOwnershipUiCoverage.id(), 0);
      requireComplete("final ownership UI coverage");
      checkGl("allocate final ownership UI coverage");
      finalOwnershipChromeCoverage = createTexture2D();
      glBindTexture(GL_TEXTURE_2D, finalOwnershipChromeCoverage.id());
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED,
                   GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                             GL_TEXTURE_2D, finalOwnershipChromeCoverage.id(), 0);
      requireComplete("final ownership interactive chrome coverage");

      finalOwnershipPreProductUiFramebuffer = createFramebuffer();
      finalOwnershipPreProductUiColor = createTexture2D();
      glBindTexture(GL_TEXTURE_2D, finalOwnershipPreProductUiColor.id());
      glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER,
                        finalOwnershipPreProductUiFramebuffer.id());
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D,
                             finalOwnershipPreProductUiColor.id(), 0);
      requireComplete("final ownership pre-product-UI color");
      checkGl("allocate final ownership pre-product-UI color");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void requireComplete(const char *name) {
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      GLint maximumSamples = 0;
      GLint maximumIntegerSamples = 0;
      glGetIntegerv(GL_MAX_SAMPLES, &maximumSamples);
      glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &maximumIntegerSamples);
      std::ostringstream detail;
      detail << "ModernRenderer incomplete framebuffer: " << name
             << " status=0x" << std::hex << status << std::dec
             << " requested_samples=" << options.msaaSamples
             << " max_samples=" << maximumSamples
             << " max_integer_samples=" << maximumIntegerSamples
             << " gl_error=0x" << std::hex << glGetError();
      throw std::runtime_error(detail.str());
    }
  }

  void primePbrTextureUnits() {
    const auto *material = pbrTextures.material("rubber");
    if (material == nullptr || !pbrTextures.ballAtlas().texture) {
      // The plain branch still belongs to a program with active PBR samplers.
      // Bind complete resources even when no external asset package is loaded;
      // an unbound sampler must not depend on a driver's branch elimination.
      if (!plainFallbackColor) {
        const unsigned char white[] = {255, 255, 255, 255};
        const unsigned char noEnvironment[] = {0, 0, 0, 255};
        plainFallbackColor = createTexture2D();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, plainFallbackColor.id());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        plainFallbackEnvironment = createTexture2D();
        glBindTexture(GL_TEXTURE_CUBE_MAP, plainFallbackEnvironment.id());
        for (int face = 0; face < 6; ++face)
          glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8,
                       1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, noEnvironment);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      }
      for (GLuint unit = 0; unit < 7; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindSampler(unit, 0);
        if (unit < 5)
          glBindTexture(GL_TEXTURE_2D,
              unit == 1 ? shadowDepth.id() : plainFallbackColor.id());
        else
          glBindTexture(GL_TEXTURE_CUBE_MAP, plainFallbackEnvironment.id());
      }
      glActiveTexture(GL_TEXTURE0);
      return;
    }
    const GLuint textures[] = {material->baseColor.texture.id(),
                               shadowDepth.id(), material->normal.texture.id(),
                               material->orm.texture.id(),
                               pbrTextures.ballAtlas().texture.id()};
    for (GLuint unit = 0; unit < 5; ++unit) {
      glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
      glBindTexture(GL_TEXTURE_2D, textures[unit]);
      glBindSampler(unit, unit == 1 ? 0 : pbrTextures.sampler().id());
    }
    glActiveTexture(GL_TEXTURE0);
  }

  RenderPassMetric timed(const std::string &name,
                         const std::function<std::uint32_t()> &work) {
    if (options.diagnosticPassTrace)
      std::cerr << "gpu-pass-begin: " << name << '\n';
    GLuint query = 0;
    glGenQueries(1, &query);
    if (options.debugErrors)
      checkGl((name + " query allocation").c_str());
    const auto start = std::chrono::steady_clock::now();
    glBeginQuery(GL_TIME_ELAPSED, query);
    if (options.debugErrors)
      checkGl((name + " query begin").c_str());
    const std::uint32_t draws = work();
    if (options.debugErrors)
      checkGl((name + " work").c_str());
    glEndQuery(GL_TIME_ELAPSED);
    if (options.debugErrors)
      checkGl((name + " query end").c_str());
    if (options.diagnosticPassTrace) {
      glFinish();
      std::cerr << "gpu-pass-end: " << name << '\n';
    }
    double gpuMilliseconds = 0.0;
    bool gpuTimingAvailable = false;
    std::uint64_t gpuTimingSourceFrame = 0;
    if (options.synchronousGpuTiming) {
      GLuint64 nanos = 0;
      glGetQueryObjectui64v(query, GL_QUERY_RESULT, &nanos);
      glDeleteQueries(1, &query);
      gpuMilliseconds = double(nanos) / 1e6;
      gpuTimingAvailable = true;
      gpuTimingSourceFrame = renderedFrameSerial;
    } else {
      auto &queue = pendingQueries[name];
      queue.push_back({query, renderedFrameSerial});
      while (!queue.empty()) {
        GLint available = GL_FALSE;
        glGetQueryObjectiv(queue.front().id, GL_QUERY_RESULT_AVAILABLE,
                           &available);
        if (available != GL_TRUE)
          break;
        GLuint64 nanos = 0;
        glGetQueryObjectui64v(queue.front().id, GL_QUERY_RESULT, &nanos);
        const PendingGpuQuery completed = queue.front();
        queue.pop_front();
        glDeleteQueries(1, &completed.id);
        latestGpuMilliseconds[name] = double(nanos) / 1e6;
        gpuTimingAvailable = true;
        gpuTimingSourceFrame = completed.sourceFrame;
      }
      const auto latest = latestGpuMilliseconds.find(name);
      if (latest != latestGpuMilliseconds.end())
        gpuMilliseconds = latest->second;
      // Availability means a newly completed query, not a cached value.  In
      // particular, operator[] must not insert a synthetic zero measurement.
    }
    const auto end = std::chrono::steady_clock::now();
    if (options.debugErrors)
      checkGl(name.c_str());
    return {name,
            std::chrono::duration<double, std::milli>(end - start).count(),
            gpuMilliseconds, draws, gpuTimingAvailable,
            gpuTimingSourceFrame};
  }

  void uniforms(const Program &program, float cx, float cy, float hx, float hy,
                float angle, float depth, const std::array<float, 3> &color,
                int shape, int identity = 0, float alpha = 1.0f) {
    glUniform2f(glGetUniformLocation(program.id(), "uCenter"), cx, cy);
    glUniform2f(glGetUniformLocation(program.id(), "uHalfSize"), hx, hy);
    glUniform1f(glGetUniformLocation(program.id(), "uAngle"), angle);
    glUniform1f(glGetUniformLocation(program.id(), "uDepth"), depth);
    glUniform4f(glGetUniformLocation(program.id(), "uColor"), color[0],
                color[1], color[2], alpha);
    glUniform1i(glGetUniformLocation(program.id(), "uShape"), shape);
    glUniform1i(glGetUniformLocation(program.id(), "uIdentity"), identity);
    glUniform1f(glGetUniformLocation(program.id(), "uRoughness"),
                lookdevResult.settings.ballRoughness);
    glUniform1f(glGetUniformLocation(program.id(), "uMetallic"), 0.0f);
    glUniform1f(glGetUniformLocation(program.id(), "uClearcoat"),
                lookdevResult.settings.ballClearcoat);
    const auto quality = qualitySettings(options.quality);
    glUniform1i(glGetUniformLocation(program.id(), "uPcfKernelWidth"),
                quality.pcfKernelWidth);
    glUniform1f(glGetUniformLocation(program.id(), "uShadowBias"),
                quality.shadowBias + quality.normalBias * 0.25f);
    glUniform1i(glGetUniformLocation(program.id(), "uAreaLightSamples"),
                quality.rectangularLightSamples);
    glUniform1f(glGetUniformLocation(program.id(), "uContactShadowOpacity"),
                quality.contactShadowOpacity *
                    (gltfSceneLoaded ? gltfContactShadowScale : 1.0f));
  }

  void plainMaterial(const Program &program) {
    glUniform1i(glGetUniformLocation(program.id(), "uPbrMode"), 0);
    glUniform2f(glGetUniformLocation(program.id(), "uUvRepeat"), 1.0f, 1.0f);
    // Keep every active sampler type on a distinct unit even when the plain
    // material branch does not sample it. Core-profile validation considers
    // sampler aliasing across the whole linked program, not just the branch
    // taken by this draw (the UI is the first sceneProgram draw in glTF mode).
    glUniform1i(glGetUniformLocation(program.id(), "uColorTexture"), 0);
    glUniform1i(glGetUniformLocation(program.id(), "uShadowMap"), 1);
    glUniform1i(glGetUniformLocation(program.id(), "uNormalTexture"), 2);
    glUniform1i(glGetUniformLocation(program.id(), "uOrmTexture"), 3);
    glUniform1i(glGetUniformLocation(program.id(), "uBallAtlas"), 4);
    glUniform1i(glGetUniformLocation(program.id(), "uIrradianceMap"), 5);
    glUniform1i(glGetUniformLocation(program.id(), "uSpecularMap"), 6);
  }

  bool bindMaterial(const Program &program, const std::string &name) {
    const auto *material = pbrTextures.material(name);
    if (material == nullptr) {
      plainMaterial(program);
      return false;
    }
    const GLuint textures[] = {material->baseColor.texture.id(),
                               material->normal.texture.id(),
                               material->orm.texture.id()};
    const GLint units[] = {0, 2, 3};
    const char *uniforms[] = {"uColorTexture", "uNormalTexture", "uOrmTexture"};
    for (int index = 0; index < 3; ++index) {
      glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + units[index]));
      glBindTexture(GL_TEXTURE_2D, textures[index]);
      glBindSampler(static_cast<GLuint>(units[index]),
                    pbrTextures.sampler().id());
      glUniform1i(glGetUniformLocation(program.id(), uniforms[index]),
                  units[index]);
    }
    glUniform1i(glGetUniformLocation(program.id(), "uPbrMode"), 1);
    glUniform2f(glGetUniformLocation(program.id(), "uUvRepeat"),
                material->uvRepeat[0], material->uvRepeat[1]);
    glUniform1f(glGetUniformLocation(program.id(), "uRoughness"),
                material->roughnessFallback);
    glUniform1f(glGetUniformLocation(program.id(), "uMetallic"),
                material->metallicFallback);
    glUniform1f(glGetUniformLocation(program.id(), "uClearcoat"),
                material->clearcoatFallback);
    return true;
  }

  void bindBallAtlas(const Program &program) {
    if (!pbrTextures.ballAtlas().texture) {
      plainMaterial(program);
      return;
    }
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, pbrTextures.ballAtlas().texture.id());
    glBindSampler(4, pbrTextures.sampler().id());
    glUniform1i(glGetUniformLocation(program.id(), "uBallAtlas"), 4);
    glUniform1i(glGetUniformLocation(program.id(), "uPbrMode"), 2);
    glUniform1f(glGetUniformLocation(program.id(), "uRoughness"),
                lookdevResult.settings.ballRoughness);
    glUniform1f(glGetUniformLocation(program.id(), "uMetallic"), 0.0f);
    glUniform1f(glGetUniformLocation(program.id(), "uClearcoat"),
                lookdevResult.settings.ballClearcoat);
  }

  void bindEnvironment(const Program &program) {
    if (!pbrTextures.irradianceMap() || !pbrTextures.specularMap())
      return;
    const bool normalizedContract = pbrPackage.lighting.environmentContract ==
        EnvironmentRadianceContract::NormalizedTintLuminanceV2;
    glUniform1i(glGetUniformLocation(program.id(), "uLightingContractV2"),
                normalizedContract ? 1 : 0);
    const auto normalizedScales =
        normalizedLightingScales(pbrPackage.lighting);
    glUniform1f(glGetUniformLocation(program.id(), "uDirectKeyIntensityScale"),
                normalizedScales.directKey);
    glUniform1f(glGetUniformLocation(program.id(), "uV2AmbientFloor"),
                normalizedScales.ambientFloor);
    glUniform1f(glGetUniformLocation(program.id(), "uV2IrradianceScale"),
                normalizedScales.irradiance);
    glUniform1f(glGetUniformLocation(program.id(), "uV2ReflectedScale"),
                normalizedScales.reflected);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pbrTextures.irradianceMap().id());
    glUniform1i(glGetUniformLocation(program.id(), "uIrradianceMap"), 5);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pbrTextures.specularMap().id());
    glUniform1i(glGetUniformLocation(program.id(), "uSpecularMap"), 6);
  }

  const gltf::PbrMaterialBinding *
  pbrBindingForMaterial(const gltf::ImportedGltfPackage &package,
                        int materialIndex) const {
    for (const auto &binding : package.pbrBindings)
      if (binding.materialIndex == materialIndex)
        return &binding;
    return nullptr;
  }

  void bindGltfMaterial(const gltf::ImportedGltfPackage &package,
                        int materialIndex) {
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uObjectIdMode"), 0);
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uPocketSidewall"), 0);
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uPocketIndirectEnabled"), 0);
    if (materialIndex < 0 || static_cast<std::size_t>(materialIndex) >=
                                 package.asset.materials.size()) {
      plainMaterial(meshProgram);
      glUniform4f(glGetUniformLocation(meshProgram.id(), "uColor"), 1, 0, 1, 1);
      return;
    }
    const auto &source =
        package.asset.materials[static_cast<std::size_t>(materialIndex)];
    const auto *authoredBinding = pbrBindingForMaterial(package, materialIndex);
    std::string set =
        authoredBinding ? authoredBinding->textureSet : std::string{};
    if (set.empty() && source.name.find("venue") != std::string::npos) {
      if (source.name.find("walnut") != std::string::npos)
        set = "walnut";
      else if (source.name.find("gunmetal") != std::string::npos)
        set = "gunmetal";
      else if (source.name.find("floor") != std::string::npos)
        set = "rubber";
    }
    std::array<float, 3> tint{{source.baseColorFactor.x,
                               source.baseColorFactor.y,
                               source.baseColorFactor.z}};
    if (!set.empty()) {
      bindMaterial(meshProgram, set);
      if (authoredBinding)
        glUniform2f(glGetUniformLocation(meshProgram.id(), "uUvRepeat"),
                    authoredBinding->uvRepeat.x, authoredBinding->uvRepeat.y);
    } else {
      plainMaterial(meshProgram);
      glUniform1f(glGetUniformLocation(meshProgram.id(), "uRoughness"),
                  source.roughnessFactor);
      const float metallic =
          source.name.find("venue") != std::string::npos &&
                  source.name.find("gunmetal") == std::string::npos
              ? 0.0f
              : source.metallicFactor;
      glUniform1f(glGetUniformLocation(meshProgram.id(), "uMetallic"),
                  metallic);
      glUniform1f(glGetUniformLocation(meshProgram.id(), "uClearcoat"),
                  source.hasClearcoat ? source.clearcoatFactor : 0.0f);
    }
    if (source.name == "MAT_background")
      tint = {{0.018f, 0.015f, 0.013f}};
    if (source.name == "MAT_venue_acoustic")
      tint = {{0.34f, 0.37f, 0.40f}};
    if (source.name == "MAT_venue_walnut_trim")
      tint = {{3.20f, 1.40f, 0.50f}};
    glUniform4f(glGetUniformLocation(meshProgram.id(), "uColor"), tint[0],
                tint[1], tint[2], source.baseColorFactor.w);
  }

  void bindPocketObjectId(const std::string &node) {
    plainMaterial(meshProgram);
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uObjectIdMode"), 1);
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uPocketSidewall"), 0);
    std::array<float, 3> color{{0.0f, 0.0f, 0.0f}};
    if (node.find("pocket_liner_rim") != std::string::npos)
      color = {{1.0f, 0.0f, 0.0f}};
    else if (node.find("pocket_liner_sidewall") != std::string::npos || node.find("pocket_inner") != std::string::npos)
      color = {{0.0f, 1.0f, 0.0f}};
    else if (node.find("pocket_void") != std::string::npos)
      color = {{0.0f, 0.0f, 1.0f}};
    else if (node.find("pocket_apron") != std::string::npos ||
             node.find("table_body") != std::string::npos)
      color = {{1.0f, 1.0f, 0.0f}};
    else if (node.find("pocket_leather") != std::string::npos)
      color = {{1.0f, 0.0f, 1.0f}};
    else if (node.find("table_rail") != std::string::npos)
      color = {{0.0f, 1.0f, 1.0f}};
    glUniform4f(glGetUniformLocation(meshProgram.id(), "uColor"), color[0],
                color[1], color[2], 1.0f);
  }

  void bindObjectClassMask(int identity) {
    const std::array<float,3> color = identity == 1 ? std::array<float,3>{{1,0,0}} :
        identity == 2 ? std::array<float,3>{{0,1,0}} :
        identity == 3 ? std::array<float,3>{{0,0,1}} :
        identity == 4 ? std::array<float,3>{{1,1,0}} : std::array<float,3>{{0,0,0}};
    glUniform4f(glGetUniformLocation(meshProgram.id(),"uColor"),color[0],color[1],color[2],1);
  }

  void meshTransform(const Matrix &viewProjection, const Matrix &model) {
    const Matrix mvp = multiply(viewProjection, model);
    glUniformMatrix4fv(glGetUniformLocation(meshProgram.id(), "uModel"), 1,
                       GL_FALSE, model.data());
    glUniformMatrix4fv(glGetUniformLocation(meshProgram.id(), "uMvp"), 1,
                       GL_FALSE, mvp.data());
  }

  static bool gltfNodeCastsTableShadow(const std::string &node) {
    return node.find("table_felt") == std::string::npos &&
           node.find("pocket_void") == std::string::npos &&
           node.find("lamp_") == std::string::npos &&
           node.find("background_") == std::string::npos &&
           node.find("ball_shared") == std::string::npos &&
           node.find("cue_") == std::string::npos;
  }

  static bool gltfNodeReceivesTableShadow(const std::string &node) {
    return node.find("lamp_") == std::string::npos &&
           node.find("background_") == std::string::npos &&
           node.find("pocket_void") == std::string::npos;
  }

  void meshDepthTransform(const Matrix &lightViewProjection,
                          const Matrix &model) {
    const Matrix lightMvp = multiply(lightViewProjection, model);
    glUniformMatrix4fv(
        glGetUniformLocation(meshDepthProgram.id(), "uLightMvp"), 1,
        GL_FALSE, lightMvp.data());
  }

  std::uint32_t drawGltfShadowScene(const PresentationSnapshot &snapshot) {
    if (!gltfSceneLoaded || !meshDepthProgram)
      return 0;
    const Matrix lightViewProjection =
        tableShadowViewProjection(pbrPackage.lighting);
    glUseProgram(meshDepthProgram.id());
    glUniform1i(glGetUniformLocation(meshDepthProgram.id(), "uInstancedBalls"), 0);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    std::uint32_t draws = 0;
    std::uint32_t tableDraws = 0, ballDraws = 0, cueDraws = 0, ballInstances = 0;

    for (const auto &item : sceneDrawList.items) {
      if (item.meshIndex < 0 ||
          static_cast<std::size_t>(item.meshIndex) >= gpuScene.meshes.size())
        continue;
      const std::string &node =
          cpuScene.nodes[static_cast<std::size_t>(item.nodeIndex)].name;
      if (!gltfNodeCastsTableShadow(node))
        continue;
      meshDepthTransform(lightViewProjection, item.worldTransformCm);
      glBindVertexArray(
          gpuScene.meshes[static_cast<std::size_t>(item.meshIndex)]
              .vertexArray.id());
      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.indexCount),
                     GL_UNSIGNED_INT,
                     reinterpret_cast<const void *>(static_cast<std::uintptr_t>(
                         item.firstIndex * sizeof(std::uint32_t))));
      ++draws;
      ++tableDraws;
    }

    if (ballMeshIndex >= 0 &&
        static_cast<std::size_t>(ballMeshIndex) < gpuScene.meshes.size()) {
      const auto &cpuMesh =
          cpuScene.meshes[static_cast<std::size_t>(ballMeshIndex)];
      const auto &gpuMesh =
          gpuScene.meshes[static_cast<std::size_t>(ballMeshIndex)];
      glBindVertexArray(gpuMesh.vertexArray.id());
      static_assert(kPresentationBallCount == 16, "depth uniform instance capacity follows presentation balls");
      std::array<Matrix, kPresentationBallCount> lightMvps{};
      for (const auto &ball : snapshot.balls)
        if (ball.visible && ball.castsShadow)
          lightMvps[ballInstances++] = multiply(lightViewProjection, ballModel(ball));
      const bool reference = std::getenv("BILLIARDGL_BALL_SHADOW_BATCH_REFERENCE") != nullptr;
      if (reference) {
        // Native counterfactual only: the original per-ball submissions, with
        // the same CPU matrices and depth state, remain available for pairing.
        for (std::uint32_t instance = 0; instance < ballInstances; ++instance) {
          glUniformMatrix4fv(glGetUniformLocation(meshDepthProgram.id(), "uLightMvp"),
              1, GL_FALSE, lightMvps[instance].data());
          for (const auto &primitive : cpuMesh.primitives) {
            glDrawElements(
                GL_TRIANGLES, static_cast<GLsizei>(primitive.indexCount),
                GL_UNSIGNED_INT,
                reinterpret_cast<const void *>(static_cast<std::uintptr_t>(
                    primitive.firstIndex * sizeof(std::uint32_t))));
            ++draws;
            ++ballDraws;
          }
        }
      } else if (ballInstances != 0) {
        // Shared sphere geometry/depth material; only the already-computed
        // per-ball MVP differs. No geometry, visibility, shadow bias, culling,
        // light projection or color/atlas submission changes with this batch.
        glUniformMatrix4fv(glGetUniformLocation(meshDepthProgram.id(), "uBallLightMvp[0]"),
            static_cast<GLsizei>(ballInstances), GL_FALSE, lightMvps.front().data());
        glUniform1i(glGetUniformLocation(meshDepthProgram.id(), "uInstancedBalls"), 1);
        for (const auto &primitive : cpuMesh.primitives) {
          glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(primitive.indexCount),
              GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(
                  primitive.firstIndex * sizeof(std::uint32_t))), static_cast<GLsizei>(ballInstances));
          ++draws;
          ++ballDraws;
        }
        glUniform1i(glGetUniformLocation(meshDepthProgram.id(), "uInstancedBalls"), 0);
      }
    }

    if (snapshot.cue.visible) {
      const Matrix dynamicCue = cueModel(snapshot.cue);
      for (const auto &item : sceneDrawList.items) {
        if (item.meshIndex < 0 ||
            static_cast<std::size_t>(item.meshIndex) >= gpuScene.meshes.size())
          continue;
        const std::string &node =
            cpuScene.nodes[static_cast<std::size_t>(item.nodeIndex)].name;
        if (node.find("cue_") == std::string::npos)
          continue;
        meshDepthTransform(lightViewProjection, dynamicCue);
        glBindVertexArray(
            gpuScene.meshes[static_cast<std::size_t>(item.meshIndex)]
                .vertexArray.id());
        glDrawElements(
            GL_TRIANGLES, static_cast<GLsizei>(item.indexCount),
            GL_UNSIGNED_INT,
            reinterpret_cast<const void *>(static_cast<std::uintptr_t>(
                item.firstIndex * sizeof(std::uint32_t))));
        ++draws;
        ++cueDraws;
      }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (std::getenv("BILLIARDGL_DRAW_CLASSIFICATION")) {
      const std::string identity = "shadow " + std::to_string(tableDraws) + '/' +
          std::to_string(ballDraws) + '/' + std::to_string(cueDraws) + '/' + std::to_string(ballInstances);
      if (drawClassificationReported.insert(identity).second)
        std::cout << "draw_classification pass=shadow_depth table=" << tableDraws
            << " ball_depth=" << ballDraws << " ball_instances=" << ballInstances
            << " cue=" << cueDraws << " total=" << draws << '\n';
    }
    return draws;
  }

  std::uint32_t drawSparsePropShadow() {
    std::vector<SparsePropShadowProjection::Point> vertices;
    const gltf::DrawItem* caster=nullptr;
    for(const auto& item:venueDrawList.items) {
      const auto& node=cpuVenue.nodes.at(static_cast<std::size_t>(item.nodeIndex));
      if(node.name.find("NODE_original_bench_and_shelf_LOD")!=0)continue;
      if(caster)throw std::runtime_error("sparse_shadow_requires_single_batch");
      caster=&item;
      const auto& mesh=cpuVenue.meshes.at(static_cast<std::size_t>(item.meshIndex));
      const auto& m=item.worldTransformCm;
      for(std::uint32_t i=0;i<item.indexCount;++i) {
        const auto& p=mesh.vertices.at(mesh.indices.at(item.firstIndex+i)).position;
        vertices.push_back({m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],
                            m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],
                            m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]});
      }
    }
    const auto& light=pbrPackage.lighting;
    sparsePropProjection=buildSparsePropShadowProjection(
        {light.keyPositionMetres[0]*100,light.keyPositionMetres[1]*100,light.keyPositionMetres[2]*100},
        light.keyEmissionNormal,directKeyIntensityScale(light),vertices);
    if(!sparsePropProjection.active)return 0;
    glUseProgram(meshDepthProgram.id());
    glUniform1i(glGetUniformLocation(meshDepthProgram.id(),"uInstancedBalls"),0);
    glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1,1);
    meshDepthTransform(sparsePropProjection.viewProjection,caster->worldTransformCm);
    glBindVertexArray(gpuVenue.meshes.at(static_cast<std::size_t>(caster->meshIndex)).vertexArray.id());
    glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(caster->indexCount),GL_UNSIGNED_INT,
                   reinterpret_cast<const void*>(static_cast<std::uintptr_t>(caster->firstIndex*sizeof(std::uint32_t))));
    glDisable(GL_POLYGON_OFFSET_FILL);
    return 1;
  }

  void bindGltfShadowReceiver(const Matrix &lightViewProjection,
                              bool enabled) {
    glUniformMatrix4fv(
        glGetUniformLocation(meshProgram.id(), "uLightViewProjection"), 1,
        GL_FALSE, lightViewProjection.data());
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uReceiveShadow"),
                enabled ? 1 : 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepth.id());
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uShadowMap"), 1);
    const auto quality = qualitySettings(options.quality);
    const std::size_t tier = options.quality == RenderQuality::Low
                                 ? 0u
                                 : (options.quality == RenderQuality::Medium
                                        ? 1u
                                        : 2u);
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uShadowFilterTaps"),
                pbrPackage.lighting.shadowFilterTaps[tier]);
    const char* nearestReference = std::getenv("BILLIARDGL_SHADOW_PCF_NEAREST_REFERENCE");
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uShadowCompareBilinear"),
                nearestReference && std::strcmp(nearestReference, "1") == 0 ? 0 : 1);
    glUniform1f(glGetUniformLocation(meshProgram.id(), "uShadowBias"),
                quality.shadowBias + quality.normalBias * 0.25f);
    glUniform1f(glGetUniformLocation(meshProgram.id(), "uBallShadowBiasScale"),
                ballShadowBiasScale);
    const float texel = 1.0f / static_cast<float>(quality.shadowMapSize);
    glUniform2f(glGetUniformLocation(meshProgram.id(), "uShadowTexelSize"),
                texel, texel);
    const float softnessCm =
        pbrPackage.lighting.shadowSoftnessMetres[tier] * 100.0f;
    glUniform2f(glGetUniformLocation(meshProgram.id(), "uShadowSoftnessUv"),
                softnessCm / (2.0f * 130.0f),
                softnessCm / (2.0f * 210.0f));
  }

  std::uint32_t drawGltfScene(const PresentationSnapshot &snapshot) {
    glUseProgram(meshProgram.id());
    const Matrix lightViewProjection =
        tableShadowViewProjection(pbrPackage.lighting);
    bindGltfShadowReceiver(lightViewProjection, false);
    const Float3 cameraEye = sharedCameraEye(snapshot.camera);
    glUniform3f(glGetUniformLocation(meshProgram.id(), "uCameraPosition"),
                cameraEye[0], cameraEye[1], cameraEye[2]);
    glUniform1i(
        glGetUniformLocation(meshProgram.id(),
                             "uBallAreaLightSamples"),
        ballAreaLightSamples);
    const char* pointSpecularReference = std::getenv("BILLIARDGL_BALL_RECT_SPECULAR_POINT_REFERENCE");
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uBallRectSpecular"),
        pointSpecularReference && std::strcmp(pointSpecularReference, "1") == 0 ? 0 : 1);
    glUniform1i(glGetUniformLocation(meshProgram.id(),
                                    "uBallLightingDiagnosticMode"),
                ballLightingMode);
    const auto &lighting = pbrPackage.lighting;
    glUniform3f(glGetUniformLocation(meshProgram.id(), "uKeyLightCenterCm"),
                lighting.keyPositionMetres[0] * 100.0f,
                lighting.keyPositionMetres[1] * 100.0f,
                lighting.keyPositionMetres[2] * 100.0f);
    glUniform2f(glGetUniformLocation(meshProgram.id(), "uKeyLightHalfSizeCm"),
                lighting.keyWidthMetres * 50.0f,
                lighting.keyDepthMetres * 50.0f);
    glUniform3f(
        glGetUniformLocation(meshProgram.id(), "uKeyLightEmissionNormal"),
        lighting.keyEmissionNormal[0], lighting.keyEmissionNormal[1],
        lighting.keyEmissionNormal[2]);
    bindEnvironment(meshProgram);
    // Prime every 2D sampler with a complete production texture before a
    // venue material without maps takes the plain-material branch.
    bindMaterial(meshProgram, "rubber");
    const Matrix vp = buildSharedCameraProjection(
                          snapshot.camera, initialization.framebufferWidth,
                          initialization.framebufferHeight)
                          .viewProjection;
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    std::uint32_t draws = 0;
    std::uint32_t venueDraws = 0, tableDraws = 0, contactDraws = 0, ballDraws = 0, cueDraws = 0;
    lastSceneTriangles = 0;
    const bool tacticalOverhead =
        std::abs(snapshot.camera.eye.x - snapshot.camera.target.x) < 80.0f &&
        std::abs(snapshot.camera.eye.z - snapshot.camera.target.z) < 80.0f &&
        snapshot.camera.eye.y > 300.0f;
    if (venueSceneLoaded && !options.pocketObjectIdPass)
      for (const auto &item : venueDrawList.items) {
        if (item.meshIndex < 0 ||
            static_cast<std::size_t>(item.meshIndex) >= gpuVenue.meshes.size())
          continue;
        const std::string &node =
            cpuVenue.nodes[static_cast<std::size_t>(item.nodeIndex)].name;
        if (tacticalOverhead && (node.find("ceiling") != std::string::npos ||
                                 node.find("lamp") != std::string::npos))
          continue;
        const bool localReceiver=sparsePropContractReady && !options.objectClassMaskPass &&
          node.find("NODE_venue_floor_LOD")==0;
        if(localReceiver) {
          std::swap(meshProgram,sparsePropReceiverProgram);
          glUseProgram(meshProgram.id());
          bindEnvironment(meshProgram);
          glUniform3f(glGetUniformLocation(meshProgram.id(),"uCameraPosition"),cameraEye[0],cameraEye[1],cameraEye[2]);
          glUniform3f(glGetUniformLocation(meshProgram.id(),"uKeyLightCenterCm"),lighting.keyPositionMetres[0]*100,
                       lighting.keyPositionMetres[1]*100,lighting.keyPositionMetres[2]*100);
          glUniformMatrix4fv(glGetUniformLocation(meshProgram.id(),"uSparsePropViewProjection"),1,GL_FALSE,
                              sparsePropProjection.viewProjection.data());
          glUniform1i(glGetUniformLocation(meshProgram.id(),"uSparsePropActive"),sparsePropProjection.active?1:0);
          glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,sparsePropShadowDepth.id());
          glBindSampler(7,0);
          glUniform1i(glGetUniformLocation(meshProgram.id(),"uSparsePropDepth"),7);
        }
        bindGltfShadowReceiver(lightViewProjection, false);
        bindGltfMaterial(importedVenue, item.materialIndex);
        if (options.objectClassMaskPass) bindObjectClassMask(3);
        meshTransform(vp, item.worldTransformCm);
        glBindVertexArray(
            gpuVenue.meshes[static_cast<std::size_t>(item.meshIndex)]
                .vertexArray.id());
        glDrawElements(
            GL_TRIANGLES, static_cast<GLsizei>(item.indexCount),
            GL_UNSIGNED_INT,
            reinterpret_cast<const void *>(static_cast<std::uintptr_t>(
                item.firstIndex * sizeof(std::uint32_t))));
        ++draws;
        ++venueDraws;
        lastSceneTriangles += item.indexCount / 3;
        if(localReceiver) {
          std::swap(meshProgram,sparsePropReceiverProgram);
          glUseProgram(meshProgram.id());
        }
      }
    const Matrix dynamicCue = cueModel(snapshot.cue);
    for (const auto &item : sceneDrawList.items) {
      if (item.meshIndex < 0 ||
          static_cast<std::size_t>(item.meshIndex) >= gpuScene.meshes.size())
        continue;
      const std::string &node =
          cpuScene.nodes[static_cast<std::size_t>(item.nodeIndex)].name;
      if (node.find("ball_shared") != std::string::npos)
        continue;
      if (tacticalOverhead && node.find("lamp_") != std::string::npos)
        continue;
      if (venueSceneLoaded && node.find("background_") != std::string::npos)
        continue;
      const bool cueItem = node.find("cue_") != std::string::npos;
      if (cueItem)
        continue;
      bindGltfShadowReceiver(lightViewProjection,
                             gltfNodeReceivesTableShadow(node));
      if (options.pocketObjectIdPass)
        bindPocketObjectId(node);
      else {
        bindGltfMaterial(importedScene, item.materialIndex);
        if (options.enablePocketIndirectBounce &&
            node.find("pocket_inner_assembly_LOD") != std::string::npos) {
          glUniform3fv(glGetUniformLocation(meshProgram.id(), "uPocketMouthsCm[0]"),
                       6, pocketMouthsCm.data());
          glUniform1i(glGetUniformLocation(meshProgram.id(), "uPocketIndirectEnabled"), 1);
        }
        if (node.find("pocket_liner_sidewall") != std::string::npos)
          glUniform1i(glGetUniformLocation(meshProgram.id(), "uPocketSidewall"),
                      1);
      }
      if (options.objectClassMaskPass)
        bindObjectClassMask(node.find("NODE_table_felt_LOD") == 0 ? 1 : 0);
      meshTransform(vp, item.worldTransformCm);
      glBindVertexArray(
          gpuScene.meshes[static_cast<std::size_t>(item.meshIndex)]
              .vertexArray.id());
      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.indexCount),
                     GL_UNSIGNED_INT,
                     reinterpret_cast<const void *>(static_cast<std::uintptr_t>(
                         item.firstIndex * sizeof(std::uint32_t))));
      ++draws;
      ++tableDraws;
      lastSceneTriangles += item.indexCount / 3;
    }
    if (options.pocketObjectIdPass) {
      glDisable(GL_DEPTH_TEST);
      return draws;
    }
    if (!options.objectClassMaskPass) {
    // Screen-space contact patches are projected from the same camera and
    // authoritative ball transforms as the sphere instances. They ground
    // the balls on cloth while the full shadow-map mesh pass is phased in.
    glUseProgram(sceneProgram.id());
    glBindVertexArray(quadVao.id());
    bindMaterial(sceneProgram, "rubber");
    bindBallAtlas(sceneProgram);
    plainMaterial(sceneProgram);
    glUniform1i(glGetUniformLocation(sceneProgram.id(), "uReceiveShadow"), 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    for (const auto &ball : snapshot.balls)
      if (ball.visible && ball.castsShadow &&
          gltfBallEligibleForClothContactCompensation(ball)) {
        const auto center =
            projectWorld(snapshot, ball.position.x, 86.55f, ball.position.z);
        const auto edge = projectWorld(snapshot, ball.position.x + 2.8575f,
                                       86.55f, ball.position.z);
        const float radius = std::max(std::abs(edge[0] - center[0]), 0.004f);
        drawQuad(sceneProgram, center[0] + radius * 0.18f,
                 center[1] - radius * 0.20f, radius * 1.12f, radius * 0.48f, 0,
                 0, {{0.002f, 0.003f, 0.003f}}, 2, 0, 1.0f);
        ++draws;
        ++contactDraws;
      }
    glDisable(GL_BLEND);
    }
    glUseProgram(meshProgram.id());
    bindGltfShadowReceiver(lightViewProjection, true);
    bindEnvironment(meshProgram);
    bindMaterial(meshProgram, "rubber");
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uObjectIdMode"), 0);
    glUniform1i(glGetUniformLocation(meshProgram.id(), "uPocketSidewall"), 0);
    glUniform2f(glGetUniformLocation(meshProgram.id(), "uUvRepeat"), 1.0f,
                1.0f);
    glUniform1i(glGetUniformLocation(meshProgram.id(),
                                    "uBallLightingDiagnosticMode"),
                ballLightingMode);
    glEnable(GL_DEPTH_TEST);
    if (ballMeshIndex >= 0 &&
        static_cast<std::size_t>(ballMeshIndex) < gpuScene.meshes.size()) {
      const auto &cpuMesh =
          cpuScene.meshes[static_cast<std::size_t>(ballMeshIndex)];
      const auto &gpuMesh =
          gpuScene.meshes[static_cast<std::size_t>(ballMeshIndex)];
      glBindVertexArray(gpuMesh.vertexArray.id());
      for (const auto &ball : snapshot.balls)
        if (ball.visible) {
          bindBallAtlas(meshProgram);
          glUniform1i(glGetUniformLocation(meshProgram.id(), "uIdentity"),
                      ball.identity);
          glUniform4f(glGetUniformLocation(meshProgram.id(), "uColor"), 1, 1, 1,
                      1);
          if (options.objectClassMaskPass) bindObjectClassMask(ball.identity == 0 ? 2 : 4);
          meshTransform(vp, ballModel(ball));
          for (const auto &primitive : cpuMesh.primitives) {
            glDrawElements(
                GL_TRIANGLES, static_cast<GLsizei>(primitive.indexCount),
                GL_UNSIGNED_INT,
                reinterpret_cast<const void *>(static_cast<std::uintptr_t>(
                    primitive.firstIndex * sizeof(std::uint32_t))));
            ++draws;
            ++ballDraws;
            lastSceneTriangles += primitive.indexCount / 3;
          }
        }
    }
    // Draw the authored cue after the ball instances so depth testing keeps
    // the tip behind the cue-ball silhouette while the exposed shaft stays
    // readable. Depth writes remain disabled to avoid contaminating later
    // transparent overlays.
    if (snapshot.cue.visible)
      for (const auto &item : sceneDrawList.items) {
        if (item.meshIndex < 0 ||
            static_cast<std::size_t>(item.meshIndex) >= gpuScene.meshes.size())
          continue;
        const std::string &node =
            cpuScene.nodes[static_cast<std::size_t>(item.nodeIndex)].name;
        if (node.find("cue_") == std::string::npos)
          continue;
        bindGltfMaterial(importedScene, item.materialIndex);
        if (options.objectClassMaskPass) bindObjectClassMask(0);
        bindGltfShadowReceiver(lightViewProjection, true);
        meshTransform(vp, dynamicCue);
        glBindVertexArray(
            gpuScene.meshes[static_cast<std::size_t>(item.meshIndex)]
                .vertexArray.id());
        glDepthMask(GL_FALSE);
        glDrawElements(
            GL_TRIANGLES, static_cast<GLsizei>(item.indexCount),
            GL_UNSIGNED_INT,
            reinterpret_cast<const void *>(static_cast<std::uintptr_t>(
                item.firstIndex * sizeof(std::uint32_t))));
        glDepthMask(GL_TRUE);
        ++draws;
        ++cueDraws;
        lastSceneTriangles += item.indexCount / 3;
      }
    glDisable(GL_DEPTH_TEST);
    if (std::getenv("BILLIARDGL_DRAW_CLASSIFICATION")) {
      const std::string identity = "opaque " + std::to_string(venueDraws) + '/' +
          std::to_string(tableDraws) + '/' + std::to_string(contactDraws) + '/' +
          std::to_string(ballDraws) + '/' + std::to_string(cueDraws);
      if (drawClassificationReported.insert(identity).second)
        std::cout << "draw_classification pass=hdr_opaque venue=" << venueDraws
            << " table=" << tableDraws << " contact_patches=" << contactDraws
            << " ball_color=" << ballDraws << " cue=" << cueDraws << " total=" << draws << '\n';
    }
    return draws;
  }

  std::array<float, 2> project(const PresentationSnapshot &snapshot, float x,
                               float z) const {
    if (snapshot.hud.aimMode && snapshot.balls[0].visible) {
      const float dx = x - snapshot.balls[0].position.x,
                  dz = z - snapshot.balls[0].position.z;
      const float dirX = snapshot.cue.direction.x,
                  dirZ = snapshot.cue.direction.z;
      return {{(dx * dirZ - dz * dirX) / 52.0f,
               -0.55f + (dx * dirX + dz * dirZ) / 105.0f}};
    }
    const float panX = snapshot.camera.projectionPan.x / 100.0f;
    const float panZ = snapshot.camera.projectionPan.z / 175.0f;
    return {{x / 100.0f - panX, -z / 175.0f + panZ}};
  }

  std::array<float, 2> projectWorld(const PresentationSnapshot &snapshot,
                                    float x, float y, float z) const {
    const SharedCameraProjection projection = buildSharedCameraProjection(
        snapshot.camera, initialization.framebufferWidth,
        initialization.framebufferHeight);
    const HomogeneousPoint clip = transformWorldToClip(projection, {x, y, z});
    if (!finiteHomogeneous(clip) || clip.w <= 1e-5)
      return {{0.0f, 0.0f}};
    return {{static_cast<float>(clip.x / clip.w),
             static_cast<float>(clip.y / clip.w)}};
  }

  std::uint32_t drawGuide(const PresentationSnapshot &snapshot) {
    if (!snapshot.cue.showGuideLine)
      return 0;
    std::array<float, 2> start, end;
    if (gltfSceneLoaded) {
      const auto center = projectWorld(
          snapshot, snapshot.cue.referencePosition.x,
          snapshot.cue.referencePosition.y, snapshot.cue.referencePosition.z);
      end = projectWorld(
          snapshot, snapshot.cue.referencePosition.x + snapshot.cue.guideEnd.x,
          snapshot.cue.referencePosition.y + 0.15f,
          snapshot.cue.referencePosition.z + snapshot.cue.guideEnd.z);
      const auto forward =
          normalize3({{snapshot.camera.target.x - snapshot.camera.eye.x,
                       snapshot.camera.target.y - snapshot.camera.eye.y,
                       snapshot.camera.target.z - snapshot.camera.eye.z}});
      const std::array<float, 3> worldUp{{0.0f, 1.0f, 0.0f}};
      const auto stableUp = std::abs(dot3(forward, worldUp)) > 0.98f
                                ? std::array<float, 3>{{0.0f, 0.0f, 1.0f}}
                                : worldUp;
      const auto cameraRight = normalize3(cross3(forward, stableUp));
      const auto cameraUp = normalize3(cross3(cameraRight, forward));
      const auto rightPoint = projectWorld(
          snapshot, snapshot.cue.referencePosition.x + cameraRight[0] * 2.8575f,
          snapshot.cue.referencePosition.y + cameraRight[1] * 2.8575f,
          snapshot.cue.referencePosition.z + cameraRight[2] * 2.8575f);
      const auto upPoint = projectWorld(
          snapshot, snapshot.cue.referencePosition.x + cameraUp[0] * 2.8575f,
          snapshot.cue.referencePosition.y + cameraUp[1] * 2.8575f,
          snapshot.cue.referencePosition.z + cameraUp[2] * 2.8575f);
      const auto projectedDistance = [&](const std::array<float, 2> &point) {
        const float x = point[0] - center[0], y = point[1] - center[1];
        return std::sqrt(x * x + y * y);
      };
      const float radius = std::max(
          {projectedDistance(rightPoint), projectedDistance(upPoint), 0.004f});
      const float dx = end[0] - center[0], dy = end[1] - center[1];
      const float axisLength = std::max(std::sqrt(dx * dx + dy * dy), 0.0001f);
      start = {{center[0] + dx / axisLength * (radius * 1.08f + 0.012f),
                center[1] + dy / axisLength * (radius * 1.08f + 0.012f)}};
    } else {
      start = {{0.0f, -0.46f}};
      end = {{0.0f, 0.70f}};
    }
    const float dx = end[0] - start[0], dy = end[1] - start[1];
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.0001f)
      return 0;
    drawQuad(sceneProgram, (start[0] + end[0]) * 0.5f,
             (start[1] + end[1]) * 0.5f, 0.004f, length * 0.5f,
             std::atan2(-dx, dy), 0, {{0.35f, 0.85f, 0.9f}}, 0, 0, 0.7f);
    return 1;
  }

  void drawQuad(const Program &program, float cx, float cy, float hx, float hy,
                float angle, float depth, const std::array<float, 3> &color,
                int shape, int identity = 0, float alpha = 1.0f) {
    uniforms(program, cx, cy, hx, hy, angle, depth, color, shape, identity,
             alpha);
    if (options.diagnosticPassTrace && renderedFrameSerial == 1 &&
        program.id() == sceneProgram.id() && depth == 0.9f) {
      GLint previousActive = 0;
      glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActive);
      for (GLuint unit = 0; unit < 7; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        GLint texture = 0, sampler = 0, width = 0, filter = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
        if (texture != 0) {
          glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
          glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
        }
        if (sampler != 0)
          glGetSamplerParameteriv(sampler, GL_TEXTURE_MIN_FILTER, &filter);
        std::cerr << "scene-texture-unit: " << unit << " texture=" << texture
                  << " sampler=" << sampler << " width=" << width
                  << " min_filter=" << filter << '\n';
      }
      glActiveTexture(previousActive);
    }
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  std::uint32_t drawScene(const PresentationSnapshot &snapshot,
                          bool depthOnly) {
    if (gltfSceneLoaded) {
      if (depthOnly)
        return drawGltfShadowScene(snapshot);
      return drawGltfScene(snapshot);
    }
    const Program &program = depthOnly ? depthProgram : sceneProgram;
    glUseProgram(program.id());
    glBindVertexArray(quadVao.id());
    if (!depthOnly) {
      bindEnvironment(program);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, shadowDepth.id());
      glUniform1i(glGetUniformLocation(program.id(), "uShadowMap"), 1);
      glUniform2f(glGetUniformLocation(program.id(), "uViewport"),
                  static_cast<float>(initialization.framebufferWidth),
                  static_cast<float>(initialization.framebufferHeight));
      glUniform1i(glGetUniformLocation(program.id(), "uReceiveShadow"), 0);
    }
    std::uint32_t draws = 0;
    if (depthOnly) {
      for (const auto &ball : snapshot.balls)
        if (ball.visible && ball.castsShadow) {
          const auto p = project(snapshot, ball.position.x, ball.position.z);
          const float radius = snapshot.hud.aimMode ? 0.065f : 0.026f;
          drawQuad(program, p[0] + 0.012f, p[1] - 0.016f, radius, radius, 0, 0,
                   {{0, 0, 0}}, 1);
          ++draws;
        }
      return draws;
    }
    plainMaterial(program);
    drawQuad(program, 0, 0, 1, 1, 0, 0.9f, {{0.004f, 0.006f, 0.010f}}, 0);
    ++draws;
    bindMaterial(program, "walnut");
    const std::array<float, 3> walnutTint{{1.0f, 0.52f, 0.28f}};
    drawQuad(program, 0, 0, 0.78f, 0.88f, 0, 0.4f, walnutTint, 0);
    ++draws;
    bindMaterial(program, "felt");
    glUniform1i(glGetUniformLocation(program.id(), "uReceiveShadow"), 1);
    drawQuad(program, 0, 0, 0.69f, 0.80f, 0, 0.3f, {{0.35f, 0.85f, 1.0f}}, 0);
    ++draws;
    glUniform1i(glGetUniformLocation(program.id(), "uReceiveShadow"), 0);
    bindMaterial(program, "rubber");
    drawQuad(program, 0, 0.805f, 0.69f, 0.010f, 0, 0.25f, {{1.0f, 1.0f, 1.0f}},
             0);
    ++draws;
    drawQuad(program, 0, -0.805f, 0.69f, 0.010f, 0, 0.25f, {{1.0f, 1.0f, 1.0f}},
             0);
    ++draws;
    drawQuad(program, -0.695f, 0, 0.010f, 0.79f, 0, 0.25f, {{1.0f, 1.0f, 1.0f}},
             0);
    ++draws;
    drawQuad(program, 0.695f, 0, 0.010f, 0.79f, 0, 0.25f, {{1.0f, 1.0f, 1.0f}},
             0);
    ++draws;
    bindMaterial(program, "gunmetal");
    drawQuad(program, 0, 0.872f, 0.72f, 0.006f, 0, 0.22f, {{1.0f, 1.0f, 1.0f}},
             0);
    ++draws;
    drawQuad(program, 0, -0.872f, 0.72f, 0.006f, 0, 0.22f, {{1.0f, 1.0f, 1.0f}},
             0);
    ++draws;
    drawQuad(program, -0.765f, 0, 0.006f, 0.84f, 0, 0.22f, {{1.0f, 1.0f, 1.0f}},
             0);
    ++draws;
    drawQuad(program, 0.765f, 0, 0.006f, 0.84f, 0, 0.22f, {{1.0f, 1.0f, 1.0f}},
             0);
    ++draws;
    bindMaterial(program, "walnut");
    drawQuad(program, 0, 0.84f, 0.78f, 0.045f, 0, 0.2f, walnutTint, 0);
    ++draws;
    drawQuad(program, 0, -0.84f, 0.78f, 0.045f, 0, 0.2f, walnutTint, 0);
    ++draws;
    drawQuad(program, -0.73f, 0, 0.045f, 0.84f, 0, 0.2f, walnutTint, 0);
    ++draws;
    drawQuad(program, 0.73f, 0, 0.045f, 0.84f, 0, 0.2f, walnutTint, 0);
    ++draws;
    const std::array<std::array<float, 2>, 6> pockets{{{{-0.69f, -0.80f}},
                                                       {{0.69f, -0.80f}},
                                                       {{-0.69f, 0.80f}},
                                                       {{0.69f, 0.80f}},
                                                       {{-0.69f, 0.0f}},
                                                       {{0.69f, 0.0f}}}};
    bindMaterial(program, "leather");
    for (const auto &pocket : pockets) {
      drawQuad(program, pocket[0], pocket[1], 0.040f, 0.040f, 0, 0.1f,
               {{1.0f, 1.0f, 1.0f}}, 1);
      ++draws;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto &ball : snapshot.balls)
      if (ball.visible) {
        const auto p = project(snapshot, ball.position.x, ball.position.z);
        const float radius = snapshot.hud.aimMode ? 0.065f : 0.026f;
        if (ball.castsShadow) {
          plainMaterial(program);
          drawQuad(program, p[0] + 0.010f, p[1] - 0.014f, radius * 1.18f,
                   radius * 0.78f, 0, 0.10f, {{0.003f, 0.004f, 0.004f}}, 2);
          ++draws;
        }
        bindBallAtlas(program);
        drawQuad(program, p[0], p[1], radius, radius,
                 ball.rotationAngleDegrees * 0.0174533f, 0.0f,
                 {{1.0f, 1.0f, 1.0f}}, 1, ball.identity);
        ++draws;
      }
    glDisable(GL_BLEND);
    if (snapshot.cue.visible) {
      bindMaterial(program, "cue");
      const auto p = project(snapshot, snapshot.cue.referencePosition.x,
                             snapshot.cue.referencePosition.z);
      const auto towardTarget = project(
          snapshot,
          snapshot.cue.referencePosition.x + snapshot.cue.direction.x * 10.0f,
          snapshot.cue.referencePosition.z + snapshot.cue.direction.z * 10.0f);
      const float deltaX = towardTarget[0] - p[0],
                  deltaY = towardTarget[1] - p[1];
      const float length =
          std::max(std::sqrt(deltaX * deltaX + deltaY * deltaY), 0.0001f);
      const float screenDirX = deltaX / length, screenDirY = deltaY / length;
      const float angle = std::atan2(-screenDirX, screenDirY);
      const float cueOffset = snapshot.hud.aimMode ? 0.10f : 0.20f;
      const float cueLength = snapshot.hud.aimMode ? 0.16f : 0.30f;
      drawQuad(program, p[0] - screenDirX * cueOffset,
               p[1] - screenDirY * cueOffset, 0.012f, cueLength, angle, 0.0f,
               {{1.0f, 1.0f, 1.0f}}, 0);
      ++draws;
    }
    plainMaterial(program);
    const auto lamp = lookdevResult.settings.lampEmissive;
    const float lampWidth = pbrTextures.report().loadedPackage
                                ? pbrPackage.lighting.keyWidthMetres
                                : lookdevResult.settings.lampWidthMetres;
    const float lampDepth = pbrTextures.report().loadedPackage
                                ? pbrPackage.lighting.keyDepthMetres
                                : lookdevResult.settings.lampDepthMetres;
    drawQuad(program, 0, 0.93f, lampWidth / 5.8f, lampDepth / 36.0f, 0, -0.1f,
             {{lamp[0] * 1.4f, lamp[1] * 1.4f, lamp[2] * 1.4f}}, 0);
    ++draws;
    return draws;
  }
};

ModernRenderer::ModernRenderer(ModernRendererOptions options)
    : impl_(new Impl(std::move(options))) {}
ModernRenderer::~ModernRenderer() = default;

bool ModernRenderer::initialize(const RendererInitialization &initialization,
                                std::string &error) {
  try {
    if (initialization.framebufferWidth <= 0 ||
        initialization.framebufferHeight <= 0)
      throw std::runtime_error("invalid framebuffer dimensions");
    if (impl_->options.msaaSamples != 0 && impl_->options.msaaSamples != 2 &&
        impl_->options.msaaSamples != 4)
      throw std::runtime_error(
          "ModernRenderer supports MSAA sample counts 0, 2 or 4");
    impl_->initialization = initialization;
    const std::optional<int> ballAreaLightSampleOverride =
        ballAreaLightDiagnosticSampleOverride();
    impl_->ballAreaLightSamples = 0;
    impl_->ballLightingMode = ballLightingDiagnosticMode();
    impl_->ballShadowBiasScale = effectiveBallShadowReceiverBiasScale(
        ballShadowBiasScaleDiagnosticRollback());
    impl_->gltfContactShadowScale = gltfContactShadowCompensationScale();
    impl_->seamlessCubemapEnabled = seamlessCubemapEnabled();
    impl_->applySeamlessCubemapState(impl_->seamlessCubemapEnabled);
    impl_->lookdevResult =
        loadLookdevSettings(impl_->options.lookdevManifestPath);
    const bool anyPbrPath = !impl_->options.pbrParametersPath.empty() ||
                            !impl_->options.pbrAssetManifestPath.empty() ||
                            !impl_->options.lightingRigPath.empty();
    if (anyPbrPath) {
      if (impl_->options.pbrParametersPath.empty() ||
          impl_->options.pbrAssetManifestPath.empty() ||
          impl_->options.lightingRigPath.empty())
        throw std::runtime_error("PBR initialization requires parameter, asset "
                                 "and lighting manifests together");
      impl_->pbrPackage = loadPbrPackageDescription(
          impl_->options.pbrParametersPath, impl_->options.pbrAssetManifestPath,
          impl_->options.lightingRigPath);
      impl_->ballAreaLightSamples = effectiveBallAreaLightSamples(
          impl_->options.quality, ballAreaLightSampleOverride);
      const int environmentResolution =
          impl_->options.quality == RenderQuality::Low ? 64 : 128;
      impl_->pbrTextures.load(impl_->pbrPackage, environmentResolution);
    } else if (ballAreaLightSampleOverride.has_value()
               && *ballAreaLightSampleOverride > 0) {
      throw std::runtime_error(
          "ball area-light diagnostic requires an authored PBR lighting rig");
    }
    impl_->sceneProgram = linkProgram(
        compileShader(ShaderStage::Vertex, kSceneVertex, "modern_scene.vert"),
        compileShader(ShaderStage::Fragment, impl_->options.objectClassMaskPass ? kObjectClassOverlayFragment : kSceneFragment,
                      "modern_scene.frag"),
        "modern_scene");
    impl_->depthProgram = linkProgram(
        compileShader(ShaderStage::Vertex, kSceneVertex, "shadow_depth.vert"),
        compileShader(ShaderStage::Fragment, kDepthFragment,
                      "shadow_depth.frag"),
        "shadow_depth");
    impl_->toneProgram = linkProgram(
        compileShader(ShaderStage::Vertex, kToneVertex, "tone_map.vert"),
        compileShader(ShaderStage::Fragment, impl_->options.objectClassMaskPass ? kObjectClassToneFragment : kToneFragment, "tone_map.frag"),
        "tone_map_aces");
    impl_->meshProgram = linkProgram(
        compileShader(ShaderStage::Vertex, kMeshVertex, "modern_gltf.vert"),
        compileShader(ShaderStage::Fragment, impl_->options.objectClassMaskPass ? kObjectClassMeshFragment : kMeshFragment, "modern_gltf.frag"),
        "modern_gltf_pbr");
    impl_->meshDepthProgram = linkProgram(
        compileShader(ShaderStage::Vertex, kMeshDepthVertex,
                      "modern_gltf_shadow.vert"),
        compileShader(ShaderStage::Fragment, kMeshDepthFragment,
                      "modern_gltf_shadow.frag"),
        "modern_gltf_shadow");
    impl_->trajectoryOverlayProgram = linkProgram(
        compileShader(ShaderStage::Vertex, kTrajectoryOverlayVertex,
                      "trajectory_overlay.vert"),
        compileShader(ShaderStage::Fragment, kTrajectoryOverlayFragment,
                      "trajectory_overlay.frag"),
        "trajectory_overlay");
    if (impl_->options.enableFinalOwnershipInstrumentation) {
      impl_->finalOwnershipResolveSingleProgram =
          linkProgram(compileShader(ShaderStage::Vertex, kToneVertex,
                                    "final_ownership_resolve_single.vert"),
                      compileShader(ShaderStage::Fragment,
                                    kFinalOwnershipResolveSingleFragment,
                                    "final_ownership_resolve_single.frag"),
                      "integrated_final_ownership_resolve_single");
      impl_->finalOwnershipResolveMsaaProgram =
          linkProgram(compileShader(ShaderStage::Vertex, kToneVertex,
                                    "final_ownership_resolve_msaa.vert"),
                      compileShader(ShaderStage::Fragment,
                                    kFinalOwnershipResolveMsaaFragment,
                                    "final_ownership_resolve_msaa.frag"),
                      "integrated_final_ownership_resolve_msaa");
    }
    if (!impl_->options.venueScenePath.empty()) {
      auto imported = gltf::importAsset(impl_->options.venueScenePath);
      if (!imported.ok)
        throw std::runtime_error("ModernRenderer venue glTF: " +
                                 imported.error);
      impl_->importedVenue.asset = std::move(imported.asset);
      for (std::size_t index = 0;
           index < impl_->importedVenue.asset.nodes.size(); ++index) {
        const auto &name = impl_->importedVenue.asset.nodes[index].name;
        for (int lod = 0; lod < 3; ++lod)
          if (name == "NODE_venue_LOD" + std::to_string(lod))
            impl_->importedVenue.lodRoots.push_back(
                {"venue", lod, static_cast<int>(index), name, 0});
      }
      if (impl_->importedVenue.lodRoots.size() != 3)
        throw std::runtime_error(
            "ModernRenderer venue glTF lacks complete LOD roots");
      auto cpu = gltf::buildCpuDrawPackage(impl_->importedVenue);
      if (!cpu.ok)
        throw std::runtime_error("ModernRenderer venue draw package: " +
                                 cpu.error);
      impl_->cpuVenue = std::move(cpu.package);
      auto gpu = gltf::uploadDrawPackage(impl_->cpuVenue);
      if (!gpu.ok)
        throw std::runtime_error("ModernRenderer venue GPU upload: " +
                                 gpu.error);
      impl_->gpuVenue = std::move(gpu.package);
      impl_->venueDrawList = gltf::buildDrawList(
          impl_->cpuVenue, {{"venue", 1000}}, gltf::LodPolicy{}, nullptr);
      impl_->venueSceneLoaded = true;
    }
    if (impl_->options.enableSparsePropShadows) {
      if (!impl_->venueSceneLoaded || !anyPbrPath)
        throw std::runtime_error("sparse_shadow_requires_venue_and_authored_key");
      if(impl_->pbrPackage.lighting.environmentContract!=EnvironmentRadianceContract::NormalizedTintLuminanceV2 ||
          impl_->pbrPackage.lighting.keyFixtureOwnership!=KeyFixtureOwnership::DirectRectAreaOnly)
        throw std::runtime_error("sparse_shadow_requires_single_owned_v2_key");
      std::ifstream input(impl_->options.venueScenePath);
      std::ostringstream text; text << input.rdbuf();
      const auto document=json::parse(text.str());
      if(!document.ok || document.value.at("asset").at("extras").at("sparse_furniture_contract").asString()!=
          "root_approved_sparse_furniture_v1" ||
          !document.value.at("asset").at("extras").at("remote_furniture_in_scope").asBool())
        throw std::runtime_error("sparse_shadow_contract_missing");
      for(int lod=0;lod<3;++lod) {
        const std::string caster="NODE_original_bench_and_shelf_LOD"+std::to_string(lod);
        const std::string floor="NODE_venue_floor_LOD"+std::to_string(lod);
        int casters=0,receivers=0;
        for(const auto& node:impl_->cpuVenue.nodes) {
          if(node.name!=caster && node.name!=floor)continue;
          if(node.meshIndex<0)throw std::runtime_error("sparse_shadow_geometry_missing");
          const auto& mesh=impl_->cpuVenue.meshes.at(static_cast<std::size_t>(node.meshIndex));
          if(mesh.primitives.size()!=1 || mesh.indices.empty())
            throw std::runtime_error("sparse_shadow_requires_single_primitive");
          if(node.name==caster)++casters;
          else {
            ++receivers;
            const int material=mesh.primitives.front().materialIndex;
            if(material<0 || impl_->importedVenue.asset.materials.at(static_cast<std::size_t>(material)).name!=
                "MAT_venue_matte_ground")throw std::runtime_error("sparse_shadow_floor_material_invalid");
          }
        }
        if(casters!=1 || receivers!=1)throw std::runtime_error("sparse_shadow_lod_binding_invalid");
      }
      impl_->sparsePropContractReady=true;
      if(!impl_->options.objectClassMaskPass) {
        const auto fragment=sparsePropReceiverFragment();
        impl_->sparsePropReceiverProgram=linkProgram(
            compileShader(ShaderStage::Vertex,kMeshVertex,"sparse_prop_receiver.vert"),
            compileShader(ShaderStage::Fragment,fragment.c_str(),"sparse_prop_receiver.frag"),
            "sparse_prop_receiver");
      }
    }
    if (!impl_->options.gltfScenePath.empty()) {
      const auto imported = gltf::importAssetPackage(
          impl_->options.gltfScenePath, impl_->options.pbrAssetManifestPath,
          impl_->options.pbrParametersPath);
      if (!imported.ok)
        throw std::runtime_error("ModernRenderer glTF package: " +
                                 imported.error);
      impl_->importedScene = imported.package;
      if (impl_->options.enablePocketIndirectBounce) {
        for (int pocket=0;pocket<6;++pocket) {
          const std::string name="SOCKET_pocket_"+std::to_string(pocket)+"_mouth";
          int found=0;
          for (const auto& socket:impl_->importedScene.asset.sockets) {
            if(socket.name!=name) continue;
            const auto p=socket.positionCm;
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
              throw std::runtime_error("Pocket indirect light has invalid mouth socket");
            impl_->pocketMouthsCm[3*pocket]=p.x;
            impl_->pocketMouthsCm[3*pocket+1]=p.y;
            impl_->pocketMouthsCm[3*pocket+2]=p.z;
            ++found;
          }
          if(found!=1) throw std::runtime_error("Pocket indirect light needs six unique mouth sockets");
        }
      }
      auto cpu = gltf::buildCpuDrawPackage(impl_->importedScene);
      if (!cpu.ok)
        throw std::runtime_error("ModernRenderer glTF draw package: " +
                                 cpu.error);
      impl_->cpuScene = std::move(cpu.package);
      auto gpu = gltf::uploadDrawPackage(impl_->cpuScene);
      if (!gpu.ok)
        throw std::runtime_error("ModernRenderer glTF GPU upload: " +
                                 gpu.error);
      impl_->gpuScene = std::move(gpu.package);
      impl_->sceneDrawList = gltf::buildDrawList(
          impl_->cpuScene,
          {{"table", 1000}, {"ball", 1000}, {"cue", 1000}, {"lamp", 1000}},
          gltf::LodPolicy{}, nullptr);
      for (std::size_t index = 0; index < impl_->cpuScene.nodes.size(); ++index)
        if (impl_->cpuScene.nodes[index].name == "NODE_ball_shared_LOD0")
          impl_->ballMeshIndex = impl_->cpuScene.nodes[index].meshIndex;
      if (impl_->ballMeshIndex < 0)
        throw std::runtime_error(
            "ModernRenderer glTF package lacks LOD0 shared ball mesh");
      impl_->gltfSceneLoaded = true;
    }
    if (impl_->options.objectClassMaskPass &&
        (!impl_->gltfSceneLoaded || impl_->options.enableProductUi ||
         impl_->options.pocketObjectIdPass || impl_->options.enableFinalOwnershipInstrumentation))
      throw std::runtime_error("object-class mask requires standalone glTF diagnostic rendering");
    impl_->quadBuffer = createBuffer();
    impl_->quadVao = createVertexArray();
    constexpr std::array<float, 24> quad = {{-1, -1, 0, 0, 1,  -1, 1, 0,
                                             1,  1,  1, 1, -1, -1, 0, 0,
                                             1,  1,  1, 1, -1, 1,  0, 1}};
    glBindVertexArray(impl_->quadVao.id());
    glBindBuffer(GL_ARRAY_BUFFER, impl_->quadBuffer.id());
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));
    impl_->trajectoryOverlayBuffer = createBuffer();
    impl_->trajectoryOverlayVao = createVertexArray();
    glBindVertexArray(impl_->trajectoryOverlayVao.id());
    glBindBuffer(GL_ARRAY_BUFFER, impl_->trajectoryOverlayBuffer.id());
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(training::TrajectoryOverlayVertex),
                          reinterpret_cast<void *>(offsetof(
                              training::TrajectoryOverlayVertex, xPhysicalPx)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          sizeof(training::TrajectoryOverlayVertex),
                          reinterpret_cast<void *>(offsetof(
                              training::TrajectoryOverlayVertex, linearRgba)));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(
        2, 1, GL_UNSIGNED_INT, sizeof(training::TrajectoryOverlayVertex),
        reinterpret_cast<void *>(
            offsetof(training::TrajectoryOverlayVertex, finalOwnershipToken)));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(
        3, 1, GL_UNSIGNED_INT, sizeof(training::TrajectoryOverlayVertex),
        reinterpret_cast<void *>(
            offsetof(training::TrajectoryOverlayVertex, visualSublayer)));
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(
        4, 1, GL_UNSIGNED_INT, sizeof(training::TrajectoryOverlayVertex),
        reinterpret_cast<void *>(offsetof(training::TrajectoryOverlayVertex,
                                          adaptiveInnerHaloWhite)));
    impl_->makeTargets(initialization.framebufferWidth,
                       initialization.framebufferHeight);
    checkGl("initialize render targets");
    impl_->primePbrTextureUnits();
    checkGl("initialize PBR texture units");
    if (impl_->options.enableProductUi) {
      if (impl_->options.productUiSourceRoot.empty())
        throw std::runtime_error(
            "modern_renderer_product_ui_source_root_empty");
      if (impl_->options.productUiFontPath.empty())
        throw std::runtime_error("modern_renderer_product_ui_font_path_empty");
      impl_->productUiDispatch =
          std::make_unique<ui::Core41ProductUiGlDispatch>();
      impl_->productUiRuntime =
          std::make_unique<ModernProductUiRuntime>(*impl_->productUiDispatch);
      impl_->productUiPass = std::make_unique<ModernRendererProductUiPass>(
          *impl_->productUiRuntime);
      auto configured = ModernProductUiRuntimeConfig::trustedDefaults(
          impl_->options.productUiSourceRoot, impl_->options.productUiFontPath,
          impl_->options.productUiFontFaceIndex);
#if defined(__APPLE__)
      configured.insetChromeFont = configured.font;
      configured.insetChromeFont->fontPath =
          "/System/Library/Fonts/Supplemental/Arial Unicode.ttf";
      configured.insetChromeFont->faceIndex = 0;
#endif
      configured.rulesDecisionUiImplementationCapability =
          impl_->options.productUiRulesDecisionImplementationCapability;
      configured.rulesDecisionIllegalBreakProductCapability =
          impl_->options.productUiRulesDecisionIllegalBreakCapability;
      configured.retainSubmittedTextEvidence =
          impl_->options.productUiRetainSubmittedTextEvidence;
      if (configured.rulesDecisionUiImplementationCapability ||
          configured.rulesDecisionIllegalBreakProductCapability) {
        configured.font.atlasWidth = 4096;
        configured.font.atlasHeight = 4096;
      }
      const ui::UiGpuResult productInitialized =
          impl_->productUiPass->initialize(configured);
      if (!productInitialized.ok)
        throw std::runtime_error(productInitialized.errorCode);
      checkGl("initialize product UI");
    }
    impl_->initialized = true;
    return true;
  } catch (const std::exception &failure) {
    if (impl_->productUiPass)
      impl_->productUiPass->shutdown();
    impl_->productUiPass.reset();
    impl_->productUiRuntime.reset();
    impl_->productUiDispatch.reset();
    impl_->restoreSeamlessCubemapState();
    error = failure.what();
    return false;
  }
}

void ModernRenderer::resize(int width, int height) {
  if (!impl_->initialized)
    throw std::runtime_error("ModernRenderer resize before initialize");
  if (width <= 0 || height <= 0)
    throw std::runtime_error(
        "ModernRenderer resize requires positive dimensions");
  invalidateTrajectoryInsetVisibleFinalStaging();
  invalidatePreparedTrajectoryInsetAtomicFrame();
  impl_->initialization.framebufferWidth = width;
  impl_->initialization.framebufferHeight = height;
  impl_->makeTargets(width, height);
  checkGl("resize render targets");
  impl_->primePbrTextureUnits();
  checkGl("resize PBR texture units");
}

void ModernRenderer::render(const PresentationSnapshot &snapshot) {
  renderFrame(snapshot, nullptr, nullptr);
}

void ModernRenderer::renderProductFrame(
    const PresentationSnapshot &snapshot,
    const ModernProductSessionSnapshot &session,
    const ui::ProductUiFrameEnvironment &environment) {
  renderFrame(snapshot, &session, &environment);
}

void ModernRenderer::renderProductFrameEvidenceReplay(
    const PresentationSnapshot &snapshot,
    const ModernProductSessionSnapshot &session,
    const ui::ProductUiFrameEnvironment &environment) {
  if (!impl_->options.enableFinalOwnershipInstrumentation)
    throw std::runtime_error(
        "product_frame_evidence_replay_requires_instrumentation");
  if (!impl_->finalOwnershipPreProductUiReady || !impl_->productUiPass)
    throw std::runtime_error(
        "product_frame_evidence_replay_base_not_ready");
  impl_->frameMetrics = {};
  impl_->latestProductUiResult = {};
  glBindFramebuffer(GL_READ_FRAMEBUFFER,
                    impl_->finalOwnershipPreProductUiFramebuffer.id());
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, impl_->ldrFramebuffer.id());
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0, 0, impl_->initialization.framebufferWidth,
                    impl_->initialization.framebufferHeight, 0, 0,
                    impl_->initialization.framebufferWidth,
                    impl_->initialization.framebufferHeight,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, impl_->ldrFramebuffer.id());
  glViewport(0, 0, impl_->initialization.framebufferWidth,
             impl_->initialization.framebufferHeight);
  const GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                            GL_COLOR_ATTACHMENT2};
  glDrawBuffers(3, buffers);
  const GLfloat zeroCoverage[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  glClearBufferfv(GL_COLOR, 1, zeroCoverage);
  glClearBufferfv(GL_COLOR, 2, zeroCoverage);
  const ModernProductUiRuntimeResult submitted =
      impl_->productUiPass->render(session, snapshot, environment);
  impl_->latestProductUiResult = submitted;
  if (!submitted.ok || !submitted.gpuSubmitted)
    throw std::runtime_error(
        std::string("product_frame_evidence_replay_ui_failed:") +
        (submitted.errorCode.empty() ? "no_gpu_draws" :
                                       submitted.errorCode));
  const GLenum colorOnly = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &colorOnly);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->ldrFramebuffer.id());
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, impl_->initialization.framebufferWidth,
                    impl_->initialization.framebufferHeight, 0, 0,
                    impl_->initialization.framebufferWidth,
                    impl_->initialization.framebufferHeight,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, impl_->ldrFramebuffer.id());
  impl_->frameMetrics.productPresentPerformed = true;
  if (impl_->options.debugErrors)
    checkGl("product frame evidence replay");
}

void ModernRenderer::renderFrame(
    const PresentationSnapshot &snapshot,
    const ModernProductSessionSnapshot *productSession,
    const ui::ProductUiFrameEnvironment *productEnvironment,
    bool evidenceReplay) {
  if (!impl_->initialized)
    throw std::runtime_error("ModernRenderer render before initialize");
  const bool productRequest =
      productSession != nullptr && productEnvironment != nullptr;
  // A caller may retain the product session while automation, screenshot, or
  // windowless policy suppresses the overlay.  Those frames use the exact
  // legacy/non-product five-pass path and never enter the product backend.
  const bool productFrame = productRequest && productEnvironment->productMode &&
                            !productEnvironment->automation &&
                            !productEnvironment->windowless;
  if (productFrame && !impl_->productUiPass)
    throw std::runtime_error("modern_renderer_product_ui_not_configured");
  if (!evidenceReplay) {
    // A successfully presented inset belongs to its previous immutable frame.
    // Retire it before preparing a new serial, including on resize or seek.
    // The last-attempt serial itself remains monotonic and is not reset.
    invalidateTrajectoryInsetVisibleFinalStaging();
    invalidatePreparedTrajectoryInsetAtomicFrame();
  }
  // CP4 requires a separately supplied, immutable current request. Neither
  // productFrame nor a Training/Replay screen can enable the inset alone.
  std::optional<ui::TrajectoryInsetVisibleCurrentFrame>
      trajectoryInsetVisibleRequest;
  if (productFrame && productSession->trajectoryInsetVisibleCurrentContext) {
    const auto& preparedUi = impl_->productUiRuntime->prepareFrame(
        *productSession, snapshot, *productEnvironment);
    trajectoryInsetVisibleRequest = ui::makeTrajectoryInsetVisibleCurrentFrame(
        *productSession, snapshot, *productEnvironment,
        *productSession->trajectoryInsetVisibleCurrentContext, &preparedUi);
    // CP2 suppression is a valid layout outcome, not a GPU transaction.
    // Never submit empty chrome or retain a previous inset for this frame.
    if (!trajectoryInsetVisibleRequest->layout.insetVisible ||
        !trajectoryInsetVisibleRequest->chrome.visible) {
      trajectoryInsetVisibleRequest->ok = false;
      if (trajectoryInsetVisibleRequest->errorCode.empty())
        trajectoryInsetVisibleRequest->errorCode =
            trajectoryInsetVisibleRequest->layout.suppressionReason.empty()
                ? "inset_layout_unavailable"
                : trajectoryInsetVisibleRequest->layout.suppressionReason;
    }
    const auto& context = *productSession->trajectoryInsetVisibleCurrentContext;
    const auto expectedPalette = productSession->effectiveSettings.colorVisionMode ==
            settings::ColorVisionMode::Default
        ? training::TrajectoryInsetPalette::Standard
        : training::TrajectoryInsetPalette::ColorVisionSafe;
    if (context.sourceContentSha256.empty() ||
        context.samples != std::max(1, impl_->options.msaaSamples) ||
        context.palette != expectedPalette ||
        productSession->shellViewport.physicalWidth != impl_->initialization.framebufferWidth ||
        productSession->shellViewport.physicalHeight != impl_->initialization.framebufferHeight) {
      trajectoryInsetVisibleRequest->ok = false;
      trajectoryInsetVisibleRequest->errorCode = "trajectory_inset_current_renderer_state_mismatch";
    }
  }
  if (!trajectoryInsetVisibleRequest || !trajectoryInsetVisibleRequest->ok) {
    invalidateTrajectoryInsetVisibleFinalStaging();
    invalidatePreparedTrajectoryInsetAtomicFrame();
  }
  bool trajectoryInsetAcceptedFailureBlocksProductPresent = false;
  impl_->frameMetrics = {};
  if (trajectoryInsetVisibleRequest && !trajectoryInsetVisibleRequest->ok)
    impl_->frameMetrics.trajectoryInsetVisibleErrorCode = trajectoryInsetVisibleRequest->errorCode;
  if (trajectoryInsetVisibleRequest) {
    std::ostringstream diagnostic;
    diagnostic.imbue(std::locale::classic());
    diagnostic << "panel=";
    const auto appendRect = [&](const product_hud::HudRect& rect) {
      diagnostic << '[' << rect.x << ',' << rect.y << ',' << rect.width << ',' << rect.height << ']';
    };
    appendRect(trajectoryInsetVisibleRequest->layout.panelRect);
    diagnostic << " suppression=" << trajectoryInsetVisibleRequest->layout.suppressionReason;
    for (const auto& exclusion : trajectoryInsetVisibleRequest->shellRequest.exclusions) {
      if (!exclusion.active) continue;
      diagnostic << " exclusion=" << exclusion.id << ':';
      appendRect(exclusion.rect);
      for (const auto& rect : exclusion.additionalRects) appendRect(rect);
    }
    const auto& package = trajectoryInsetVisibleRequest->package;
    if (!package.probeResolutionFailureMember.empty()) {
      diagnostic << " unresolved_member=" << package.probeResolutionFailureMember;
      const auto failed = std::find_if(package.semanticBatches.begin(),package.semanticBatches.end(),
          [&](const auto& batch){return batch.insetMemberKey==package.probeResolutionFailureMember;});
      if (failed!=package.semanticBatches.end()) {
        diagnostic << " unresolved_track=" << training::trajectoryInsetTrackName(failed->track)
            << " parent=" << failed->parentSourceKey
            << " role=" << training::trajectoryInsetRoleName(failed->role)
            << " token=" << failed->insetOwnershipToken
            << " visit_marker_fallback=" << (failed->role==training::TrajectoryInsetRole::PredictedEventVisitMarker);
      }
    }
    diagnostic << " canonical_table=";
    appendRect({package.canonicalTableRect.x, package.canonicalTableRect.y,
        package.canonicalTableRect.width, package.canonicalTableRect.height});
    for (const bool predicted : {true,false}) {
      const auto& track = predicted ? productSession->shell.trajectoryOverlay.predicted :
          productSession->shell.trajectoryOverlay.actual;
      double minX=std::numeric_limits<double>::infinity(),minZ=minX,maxX=-minX,maxZ=-minX,path=0;
      const training::OverlaySample* previous=nullptr;
      for (const auto& sample:track.samples) {
        if (!sample.visible) { previous=nullptr; continue; }
        minX=std::min(minX,static_cast<double>(sample.position.x));
        maxX=std::max(maxX,static_cast<double>(sample.position.x));
        minZ=std::min(minZ,static_cast<double>(sample.position.z));
        maxZ=std::max(maxZ,static_cast<double>(sample.position.z));
        if(previous) path+=std::hypot(sample.position.x-previous->position.x,
            sample.position.z-previous->position.z);
        previous=&sample;
      }
      diagnostic << (predicted ? " predicted" : " actual") << "_samples=" << track.samples.size();
      if(minX<=maxX) diagnostic << " world_xz_cm=[" << minX << ',' << minZ << ',' << maxX << ',' << maxZ
          << "] path_cm=" << path;
      double left=std::numeric_limits<double>::infinity(),top=left,right=-left,bottom=-left;
      for (const auto& batch:package.semanticBatches) {
        if ((batch.track==training::TrajectoryInsetTrack::Predicted)!=predicted) continue;
        if (batch.role!=training::TrajectoryInsetRole::PredictedDashCore &&
            batch.role!=training::TrajectoryInsetRole::ActualLeftCore &&
            batch.role!=training::TrajectoryInsetRole::ActualRightCore) continue;
        for (std::size_t i=batch.firstVertex;i<std::min(package.vertices.size(),batch.firstVertex+batch.vertexCount);++i) {
          const auto& vertex=package.vertices[i];
          left=std::min(left,static_cast<double>(vertex.xPhysicalPx));right=std::max(right,static_cast<double>(vertex.xPhysicalPx));
          top=std::min(top,static_cast<double>(vertex.yPhysicalPx));bottom=std::max(bottom,static_cast<double>(vertex.yPhysicalPx));
        }
      }
      if(left<=right) diagnostic << " core_bbox_px=[" << left << ',' << top << ',' << right << ',' << bottom
          << "] plot_span_fraction=[" << (right-left)/package.layout.plotRect.width << ','
          << (bottom-top)/package.layout.plotRect.height << ']';
    }
    impl_->frameMetrics.trajectoryInsetLayoutDiagnostic = diagnostic.str();
  }
  impl_->frameMetrics.depthOfFieldRadius = effectiveDepthOfFieldRadius(
      impl_->options.quality, productFrame && productSession->effectiveSettings.depthOfField,
      productFrame && productSession->shell.screen == ProductScreen::ReplayPlayback);
  impl_->frameMetrics.bloomStrength = effectiveBloomStrength(
      impl_->options.quality, productFrame ? productSession->effectiveSettings.bloom
                                          : impl_->options.enableBloom);
  impl_->frameMetrics.bloomApplied = impl_->frameMetrics.bloomStrength > 0.0f;
  if (impl_->options.objectClassMaskPass) {
    impl_->frameMetrics.depthOfFieldRadius = 0;
    impl_->frameMetrics.bloomStrength = 0;
    impl_->frameMetrics.bloomApplied = false;
  }
  impl_->frameMetrics.msaaSamples = impl_->options.msaaSamples;
  ++impl_->renderedFrameSerial;
  impl_->finalOwnershipFrameReady = false;
  impl_->finalOwnershipProductUiApplied = false;
  impl_->latestProductUiResult = {};
  const auto quality = qualitySettings(impl_->options.quality);
  impl_->frameMetrics.quality = impl_->options.quality;
  impl_->frameMetrics.shadowMapSize = quality.shadowMapSize;
  impl_->frameMetrics.pcfKernelWidth = quality.pcfKernelWidth;
  const auto &pbr = impl_->pbrTextures.report();
  impl_->frameMetrics.pbrPackageLoaded = pbr.loadedPackage;
  impl_->frameMetrics.pbrTextureCount = pbr.uploadedTextureCount;
  impl_->frameMetrics.pbrGpuBytes = pbr.uploadedGpuBytes;
  impl_->frameMetrics.environmentGpuBytes = pbr.environmentGpuBytes;
  impl_->frameMetrics.environmentResolution = pbr.environmentResolution;
  impl_->frameMetrics.anisotropySupported = pbr.anisotropySupported;
  impl_->frameMetrics.anisotropy = pbr.anisotropy;
  impl_->frameMetrics.gltfSceneLoaded = impl_->gltfSceneLoaded;
  impl_->frameMetrics.venueSceneLoaded = impl_->venueSceneLoaded;
  impl_->frameMetrics.sparsePropShadowGpuBytes=impl_->sparsePropShadowDepth.id()!=0 ?
    SparsePropShadowProjection::residentBytes : 0;
  for (const auto &ball : snapshot.balls)
    if (ball.visible) {
      ++impl_->frameMetrics.visibleBalls;
      const auto p = impl_->project(snapshot, ball.position.x, ball.position.z);
      const float radius = snapshot.hud.aimMode ? 0.065f : 0.026f;
      impl_->frameMetrics.ballNdcBounds[0] =
          std::min(impl_->frameMetrics.ballNdcBounds[0], p[0] - radius);
      impl_->frameMetrics.ballNdcBounds[1] =
          std::min(impl_->frameMetrics.ballNdcBounds[1], p[1] - radius);
      impl_->frameMetrics.ballNdcBounds[2] =
          std::max(impl_->frameMetrics.ballNdcBounds[2], p[0] + radius);
      impl_->frameMetrics.ballNdcBounds[3] =
          std::max(impl_->frameMetrics.ballNdcBounds[3], p[1] + radius);
    }
  const auto &bounds = impl_->frameMetrics.ballNdcBounds;
  impl_->frameMetrics.compositionSafe =
      impl_->frameMetrics.visibleBalls > 0 && bounds[0] > -0.90f &&
      bounds[1] > -0.86f && bounds[2] < 0.90f && bounds[3] < 0.86f;
  if (snapshot.cue.visible) {
    const auto cue = impl_->project(snapshot, snapshot.cue.referencePosition.x,
                                    snapshot.cue.referencePosition.z);
    const auto target = impl_->project(
        snapshot,
        snapshot.cue.referencePosition.x + snapshot.cue.direction.x * 10.0f,
        snapshot.cue.referencePosition.z + snapshot.cue.direction.z * 10.0f);
    const float dx = target[0] - cue[0], dy = target[1] - cue[1];
    const float length = std::max(std::sqrt(dx * dx + dy * dy), 0.0001f);
    const float offset = snapshot.hud.aimMode ? 0.10f : 0.20f;
    const float halfLength = snapshot.hud.aimMode ? 0.16f : 0.30f;
    const float farX = cue[0] - (dx / length) * (offset + halfLength);
    const float farY = cue[1] - (dy / length) * (offset + halfLength);
    impl_->frameMetrics.compositionSafe = impl_->frameMetrics.compositionSafe &&
                                          std::abs(farX) < 0.92f &&
                                          farY > -0.92f && farY < 0.86f;
  }
  impl_->frameMetrics.passes.push_back(impl_->timed("shadow_depth", [&] {
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->shadowFramebuffer.id());
    glViewport(0, 0, quality.shadowMapSize, quality.shadowMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    auto d = impl_->drawScene(snapshot, true);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    return d;
  }));
  if(impl_->sparsePropContractReady && !impl_->options.objectClassMaskPass) {
    impl_->frameMetrics.passes.push_back(impl_->timed("sparse_prop_shadow_depth",[&] {
      glDisable(GL_FRAMEBUFFER_SRGB);
      glBindFramebuffer(GL_FRAMEBUFFER,impl_->sparsePropShadowFramebuffer.id());
      glViewport(0,0,SparsePropShadowProjection::mapSize,SparsePropShadowProjection::mapSize);
      glClear(GL_DEPTH_BUFFER_BIT);
      glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
      const auto draws=impl_->drawSparsePropShadow();
      glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
      return draws;
    }));
  }
  const GLuint sceneFbo = impl_->options.msaaSamples
                              ? impl_->msaaFramebuffer.id()
                              : impl_->hdrFramebuffer.id();
  impl_->frameMetrics.passes.push_back(impl_->timed("hdr_opaque", [&] {
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &impl_->frameMetrics.sceneDepthBits);
    glViewport(0, 0, impl_->initialization.framebufferWidth,
               impl_->initialization.framebufferHeight);
    if (impl_->options.objectClassMaskPass) glClearColor(0,0,0,1);
    else glClearColor(0.004f, 0.006f, 0.01f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (impl_->options.enableFinalOwnershipInstrumentation) {
      const GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
      glDrawBuffers(2, buffers);
      const GLfloat zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      glClearBufferfv(GL_COLOR, 1, zero);
      const GLenum colorOnly = GL_COLOR_ATTACHMENT0;
      glDrawBuffers(1, &colorOnly);
    }
    glDisable(GL_DEPTH_TEST);
    return impl_->drawScene(snapshot, false);
  }));
  for (bool dof : {false, true}) {
    if (dof ? impl_->frameMetrics.depthOfFieldRadius <= 0.0f
            : !impl_->frameMetrics.bloomApplied) continue;
    impl_->frameMetrics.passes.push_back(impl_->timed(dof ? "scene_dof" : "scene_bloom", [&] {
      // Resolve/copy opaque color to a distinct texture before sampling it.
      // This precedes guides and trajectories, so their adaptive contrast
      // sees the actual bloomed backdrop and their ink never blooms itself.
      glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, impl_->trajectoryBackdropFramebuffer.id());
      glDrawBuffer(GL_COLOR_ATTACHMENT0);
      glBlitFramebuffer(0, 0, impl_->initialization.framebufferWidth,
          impl_->initialization.framebufferHeight, 0, 0,
          impl_->initialization.framebufferWidth,
          impl_->initialization.framebufferHeight,
          GL_COLOR_BUFFER_BIT | (dof ? GL_DEPTH_BUFFER_BIT : 0), GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
      const GLenum colorOnly = GL_COLOR_ATTACHMENT0;
      glDrawBuffers(1, &colorOnly);
      glViewport(0, 0, impl_->initialization.framebufferWidth,
          impl_->initialization.framebufferHeight);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_BLEND);
      glDisable(GL_CULL_FACE);
      glDisable(GL_FRAMEBUFFER_SRGB);
      glUseProgram(impl_->toneProgram.id());
      glBindVertexArray(impl_->quadVao.id());
      glActiveTexture(GL_TEXTURE0);
      glBindSampler(0, 0);
      glBindTexture(GL_TEXTURE_2D, impl_->trajectoryBackdropColor.id());
      glUniform1i(glGetUniformLocation(impl_->toneProgram.id(), "uHdr"), 0);
      glUniform1i(glGetUniformLocation(impl_->toneProgram.id(), "uOutputLinear"), 1);
      glUniform1f(glGetUniformLocation(impl_->toneProgram.id(), "uExposureScale"),
          impl_->calibratedExposureScale());
      glUniform1f(glGetUniformLocation(impl_->toneProgram.id(), "uBloomStrength"),
          dof ? 0.0f : impl_->frameMetrics.bloomStrength);
      glUniform1f(glGetUniformLocation(impl_->toneProgram.id(), "uDofRadius"),
          dof ? impl_->frameMetrics.depthOfFieldRadius : 0.0f);
      glUniform1i(glGetUniformLocation(impl_->toneProgram.id(), "uSceneDepth"), 0);
      if (dof) {
        const auto projection = buildSharedCameraProjection(snapshot.camera,
            impl_->initialization.framebufferWidth, impl_->initialization.framebufferHeight);
        const auto eye = sharedCameraEye(snapshot.camera);
        const auto target = sharedCameraTarget(snapshot.camera);
        const float dx = target[0] - eye[0], dy = target[1] - eye[1], dz = target[2] - eye[2];
        const float focus = std::sqrt(dx * dx + dy * dy + dz * dz);
        glUniform1f(glGetUniformLocation(impl_->toneProgram.id(), "uFocusDistance"),
            std::isfinite(focus) ? std::max(2.0f, focus) : 180.0f);
        glUniform2f(glGetUniformLocation(impl_->toneProgram.id(), "uDepthProjection"),
            projection.projection[10], projection.projection[14]);
        glActiveTexture(GL_TEXTURE1);
        glBindSampler(1, 0);
        glBindTexture(GL_TEXTURE_2D, impl_->trajectoryBackdropDepth.id());
        glUniform1i(glGetUniformLocation(impl_->toneProgram.id(), "uSceneDepth"), 1);
      }
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, impl_->shadowDepth.id());
      glActiveTexture(GL_TEXTURE0);
      return 1u;
    }));
  }
  impl_->frameMetrics.passes.push_back(impl_->timed("overlay", [&] {
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    std::uint32_t draws = 0;
    if (snapshot.cue.showGuideLine) {
      glUseProgram(impl_->sceneProgram.id());
      glBindVertexArray(impl_->quadVao.id());
      impl_->bindMaterial(impl_->sceneProgram, "rubber");
      impl_->bindBallAtlas(impl_->sceneProgram);
      impl_->plainMaterial(impl_->sceneProgram);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      draws += impl_->drawGuide(snapshot);
      glDisable(GL_BLEND);
    }
    if (productSession != nullptr) {
      // Freeze the exact opaque+guide HDR backdrop before trajectory
      // submission. A separate texture avoids framebuffer feedback;
      // integer physical texels make adaptive polarity deterministic.
      const auto freezeBackdrop = [&](GLuint target) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(0, 0, impl_->initialization.framebufferWidth,
                          impl_->initialization.framebufferHeight, 0, 0,
                          impl_->initialization.framebufferWidth,
                          impl_->initialization.framebufferHeight,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
      };
      freezeBackdrop(impl_->trajectoryBackdropFramebuffer.id());
      freezeBackdrop(impl_->trajectoryCompositedBackdropFramebuffer.id());
      glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
      draws +=
          impl_->drawTrajectoryOverlay(*productSession, snapshot, sceneFbo);
    }
    return draws;
  }));
  if (impl_->options.msaaSamples) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->msaaFramebuffer.id());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, impl_->hdrFramebuffer.id());
    glBlitFramebuffer(0, 0, impl_->initialization.framebufferWidth,
                      impl_->initialization.framebufferHeight, 0, 0,
                      impl_->initialization.framebufferWidth,
                      impl_->initialization.framebufferHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
  if (impl_->options.enableFinalOwnershipInstrumentation) {
    impl_->frameMetrics.passes.push_back(
        impl_->timed("integrated_semantic_msaa_resolve", [&] {
          glBindFramebuffer(GL_FRAMEBUFFER,
                            impl_->finalOwnershipResolvedFramebuffer.id());
          glViewport(0, 0, impl_->initialization.framebufferWidth,
                     impl_->initialization.framebufferHeight);
          glDisable(GL_BLEND);
          const Program &resolveProgram =
              impl_->options.msaaSamples
                  ? impl_->finalOwnershipResolveMsaaProgram
                  : impl_->finalOwnershipResolveSingleProgram;
          glUseProgram(resolveProgram.id());
          glBindVertexArray(impl_->quadVao.id());
          glActiveTexture(GL_TEXTURE0);
          glBindSampler(0, 0);
          if (impl_->options.msaaSamples) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,
                          impl_->finalOwnershipMsaa.id());
            glUniform1i(
                glGetUniformLocation(resolveProgram.id(), "uSemanticMsaa"), 0);
            glUniform1i(
                glGetUniformLocation(resolveProgram.id(), "uSampleCount"),
                impl_->options.msaaSamples);
          } else {
            glBindTexture(GL_TEXTURE_2D, impl_->finalOwnershipScene.id());
            glUniform1i(
                glGetUniformLocation(resolveProgram.id(), "uSemanticSingle"),
                0);
          }
          glDrawArrays(GL_TRIANGLES, 0, 6);
          return 1u;
        }));
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->ldrFramebuffer.id());
    const GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                              GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, buffers);
    const GLfloat clearUi[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 1, clearUi);
    glClearBufferfv(GL_COLOR, 2, clearUi);
    const GLenum colorOnly = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &colorOnly);
  }
  impl_->frameMetrics.passes.push_back(impl_->timed("aces_tone_map", [&] {
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->ldrFramebuffer.id());
    glViewport(0, 0, impl_->initialization.framebufferWidth,
               impl_->initialization.framebufferHeight);
    if (impl_->initialization.enableSrgbOutput && !impl_->options.objectClassMaskPass)
      glEnable(GL_FRAMEBUFFER_SRGB);
    glUseProgram(impl_->toneProgram.id());
    glBindVertexArray(impl_->quadVao.id());
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D, impl_->hdrColor.id());
    glUniform1i(glGetUniformLocation(impl_->toneProgram.id(), "uHdr"), 0);
    glUniform1i(glGetUniformLocation(impl_->toneProgram.id(), "uSceneDepth"), 0);
    glUniform1f(glGetUniformLocation(impl_->toneProgram.id(), "uDofRadius"), 0.0f);
    glUniform1f(glGetUniformLocation(impl_->toneProgram.id(), "uBloomStrength"),
                0.0f);
    glUniform1i(glGetUniformLocation(impl_->toneProgram.id(), "uOutputLinear"), 0);
    glUniform1f(glGetUniformLocation(impl_->toneProgram.id(), "uExposureScale"),
                impl_->calibratedExposureScale());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_FRAMEBUFFER_SRGB);
    return 1u;
  }));
  if (productFrame &&
      impl_->options.enableFinalOwnershipInstrumentation) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->ldrFramebuffer.id());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                      impl_->finalOwnershipPreProductUiFramebuffer.id());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, impl_->initialization.framebufferWidth,
                      impl_->initialization.framebufferHeight, 0, 0,
                      impl_->initialization.framebufferWidth,
                      impl_->initialization.framebufferHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    impl_->finalOwnershipPreProductUiReady = true;
  }
  if (!productFrame) {
    impl_->frameMetrics.passes.push_back(impl_->timed("ui", [&] {
      if (impl_->options.suppressLegacyUi)
        return 0u;
      glBindFramebuffer(GL_FRAMEBUFFER, impl_->ldrFramebuffer.id());
      if (impl_->initialization.enableSrgbOutput && !impl_->options.objectClassMaskPass)
        glEnable(GL_FRAMEBUFFER_SRGB);
      glUseProgram(impl_->sceneProgram.id());
      glBindVertexArray(impl_->quadVao.id());
      impl_->bindMaterial(impl_->sceneProgram, "rubber");
      impl_->bindBallAtlas(impl_->sceneProgram);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, impl_->shadowDepth.id());
      glBindSampler(1, 0);
      impl_->plainMaterial(impl_->sceneProgram);
      const float power =
          std::clamp(snapshot.hud.shotPower / 200.0f, 0.0f, 1.0f);
      impl_->drawQuad(impl_->sceneProgram, -0.75f + 0.30f * power, -0.92f,
                      0.30f * power, 0.014f, 0, 0,
                      {{0.05f + power, 0.35f, 0.08f}}, 0);
      glDisable(GL_FRAMEBUFFER_SRGB);
      return 1u;
    }));
  } else {
    impl_->frameMetrics.passes.push_back(impl_->timed("product_ui", [&] {
      glBindFramebuffer(GL_FRAMEBUFFER, impl_->ldrFramebuffer.id());
      glViewport(0, 0, impl_->initialization.framebufferWidth,
                 impl_->initialization.framebufferHeight);
      if (impl_->options.enableFinalOwnershipInstrumentation) {
        const GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                  GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, buffers);
      }
      impl_->productUiDispatch->configureTrajectoryInsetChromeQueries(
          [&]() {
            GLint drawFramebuffer = 0;
            GLint attachmentType = GL_NONE;
            GLint attachmentName = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
            glGetFramebufferAttachmentParameteriv(
                GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachmentType);
            glGetFramebufferAttachmentParameteriv(
                GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &attachmentName);
            return ui::TrajectoryInsetChromeGpuTargetBinding{
                static_cast<std::uint32_t>(std::max(0, drawFramebuffer)),
                static_cast<std::uint32_t>(std::max(0, attachmentName)),
                drawFramebuffer == static_cast<GLint>(impl_->ldrFramebuffer.id()) &&
                    attachmentType == GL_TEXTURE &&
                    attachmentName == static_cast<GLint>(impl_->ldrColor.id())};
          },
          []() { return static_cast<std::uint32_t>(glGetError()); });
      const ModernProductUiRuntimeResult submitted =
          impl_->productUiPass->render(*productSession, snapshot,
                                       *productEnvironment);
      impl_->latestProductUiResult = submitted;
      if (!submitted.ok || !submitted.gpuSubmitted) {
        throw std::runtime_error(
            std::string("modern_renderer_product_ui_submit_failed:") +
            (submitted.errorCode.empty() ? "no_gpu_draws"
                                         : submitted.errorCode));
      }
      if (impl_->options.enableFinalOwnershipInstrumentation) {
        const GLenum colorOnly = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &colorOnly);
        impl_->finalOwnershipProductUiApplied = true;
      }
      return static_cast<std::uint32_t>(submitted.gpuMetrics.drawCalls);
    }));
    if (!evidenceReplay && trajectoryInsetVisibleRequest &&
        trajectoryInsetVisibleRequest->ok) {
      TrajectoryInsetProductUiChromeTarget chromeTarget;
      chromeTarget.sessionSerial = trajectoryInsetVisibleRequest->request.sessionSerial;
      chromeTarget.frameSerial = trajectoryInsetVisibleRequest->request.frameSerial;
      chromeTarget.layoutSerial = trajectoryInsetVisibleRequest->request.layoutSerial;
      chromeTarget.cp1GenerationId =
          trajectoryInsetVisibleRequest->request.cp1GenerationId;
      chromeTarget.cp1Digests = trajectoryInsetVisibleRequest->request.cp1Digests;
      chromeTarget.cp1GeometryDigest =
          trajectoryInsetVisibleRequest->request.cp1GeometryDigest;
      chromeTarget.cp2LayoutDigest =
          trajectoryInsetVisibleRequest->request.cp2LayoutDigest;
      chromeTarget.cp2ExclusionDigest =
          trajectoryInsetVisibleRequest->request.cp2ExclusionDigest;
      chromeTarget.palette = trajectoryInsetVisibleRequest->request.palette;
      chromeTarget.physicalWidth = impl_->initialization.framebufferWidth;
      chromeTarget.physicalHeight = impl_->initialization.framebufferHeight;
      chromeTarget.samples = trajectoryInsetVisibleRequest->request.samples;
      chromeTarget.insetControlFocused =
          trajectoryInsetVisibleRequest->layout.insetControlFocused;
      chromeTarget.cp1GeometryDigest =
          trajectoryInsetVisibleRequest->request.cp1GeometryDigest;
      chromeTarget.atlasCacheKey =
          impl_->productUiRuntime->trajectoryInsetChromeAtlasCacheKey();
      chromeTarget.chromeSemanticDigest =
          trajectoryInsetChromeSemanticDigest(
              trajectoryInsetVisibleRequest->chrome);
      GLint boundLdrFramebuffer = 0;
      glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundLdrFramebuffer);
      chromeTarget.currentLdrFramebuffer =
          static_cast<std::uint32_t>(std::max(0, boundLdrFramebuffer));
      chromeTarget.currentLdrColorTexture = impl_->ldrColor.id();
      const auto chromeReceipt =
          impl_->productUiPass->submitTrajectoryInsetChrome(
              *trajectoryInsetVisibleRequest, chromeTarget);
      const bool chromeReceiptValidated =
          validateTrajectoryInsetProductUiChromeReceipt(
              chromeReceipt, chromeTarget);
      // productChromeAlreadyInCurrentLdr = true only after this exact receipt
      // validation; final staging consumes the sealed current LDR unchanged.
      if (!chromeReceiptValidated) {
        trajectoryInsetAcceptedFailureBlocksProductPresent = true;
        invalidateTrajectoryInsetVisibleFinalStaging();
        invalidatePreparedTrajectoryInsetAtomicFrame();
      }
      ui::TrajectoryInsetVisibleProductCallbacks callbacks;
      callbacks.executeIntegration = [&](const auto &request) {
        TrajectoryInsetAtomicFrameInput input;
        input.cp1Package = request.package;
        input.cp2Layout = request.layout;
        input.frameSerial = request.frameSerial;
        input.cp1GenerationId = request.cp1GenerationId;
        input.cp1OrderedDigests = request.cp1Digests;
        input.cp1GeometryDigest = request.cp1GeometryDigest;
        input.cp2LayoutDigest = request.cp2LayoutDigest;
        input.cp2LayoutSerial = request.layoutSerial;
        if (trajectoryInsetAcceptedFailureBlocksProductPresent) {
          modern::TrajectoryInsetVisibleResult result;
          result.errorCode = "trajectory_inset_chrome_receipt_invalid:" +
              (chromeReceipt.errorCode.empty() ? std::string("validation_failed") : chromeReceipt.errorCode);
          return result;
        }
        const auto preparation = prepareTrajectoryInsetAtomicFrame(input);
        modern::TrajectoryInsetVisibleResult result;
        result.stateRestored = preparation.stateRestored;
        if (!preparation.ok) {
          result.errorCode = preparation.errorCode;
          return result;
        }
        std::string finalError;
        const bool finalPrepared = prepareTrajectoryInsetVisibleFinalStaging(
            *trajectoryInsetVisibleRequest, finalError);
        result.ok = result.presented = finalPrepared;
        result.errorCode = finalError;
        return result;
      };
      callbacks.submitChrome = [&](const auto &, const auto &,
                                   std::string &error) {
        if (!chromeReceiptValidated ||
            impl_->trajectoryInsetPublishedOutputFramebuffer == 0) {
          error = "trajectory_inset_visible_final_output_unpublished";
          return false;
        }
        return true;
      };
      callbacks.commitVisible = [] {};
      callbacks.invalidate = [&] {
        invalidateTrajectoryInsetVisibleFinalStaging();
        invalidatePreparedTrajectoryInsetAtomicFrame();
      };
      const auto visibleInsetResult =
          ui::executeTrajectoryInsetVisibleCurrentFrame(
              *trajectoryInsetVisibleRequest, callbacks);
      if (visibleInsetResult.ok)
        writeInsetMarkerReceipt(*trajectoryInsetVisibleRequest);
      if (!visibleInsetResult.ok)
        impl_->frameMetrics.trajectoryInsetVisibleErrorCode =
            visibleInsetResult.errorCode;
      if (impl_->trajectoryInsetPublishedOutputFramebuffer == 0)
        trajectoryInsetAcceptedFailureBlocksProductPresent = true;
    }
    // Product windows present only after the complete straight-alpha UI
    // submission succeeds. A failed UI frame never exposes a half-frame.
    impl_->frameMetrics.passes.push_back(impl_->timed("product_present", [&] {
      if (trajectoryInsetAcceptedFailureBlocksProductPresent)
        return 0u;
      const GLuint productOutput = evidenceReplay
          ? 0 : publishedProductOutputFramebuffer();
      glBindFramebuffer(GL_READ_FRAMEBUFFER,
                        productOutput != 0 ? productOutput
                                           : impl_->ldrFramebuffer.id());
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBlitFramebuffer(0, 0, impl_->initialization.framebufferWidth,
                        impl_->initialization.framebufferHeight, 0, 0,
                        impl_->initialization.framebufferWidth,
                        impl_->initialization.framebufferHeight,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, impl_->ldrFramebuffer.id());
      impl_->frameMetrics.productPresentPerformed = true;
      // glBlitFramebuffer is a present copy, not a glDraw* submission.
      return 0u;
    }));
  }
  std::uint32_t draws = 0;
  for (const auto &p : impl_->frameMetrics.passes)
    draws += p.drawCalls;
  impl_->frameMetrics.gltfSceneTriangles = impl_->lastSceneTriangles;
  impl_->frameMetrics.triangles =
      impl_->gltfSceneLoaded
          ? static_cast<std::uint32_t>(impl_->lastSceneTriangles)
          : draws * 2;
  impl_->finalOwnershipFrameReady =
      impl_->options.enableFinalOwnershipInstrumentation &&
      impl_->finalOwnershipProductUiApplied;
  impl_->frameMetrics.trajectoryInsetBlockedProductPresent =
      trajectoryInsetAcceptedFailureBlocksProductPresent;
}

bool ModernRenderer::capture(const RenderCaptureRequest &request,
                             std::string &error) {
  try {
    const int width = impl_->initialization.framebufferWidth,
              height = impl_->initialization.framebufferHeight;
    if ((request.width && request.width != width) ||
        (request.height && request.height != height))
      throw std::runtime_error("capture dimensions differ from latest rendered "
                               "framebuffer; resize and render first");
    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width * height * 3));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->ldrFramebuffer.id());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    const std::filesystem::path path(request.outputPath);
    if (!path.parent_path().empty())
      std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output)
      throw std::runtime_error("cannot open modern capture: " +
                               request.outputPath);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    const std::size_t row = static_cast<std::size_t>(width * 3);
    for (int y = height - 1; y >= 0; --y)
      output.write(reinterpret_cast<const char *>(
                       pixels.data() + static_cast<std::size_t>(y) * row),
                   static_cast<std::streamsize>(row));
    if (!output)
      throw std::runtime_error("modern capture write failed: " +
                               request.outputPath);
    return true;
  } catch (const std::exception &failure) {
    error = failure.what();
    return false;
  }
}

bool ModernRenderer::captureShadowDepthForValidation(
    const std::string &outputPath, std::string &error) const {
  try {
    if (!impl_->initialized || !impl_->gltfSceneLoaded)
      throw std::runtime_error("shadow depth validation requires initialized glTF");
    const int size = qualitySettings(impl_->options.quality).shadowMapSize;
    std::vector<float> depth(static_cast<std::size_t>(size * size));
    GLint previousReadFramebuffer = 0;
    GLint previousPackAlignment = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->shadowFramebuffer.id());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size, size, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      static_cast<GLuint>(previousReadFramebuffer));
    const std::filesystem::path path(outputPath);
    if (!path.parent_path().empty())
      std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output)
      throw std::runtime_error("cannot open shadow depth validation capture");
    output << "P5\n" << size << ' ' << size << "\n65535\n";
    for (int y = size - 1; y >= 0; --y)
      for (int x = 0; x < size; ++x) {
        const float value = std::clamp(
            depth[static_cast<std::size_t>(y * size + x)], 0.0f, 1.0f);
        const std::uint16_t encoded = static_cast<std::uint16_t>(
            std::lround(static_cast<double>(value) * 65535.0));
        const char bytes[2] = {
            static_cast<char>((encoded >> 8u) & 0xffu),
            static_cast<char>(encoded & 0xffu)};
        output.write(bytes, 2);
      }
    if (!output)
      throw std::runtime_error("shadow depth validation capture write failed");
    return true;
  } catch (const std::exception &failure) {
    error = failure.what();
    return false;
  }
}

bool ModernRenderer::captureIntegratedFinalOwnership(
    const std::string &captureId,
    training::IntegratedFinalOwnershipFrame &frame, std::string &error) {
  try {
    if (!impl_->options.enableFinalOwnershipInstrumentation)
      throw std::runtime_error("final_ownership_instrumentation_not_enabled");
    if (!impl_->finalOwnershipFrameReady)
      throw std::runtime_error("final_ownership_frame_not_ready");
    if (captureId.empty())
      throw std::runtime_error("final_ownership_capture_id_empty");
    const int width = impl_->initialization.framebufferWidth;
    const int height = impl_->initialization.framebufferHeight;
    const int samples = std::max(1, impl_->options.msaaSamples);
    std::vector<std::uint8_t> color(
        static_cast<std::size_t>(width * height * 3));
    std::vector<std::uint32_t> ownerTokens(
        static_cast<std::size_t>(width * height * 4));
    std::vector<std::uint8_t> uiCoverage(
        static_cast<std::size_t>(width * height));
    std::vector<std::uint8_t> chromeCoverage(uiCoverage.size());

    GLint previousReadFramebuffer = 0;
    GLint previousDrawFramebuffer = 0;
    GLint previousPackAlignment = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->ldrFramebuffer.id());
    GLint previousLdrReadBuffer = GL_COLOR_ATTACHMENT0;
    glGetIntegerv(GL_READ_BUFFER, &previousLdrReadBuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, color.data());
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE,
                 uiCoverage.data());
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    glReadPixels(0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE,
                 chromeCoverage.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      impl_->finalOwnershipResolvedFramebuffer.id());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                 ownerTokens.data());
    checkGl("paired final ownership readback");

    std::vector<std::array<float, 2>> samplePositions;
    if (samples == 1) {
      samplePositions.push_back({{0.5f, 0.5f}});
    } else {
      // GL_SAMPLE_POSITION is defined against the sample count of the current
      // draw framebuffer.  Querying while the single-sample resolved FBO is
      // current is GL_INVALID_VALUE on the shipping macOS 4.1 driver.
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, impl_->msaaFramebuffer.id());
      for (int sample = 0; sample < samples; ++sample) {
        std::array<GLfloat, 2> position{{0.0f, 0.0f}};
        glGetMultisamplefv(GL_SAMPLE_POSITION, static_cast<GLuint>(sample),
                           position.data());
        samplePositions.push_back({{position[0], position[1]}});
      }
      checkGl("final ownership MSAA sample positions");
    }
    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->ldrFramebuffer.id());
    glReadBuffer(static_cast<GLenum>(previousLdrReadBuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER,
                      static_cast<GLuint>(previousReadFramebuffer));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                      static_cast<GLuint>(previousDrawFramebuffer));

    frame = {};
    frame.captureId = captureId;
    frame.colorFrameId =
        captureId + ":" + std::to_string(impl_->renderedFrameSerial);
    frame.semanticFrameId = frame.colorFrameId;
    frame.physicalWidth = width;
    frame.physicalHeight = height;
    frame.msaaSamples = samples;
    frame.msaaSamplePositions = std::move(samplePositions);
    frame.colorContentSha256 = sha256Hex(std::string(
        reinterpret_cast<const char *>(color.data()), color.size()));
    std::string semanticBytes(
        reinterpret_cast<const char *>(ownerTokens.data()),
        ownerTokens.size() * sizeof(std::uint32_t));
    semanticBytes.append(reinterpret_cast<const char *>(uiCoverage.data()),
                         uiCoverage.size());
    semanticBytes.append(reinterpret_cast<const char *>(chromeCoverage.data()),
                         chromeCoverage.size());
    frame.semanticContentSha256 = sha256Hex(semanticBytes);

    for (std::size_t index = 0;
         index < impl_->trajectoryOverlayPackage.semanticBatches.size();
         ++index) {
      const training::TrajectorySemanticBatch &batch =
          impl_->trajectoryOverlayPackage.semanticBatches[index];
      training::FinalSemanticOwner owner;
      owner.token = batch.finalOwnershipToken;
      owner.track = batch.track == training::TrajectoryContrastTrack::Predicted
                        ? training::FinalSemanticTrack::Predicted
                        : training::FinalSemanticTrack::Actual;
      owner.componentRole = batch.componentRole;
      owner.primitiveId = batch.primitiveId;
      owner.productionDrawOrdinal = index;
      owner.visualSublayer =
          training::trajectoryVisualSublayerName(batch.visualSublayer);
      if (owner.token == 0 ||
          !frame.ownerTable.emplace(owner.token, owner).second)
        throw std::runtime_error("final_ownership_token_table_invalid");
    }

    const auto colorAt = [&](int x, int y) {
      const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 3;
      return std::array<std::uint8_t, 3>{
          {color[index], color[index + 1], color[index + 2]}};
    };
    frame.pixels.reserve(static_cast<std::size_t>(width * height));
    for (int glY = 0; glY < height; ++glY) {
      for (int x = 0; x < width; ++x) {
        const std::size_t pixelIndex =
            static_cast<std::size_t>(glY) * width + x;
        training::FinalSemanticPixel pixel;
        pixel.xPhysicalPx = x;
        pixel.yPhysicalPx = height - 1 - glY;
        const std::size_t tokenBase = pixelIndex * 4;
        for (int sample = 0; sample < samples; ++sample)
          pixel.sampleOwnerTokens.push_back(ownerTokens[tokenBase + sample]);
        pixel.laterUiCoverage =
            static_cast<double>(uiCoverage[pixelIndex]) / 255.0;
        pixel.interactiveChromeCoverage =
            static_cast<double>(chromeCoverage[pixelIndex]) / 255.0;
        pixel.finalLdrSrgb8 = colorAt(x, glY);
        frame.pixels.push_back(pixel);
      }
    }
    frame.renderGraph.passSequence.clear();
    for (const auto& pass : impl_->frameMetrics.passes)
      frame.renderGraph.passSequence.push_back(pass.name);
    frame.renderGraph.passSequence.push_back("paired_color_semantic_readback");
    frame.renderGraph.exactProductionVertexStream = true;
    frame.renderGraph.exactProductionTransforms =
        impl_->frameMetrics.trajectoryOverlayUsedSharedProjection;
    frame.renderGraph.actualThenPredictedSubmission = true;
    frame.renderGraph.homogeneousClipShared =
        impl_->frameMetrics.trajectoryOverlayHomogeneousClip;
    frame.renderGraph.safeAreaClipShared =
        impl_->frameMetrics.trajectoryOverlaySafeAreaClip;
    frame.renderGraph.playSurfaceStencilShared =
        impl_->frameMetrics.trajectoryOverlayPlaySurfaceClip;
    frame.renderGraph.exactMsaaSampleCountAndIndices = true;
    frame.renderGraph.blendAndLaterReplacementApplied = true;
    frame.renderGraph.semanticResolveBeforeToneMapPairing = true;
    frame.renderGraph.laterProductUiCoverageApplied =
        impl_->finalOwnershipProductUiApplied;
    frame.renderGraph.colorAndSemanticReadbackPaired = true;
    frame.renderGraph.instrumentationColorWritesDisabled = true;
    error.clear();
    return true;
  } catch (const std::exception &failure) {
    frame = {};
    error = failure.what();
    return false;
  }
}

void ModernRenderer::productUiContextLost() noexcept {
  invalidateTrajectoryInsetVisibleFinalStaging();
  impl_->trajectoryInsetFinalContextPoisoned = false;
  invalidatePreparedTrajectoryInsetAtomicFrame();
  if (impl_->productUiPass)
    impl_->productUiPass->contextLost();
}

bool ModernRenderer::reinitializeProductUi(std::string &error) {
  if (!impl_->initialized) {
    error =
        "modern_renderer_product_ui_reinitialize_before_renderer_initialize";
    return false;
  }
  if (!impl_->productUiPass) {
    error = "modern_renderer_product_ui_not_configured";
    return false;
  }
  const ui::UiGpuResult result = impl_->productUiPass->reinitializeGpu();
  if (!result.ok) {
    error = result.errorCode;
    return false;
  }
  impl_->trajectoryInsetFinalContextPoisoned = false;
  error.clear();
  return true;
}

bool ModernRenderer::setProductUiGlyphEvidenceSuppressed(
    bool suppressed, std::string &error) {
  if (!impl_->productUiRuntime) {
    error = "product_ui_runtime_unavailable";
    return false;
  }
  const ui::UiGpuResult result =
      impl_->productUiRuntime->setGlyphEvidenceSuppressed(suppressed);
  error = result.errorCode;
  return result.ok;
}

TrajectoryInsetAtomicPreparation
ModernRenderer::prepareTrajectoryInsetAtomicFrame(
    const TrajectoryInsetAtomicFrameInput &input) {
  TrajectoryInsetAtomicPreparation preparation;
  preparation.frameSerial = input.frameSerial;
  if (!impl_->initialized) {
    preparation.errorCode = "trajectory_inset_renderer_not_initialized";
    return preparation;
  }
  if (input.cp1Package == nullptr || input.cp2Layout == nullptr) {
    preparation.errorCode = "trajectory_inset_input_absent";
    return preparation;
  }
  if (impl_->trajectoryInsetDispatch != nullptr) {
    preparation.errorCode =
        "trajectory_inset_prepared_frame_not_consumed_or_invalidated";
    return preparation;
  }
  const training::TrajectoryInsetDrawPackage &package = *input.cp1Package;
  const ui::TrajectoryInsetShellLayout &layout = *input.cp2Layout;
  const auto same = [](float first, float second) {
    return std::abs(first - second) <= 1e-3f;
  };
  const bool rectMatches =
      same(package.layout.plotRect.x, layout.plotCutoutRect.x) &&
      same(package.layout.plotRect.y, layout.plotCutoutRect.y) &&
      same(package.layout.plotRect.width, layout.plotCutoutRect.width) &&
      same(package.layout.plotRect.height, layout.plotCutoutRect.height);
  std::string identityError;
  const bool packageInventoryValid =
      package.ok &&
      validateTrajectoryInsetPackageForSubmission(package, input, identityError);
  if (!packageInventoryValid && std::getenv("BILLIARDGL_INSET_FINAL_PROBE_SELF_TEST"))
    std::cerr << "inset_cp1_consumer_identity_error=" << identityError << '\n';
  const auto recomputedCp1Digests = recomputeTrajectoryInsetDigests(package);
  const std::array<std::string, 5> embeddedCp1Digests{
      {package.orderedParentSourceDigest, package.orderedProbeDigest,
       package.orderedDashEventDigest, package.orderedDashContributorDigest,
       package.orderedSourceRouteIntervalDigest}};
  modern::TrajectoryInsetAtomicIdentityContract identity;
  identity.cp1GenerationId = package.generationId;
  identity.frameSerial = input.frameSerial;
  identity.lastAttemptSerial = impl_->lastTrajectoryInsetAttemptSerial;
  identity.cp2LayoutSerial = input.cp2LayoutSerial;
  identity.embeddedCp1Digests = embeddedCp1Digests;
  identity.recomputedCp1Digests = recomputedCp1Digests;
  identity.expectedCp1Digests = input.cp1OrderedDigests;
  identity.recomputedCp1GeometryDigest = trajectoryInsetGeometryDigest(package);
  identity.expectedCp1GeometryDigest = input.cp1GeometryDigest;
  identity.recomputedCp2LayoutDigest = trajectoryInsetLayoutDigest(layout);
  identity.expectedCp2LayoutDigest = input.cp2LayoutDigest;
  identity.sourceGroups = package.sourceGroupDenominator;
  identity.mappingRows = package.mappingRows.size();
  identity.orderedParents = package.orderedParentSourceKeys.size();
  identity.semanticMembers = package.semanticBatches.size();
  identity.semanticDenominator = package.semanticMemberDenominator;
  identity.probes = package.probes.size();
  identity.probeDenominator = package.probeDenominator;
  identity.orderedProbeIds = package.orderedProbeIds.size();
  identity.generationContentDeepValid = packageInventoryValid;
  const bool identityValid =
      modern::validateTrajectoryInsetAtomicIdentity(identity, identityError);
  const bool layoutAuthorityValid =
      layout.ok && layout.insetVisible && layout.insetControlOn &&
      !layout.worldOverlayVisible && !layout.worldOverlayControlOn &&
      layout.trajectorySemanticMemberCount == 0 &&
      layout.trajectoryProbeCount == 0 &&
      layout.trajectoryDenominatorContribution == 0;
  if (!identityValid || !packageInventoryValid || !layoutAuthorityValid ||
      !rectMatches) {
    preparation.errorCode =
        identityError.empty()
            ? "trajectory_inset_explicit_input_contract_invalid"
            : identityError;
    return preparation;
  }
  // Validated frame serials are single-attempt. A failed GL transaction cannot
  // be replayed with the same CP1/CP2 labels or success flags.
  impl_->lastTrajectoryInsetAttemptSerial = input.frameSerial;
  const int samples =
      impl_->options.msaaSamples == 0 ? 1 : impl_->options.msaaSamples;
  if (samples != 1 && samples != 2 && samples != 4) {
    preparation.errorCode = "trajectory_inset_exact_sample_count_unsupported";
    return preparation;
  }
  modern::TrajectoryInsetAtomicRequest request;
  request.width =
      std::max(1, static_cast<int>(std::lround(layout.plotCutoutRect.width)));
  request.height =
      std::max(1, static_cast<int>(std::lround(layout.plotCutoutRect.height)));
  request.samples = samples;
  request.semanticSamples = samples;
  request.allocationGeneration = request.frameSerial =
      request.cp1PackageGeneration = request.cp2LayoutSerial =
          request.colorSerial = request.semanticSerial = request.toneSerial =
              input.frameSerial;
  request.completeParentMapping = packageInventoryValid;
  request.tokensValid = packageInventoryValid;
  request.colorSemanticCoveragePaired = packageInventoryValid;
  request.toneAlphaCoverageValid = packageInventoryValid;
  request.fixedPaletteOnly = trajectoryInsetPaletteContractValid(package);
  const auto appendMainTexture = [&](GLuint handle) {
    if (handle == 0)
      return;
    auto &entry =
        request.forbiddenMainResources[request.forbiddenMainResourceCount++];
    entry.kind =
        modern::TrajectoryInsetAtomicRequest::ExternalResourceKind::Texture;
    entry.handle = ModernTrajectoryInsetGlDispatch::externalTextureKey(handle);
  };
  const auto appendMainRenderbuffer = [&](GLuint handle) {
    if (handle == 0)
      return;
    auto &entry =
        request.forbiddenMainResources[request.forbiddenMainResourceCount++];
    entry.kind = modern::TrajectoryInsetAtomicRequest::ExternalResourceKind::
        Renderbuffer;
    entry.handle =
        ModernTrajectoryInsetGlDispatch::externalRenderbufferKey(handle);
  };
  appendMainTexture(impl_->hdrColor.id());
  appendMainTexture(impl_->hdrDepth.id());
  appendMainTexture(impl_->ldrColor.id());
  appendMainTexture(impl_->shadowDepth.id());
  appendMainTexture(impl_->sparsePropShadowDepth.id());
  appendMainTexture(impl_->trajectoryBackdropColor.id());
  appendMainTexture(impl_->trajectoryBackdropDepth.id());
  appendMainTexture(impl_->trajectoryCompositedBackdropColor.id());
  appendMainRenderbuffer(impl_->msaaColor);
  appendMainRenderbuffer(impl_->msaaDepth);
  appendMainTexture(impl_->finalOwnershipScene.id());
  appendMainTexture(impl_->finalOwnershipMsaa.id());
  appendMainTexture(impl_->finalOwnershipResolved.id());
  appendMainTexture(impl_->finalOwnershipUiCoverage.id());
  appendMainTexture(impl_->finalOwnershipChromeCoverage.id());
  appendMainTexture(impl_->finalOwnershipPreProductUiColor.id());
  request.instrumentationEnabled = input.instrumentationEnabled;
  request.instrumentationExpectedPairs =
      input.instrumentationEnabled ? package.probeDenominator : 0;
  request.insetRequested = true;
  request.worldOverlayRequested = false;
  try {
    auto dispatch =
        std::make_unique<ModernTrajectoryInsetGlDispatch>(package, layout);
    modern::TrajectoryInsetAtomicTransaction transaction(*dispatch);
    modern::TrajectoryInsetAtomicResult result = transaction.execute(request);
    preparation.stateRestored = result.stateRestored;
    preparation.resourceGeneration = result.resourceGeneration;
    preparation.stageLedger = result.stageLedger;
    preparation.mainHdrWrites = 0;
    preparation.mainWorldTokenWrites = 0;
    preparation.visibleCompositeWrites = 0;
    if (!result.ok || !dispatch->committedOffscreen()) {
      preparation.errorCode =
          result.errorCode.empty()
              ? "trajectory_inset_offscreen_transaction_failed"
              : result.errorCode;
      return preparation;
    }
    // trajectory_inset_visible_composite_deferred_to_cp4: retain only the
    // fully completed offscreen texture generation. renderFrame never
    // consumes it in CP3.
    impl_->trajectoryInsetDispatch = std::move(dispatch);
    preparation.ok = true;
    preparation.committedOffscreen = true;
    preparation.visibleCompositePerformed = false;
    preparation.errorCode.clear();
    impl_->trajectoryInsetPreparation = preparation;
    return preparation;
  } catch (...) {
    impl_->trajectoryInsetDispatch.reset();
    preparation.errorCode =
        "trajectory_inset_transaction_boundary_unexpected_exception";
    return preparation;
  }
}

void ModernRenderer::invalidatePreparedTrajectoryInsetAtomicFrame() noexcept {
  impl_->trajectoryInsetDispatch.reset();
  impl_->trajectoryInsetPreparation = {};
}

bool ModernRenderer::prepareTrajectoryInsetVisibleFinalStaging(
    ui::TrajectoryInsetVisibleCurrentFrame &current, std::string &error) {
  if (!impl_->trajectoryInsetDispatch || !impl_->trajectoryInsetPreparation.ok) {
    error = "trajectory_inset_final_missing_cp3_staging";
    return false;
  }
  if (impl_->trajectoryInsetFinalContextPoisoned) {
    error = "trajectory_inset_final_context_poisoned";
    return false;
  }
  if (impl_->trajectoryInsetVisibleFinalDispatch &&
      !impl_->trajectoryInsetVisibleFinalDispatch->contextUsable()) {
    error = "trajectory_inset_final_context_poisoned";
    return false;
  }
  invalidateTrajectoryInsetVisibleFinalStaging();
  const GLuint semantic = impl_->options.enableFinalOwnershipInstrumentation
                              ? impl_->finalOwnershipResolved.id()
                              : impl_->trajectoryBackdropColor.id();
  impl_->trajectoryInsetVisibleFinalDispatch =
      std::make_unique<ModernTrajectoryInsetVisibleFinalGlDispatch>(
          impl_->trajectoryInsetDispatch->stagingTexture(),
          impl_->ldrColor.id(), semantic, current,
          *impl_->trajectoryInsetDispatch);
  modern::TrajectoryInsetVisibleFinalGlInput input;
  input.current = &current;
  input.sessionSerial = current.request.sessionSerial;
  input.frameSerial = current.request.frameSerial;
  input.layoutSerial = current.request.layoutSerial;
  input.packageGeneration = current.request.cp1GenerationId;
  input.preparationGeneration = impl_->trajectoryInsetPreparation.resourceGeneration;
  input.width = std::max(
      1, static_cast<int>(std::lround(current.layout.plotCutoutRect.width)));
  input.height = std::max(
      1, static_cast<int>(std::lround(current.layout.plotCutoutRect.height)));
  input.outputWidth = impl_->initialization.framebufferWidth;
  input.outputHeight = impl_->initialization.framebufferHeight;
  input.cp3InputSamples = current.request.samples;
  input.finalOutputSamples = 1;
  input.cutout = current.layout.plotCutoutRect;
  input.cp3ColorStaging = {
      modern::TrajectoryInsetFinalResourceKind::Texture,
      impl_->trajectoryInsetDispatch->stagingTexture()};
  input.cp3InsetSemantic = {
      modern::TrajectoryInsetFinalResourceKind::Texture,
      impl_->trajectoryInsetDispatch->resolvedSemanticTexture()};
  input.cp3InsetSemanticFormat = "GL_RGBA32UI";
  input.cp3InsetSemanticGeneration =
      impl_->trajectoryInsetPreparation.resourceGeneration;
  input.cp3InsetSemanticChannel = 0;
  input.currentProductLdr = {modern::TrajectoryInsetFinalResourceKind::Texture,
                             impl_->ldrColor.id()};
  input.mainWorldSemantic = {
      modern::TrajectoryInsetFinalResourceKind::Texture, semantic};
  impl_->trajectoryInsetVisibleFinalResult =
      modern::prepareTrajectoryInsetVisibleFinalGlStaging(
          input, *impl_->trajectoryInsetVisibleFinalDispatch);
  if (!impl_->trajectoryInsetVisibleFinalResult.ok) {
    impl_->trajectoryInsetFinalContextPoisoned =
        impl_->trajectoryInsetVisibleFinalResult.adapterContextPoisoned;
    error = impl_->trajectoryInsetVisibleFinalResult.errorCode;
    return false;
  }
  impl_->trajectoryInsetPublishedOutputFramebuffer =
      impl_->trajectoryInsetVisibleFinalDispatch
          ->publishedProductOutputFramebuffer();
  if (impl_->trajectoryInsetPublishedOutputFramebuffer != 0) {
    impl_->trajectoryInsetToggleRect = current.layout.toggleRect;
    impl_->trajectoryInsetPlotRect = current.layout.plotCutoutRect;
    impl_->trajectoryInsetPresentedSerial = current.request.frameSerial;
  }
  error.clear();
  return impl_->trajectoryInsetPublishedOutputFramebuffer != 0;
}

std::uint32_t
ModernRenderer::publishedProductOutputFramebuffer() const noexcept {
  return impl_->trajectoryInsetPublishedOutputFramebuffer;
}

void ModernRenderer::invalidateTrajectoryInsetVisibleFinalStaging() noexcept {
  impl_->trajectoryInsetToggleRect.reset();
  impl_->trajectoryInsetPlotRect.reset();
  impl_->trajectoryInsetPresentedSerial = 0;
  if (impl_->trajectoryInsetVisibleFinalDispatch)
    modern::invalidateTrajectoryInsetVisibleFinalGlStaging(
        modern::TrajectoryInsetVisibleFinalGlInvalidation::FrameSuperseded,
        impl_->trajectoryInsetVisibleFinalResult,
        *impl_->trajectoryInsetVisibleFinalDispatch);
  impl_->trajectoryInsetVisibleFinalDispatch.reset();
  impl_->trajectoryInsetVisibleFinalResult = {};
  impl_->trajectoryInsetPublishedOutputFramebuffer = 0;
}

std::optional<product_hud::HudRect>
ModernRenderer::latestTrajectoryInsetToggleRect() const noexcept {
  return impl_->trajectoryInsetToggleRect;
}

std::optional<product_hud::HudRect>
ModernRenderer::latestTrajectoryInsetPlotRect() const noexcept {
  return impl_->trajectoryInsetPlotRect;
}

std::uint64_t ModernRenderer::latestTrajectoryInsetFrameSerial() const noexcept {
  return impl_->trajectoryInsetPresentedSerial;
}

bool ModernRenderer::verifyTrajectoryInsetTokenCodec(std::string& error) {
  if (!impl_->initialized) { error="inset_codec_renderer_not_initialized"; return false; }
  training::TrajectoryInsetDrawPackage package;
  package.layout.plotRect = {0,0,2,2};
  const std::array<std::uint32_t,4> tokens{{0x01000000u,0x80000001u,0xffffffffu,1u}};
  for (std::size_t i=0;i<tokens.size();++i) {
    const float x=static_cast<float>(i%2), y=static_cast<float>(i/2);
    const std::array<float,4> color{{0.15f+0.1f*static_cast<float>(i),0.35f,0.6f,1.0f}};
    const std::array<std::array<float,2>,6> points{{
        {{x,y}},{{x+1,y}},{{x+1,y+1}},{{x,y}},{{x+1,y+1}},{{x,y+1}}}};
    training::TrajectoryInsetSemanticBatch batch;
    batch.firstVertex=package.vertices.size(); batch.vertexCount=6;
    batch.insetOwnershipToken=tokens[i];
    package.semanticBatches.push_back(batch);
    for (const auto& point:points)
      package.vertices.push_back({point[0],point[1],color,tokens[i]});
  }
  const ui::TrajectoryInsetShellLayout layout;
  ModernTrajectoryInsetGlDispatch dispatch(package,layout);
  return dispatch.verifyTokenCodec(error);
}

void ModernRenderer::shutdown() noexcept {
  if (!impl_->initialized)
    return;
  // Deleting the currently bound program only marks it for deletion. A later
  // UI state guard could save that name, switch programs (finishing deletion),
  // then attempt to restore the dead name during renderer reinitialization.
  GLint currentProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
  for (GLuint owned : {impl_->sceneProgram.id(), impl_->depthProgram.id(),
           impl_->toneProgram.id(), impl_->meshProgram.id(), impl_->meshDepthProgram.id(), impl_->sparsePropReceiverProgram.id(),
           impl_->trajectoryOverlayProgram.id(), impl_->finalOwnershipResolveSingleProgram.id(),
           impl_->finalOwnershipResolveMsaaProgram.id()}) {
    if (owned != 0 && owned == static_cast<GLuint>(currentProgram)) {
      glUseProgram(0);
      break;
    }
  }
  if (impl_->productUiPass)
    impl_->productUiPass->shutdown();
  impl_->productUiPass.reset();
  impl_->productUiRuntime.reset();
  impl_->productUiDispatch.reset();
  impl_->trajectoryInsetDispatch.reset();
  impl_->trajectoryInsetVisibleFinalDispatch.reset();
  impl_->trajectoryInsetPreparation = {};
  impl_->releaseTargets();
  impl_->gpuScene = {};
  impl_->gpuVenue = {};
  impl_->pbrTextures.reset();
  impl_->plainFallbackColor.reset();
  impl_->plainFallbackEnvironment.reset();
  impl_->trajectoryOverlayBuffer.reset();
  impl_->trajectoryOverlayVao.reset();
  impl_->trajectoryOverlayProgram.reset();
  impl_->finalOwnershipResolveSingleProgram.reset();
  impl_->finalOwnershipResolveMsaaProgram.reset();
  impl_->quadBuffer.reset();
  impl_->quadVao.reset();
  impl_->sceneProgram.reset();
  impl_->depthProgram.reset();
  impl_->toneProgram.reset();
  impl_->meshProgram.reset();
  impl_->meshDepthProgram.reset();
  impl_->sparsePropReceiverProgram.reset();
  impl_->sparsePropContractReady=false;
  impl_->sparsePropProjection={};
  impl_->restoreSeamlessCubemapState();
  impl_->seamlessCubemapEnabled = true;
  impl_->seamlessCubemapPreviouslyEnabled = false;
  impl_->gltfContactShadowScale = 0.25f;
  impl_->initialized = false;
}

const ModernFrameMetrics &ModernRenderer::metrics() const noexcept {
  return impl_->frameMetrics;
}
const ModernRendererProductUiMetrics &
ModernRenderer::productUiMetrics() const noexcept {
  static const ModernRendererProductUiMetrics empty;
  return impl_->productUiPass ? impl_->productUiPass->metrics() : empty;
}
const ModernProductUiRuntimeResult &
ModernRenderer::latestProductUiResult() const noexcept {
  return impl_->latestProductUiResult;
}
const LookdevLoadResult &ModernRenderer::lookdev() const noexcept {
  return impl_->lookdevResult;
}
const PbrPackageDescription &ModernRenderer::pbrPackage() const noexcept {
  return impl_->pbrPackage;
}
const PbrUploadReport &ModernRenderer::pbrUpload() const noexcept {
  return impl_->pbrTextures.report();
}
bool ModernRenderer::gltfSceneLoaded() const noexcept {
  return impl_->gltfSceneLoaded;
}
bool ModernRenderer::venueSceneLoaded() const noexcept {
  return impl_->venueSceneLoaded;
}

} // namespace billiardgl::render
