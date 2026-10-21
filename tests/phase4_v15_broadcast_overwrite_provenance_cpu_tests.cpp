#define main phase4_v14_frozen_control_main
#include "docs/art/phase4-v14-cap-mask-premise-falsification-v2/phase4_v14_cap_mask_cpu_diagnostic.cpp"
#undef main

#include <iomanip>
#include <limits>
#include <sstream>

namespace v15 {

using namespace billiardgl;
using namespace billiardgl::training;

constexpr int kRadius = 8;
constexpr std::array<std::array<int, 2>, 3> kGaps{{
    {{783, 413}}, {{892, 491}}, {{940, 525}}}};
constexpr const char* kCapture = "camera_broadcast_1080";
constexpr const char* kDigest =
    "33511aa46717a208f3db43acd278416b8fe8393e2e003484da8f44dabffe5645";
constexpr std::array<const char*, 3> kRepairIds{{
    "d6d850a1a10c7c7ce403e6de4afd4330e4c6e89b9100c72f4c23fbab1f6f978b",
    "1890fd0652c8f74a8b03a35a6cee1ad2cab43062987cab73efb1f6b455da468a",
    "1bf60bcd841a0d754c82876b55a4d3d6c29ebad582e069d4f4a608a4778c33a9"}};

enum class Fragment {
    CoreBaseBody,
    CoreStartCapExtension,
    CoreEndCapExtension,
    InnerCentralBody,
    InnerLateralAnnulus,
    InnerStartCapAnnulus,
    InnerEndCapAnnulus,
    OuterSideBand,
    EndpointOrTickOrMarker,
};

enum class Interval {
    DashOnInterior,
    DashGapIntrusion,
    RouteEndpointEnvelope,
    CrossSegmentOrCrossSourceOverlap,
};

const char* name(Fragment value)
{
    switch (value) {
    case Fragment::CoreBaseBody: return "core_base_body";
    case Fragment::CoreStartCapExtension: return "core_start_cap_extension";
    case Fragment::CoreEndCapExtension: return "core_end_cap_extension";
    case Fragment::InnerCentralBody: return "inner_central_body";
    case Fragment::InnerLateralAnnulus: return "inner_lateral_annulus";
    case Fragment::InnerStartCapAnnulus: return "inner_start_cap_annulus";
    case Fragment::InnerEndCapAnnulus: return "inner_end_cap_annulus";
    case Fragment::OuterSideBand: return "outer_side_band";
    case Fragment::EndpointOrTickOrMarker:
        return "endpoint_or_tick_or_marker";
    }
    return "invalid";
}

const char* name(Interval value)
{
    switch (value) {
    case Interval::DashOnInterior: return "dash_on_interior";
    case Interval::DashGapIntrusion: return "dash_gap_intrusion";
    case Interval::RouteEndpointEnvelope: return "route_endpoint_envelope";
    case Interval::CrossSegmentOrCrossSourceOverlap:
        return "cross_segment_or_cross_source_overlap";
    }
    return "invalid";
}

std::string stem(std::string primitive)
{
    for (const std::string suffix : {"_core", "_halo"}) {
        if (primitive.size() >= suffix.size() &&
                primitive.compare(primitive.size() - suffix.size(),
                    suffix.size(), suffix) == 0) {
            primitive.resize(primitive.size() - suffix.size());
            break;
        }
    }
    return primitive;
}

std::string provenanceKey(const std::string& arc,
    const std::string& primitive)
{
    return arc + "|" + stem(primitive);
}

bool crossesDistinctProvenance(std::set<std::string>& observed,
    const std::string& arc, const std::string& primitive)
{
    observed.insert(provenanceKey(arc, primitive));
    return observed.size() > 1;
}

void testCrossClassificationContract()
{
    std::set<std::string> observed;
    expect(!crossesDistinctProvenance(observed, "predicted_arc_7",
            "predicted_dash_11_halo"), "first_stem_cannot_be_cross");
    expect(!crossesDistinctProvenance(observed, "predicted_arc_7",
            "predicted_dash_11_core"),
        "same_stem_core_halo_misclassified_cross");
    expect(crossesDistinctProvenance(observed, "predicted_arc_7",
            "predicted_dash_12_core"),
        "different_stem_must_be_cross");
    observed.clear();
    expect(!crossesDistinctProvenance(observed, "predicted_arc_7",
            "predicted_dash_11_core"), "new_arc_fixture_first_not_cross");
    expect(crossesDistinctProvenance(observed, "predicted_arc_8",
            "predicted_dash_11_core"), "different_arc_must_be_cross");
}

struct ProbeIdentity {
    std::string id;
    const TrajectoryContrastProbe* probe = nullptr;
};

std::vector<ProbeIdentity> sourceIdentities(
    const TrajectoryOverlayDrawPackage& package)
{
    std::vector<ProbeIdentity> output;
    std::string ordered;
    const std::string profile = "1920x1080|line=" +
        std::to_string(package.style.lineWidthPx) + "|halo=" +
        std::to_string(package.style.haloWidthPx);
    for (const auto& probe : package.contrastProbes) {
        CompoundSourceProbe source;
        source.captureId = kCapture;
        source.track = compoundTrack(probe.track);
        source.primitiveId = probe.primitiveId;
        source.componentRole = probe.componentRole;
        source.arcSegmentId = probe.arcSegmentId;
        source.probeOrdinal = probe.probeOrdinal;
        source.physicalProfile = profile;
        source.xPhysicalPx = probe.componentRole ==
                CompoundComponentRole::OuterHaloEdge ?
            probe.haloXPhysicalPx : probe.strokeXPhysicalPx;
        source.yPhysicalPx = probe.componentRole ==
                CompoundComponentRole::OuterHaloEdge ?
            probe.haloYPhysicalPx : probe.strokeYPhysicalPx;
        source.outerHaloRadiusPx = package.style.lineWidthPx * 0.5 +
            package.style.haloWidthPx;
        source.halfTickLengthPx = package.style.actualTickLengthPx * 0.5;
        source.sourceProbeSpacingPx = source.track ==
                CompoundVisibilityTrack::Predicted ?
            package.style.dashOnPx + package.style.dashOffPx :
            package.style.actualTickSpacingPx;
        source.insideFinalViewport = true;
        const std::string id = stableCompoundSourceProbeId(source);
        output.push_back({id, &probe});
        ordered += id + "\n";
    }
    expect(output.size() == 81, "v15_source_count_not_81");
    expect(sha256Hex(ordered) == kDigest, "v15_source_digest_drift");
    std::set<std::string> unique;
    for (const auto& item : output) unique.insert(item.id);
    expect(unique.size() == output.size(), "v15_duplicate_source_id");
    for (const char* id : kRepairIds)
        expect(unique.count(id) == 1, std::string("repair_id_missing:") + id);
    return output;
}

std::string sourceForPrimitive(const std::vector<ProbeIdentity>& sources,
    TrajectoryContrastTrack track, const std::string& primitive,
    const std::string& arc, double x, double y)
{
    const std::string wanted = stem(primitive);
    double best = std::numeric_limits<double>::infinity();
    std::string result;
    for (const auto& source : sources) {
        const auto& probe = *source.probe;
        if (probe.track != track) continue;
        const bool exactStem = stem(probe.primitiveId) == wanted;
        const bool sameArc = probe.arcSegmentId == arc;
        const double dx = probe.strokeXPhysicalPx - x;
        const double dy = probe.strokeYPhysicalPx - y;
        const double distance = dx * dx + dy * dy +
            (exactStem ? 0.0 : sameArc ? 1.0e6 : 2.0e6);
        if (distance < best) {
            best = distance;
            result = source.id;
        }
    }
    expect(!result.empty(), "source_probe_association_missing:" + primitive);
    return result;
}

const TrajectoryV15PredictedDashProvenance* predictedPrimitive(
    const TrajectoryOverlayDrawPackage& package, const std::string& primitive)
{
    const std::string wanted = stem(primitive);
    for (const auto& item : package.v15PredictedDashProvenance)
        if (item.primitiveStem == wanted) return &item;
    return nullptr;
}

struct Classification {
    Fragment fragment = Fragment::EndpointOrTickOrMarker;
    Interval interval = Interval::RouteEndpointEnvelope;
    std::uint64_t dashOrdinal = 0;
    double localTangent = 0.0;
    bool protectedBodyOrLateral = true;
    bool futureGapCandidate = false;
};

Classification classify(const TrajectoryOverlayDrawPackage& package,
    const TrajectorySemanticBatch& batch, double x, double y,
    bool crossOverlap)
{
    Classification result;
    const auto* primitive = predictedPrimitive(package, batch.primitiveId);
    if (batch.track != TrajectoryContrastTrack::Predicted || !primitive)
        return result;
    const double vx = primitive->secondXPhysicalPx -
        primitive->firstXPhysicalPx;
    const double vy = primitive->secondYPhysicalPx -
        primitive->firstYPhysicalPx;
    const double length = std::hypot(vx, vy);
    const double tx = vx / length;
    const double ty = vy / length;
    const double dx = x - primitive->firstXPhysicalPx;
    const double dy = y - primitive->firstYPhysicalPx;
    const double tangent = dx * tx + dy * ty;
    const double normal = std::fabs(-dx * ty + dy * tx);
    result.dashOrdinal = primitive->dashOrdinal;
    result.localTangent = tangent;
    if (batch.visualSublayer == TrajectoryVisualSublayer::Core) {
        if (tangent < 0.0)
            result.fragment = Fragment::CoreStartCapExtension;
        else if (tangent > length)
            result.fragment = Fragment::CoreEndCapExtension;
        else result.fragment = Fragment::CoreBaseBody;
    } else if (batch.visualSublayer == TrajectoryVisualSublayer::InnerHalo) {
        if (tangent < 0.0)
            result.fragment = Fragment::InnerStartCapAnnulus;
        else if (tangent > length)
            result.fragment = Fragment::InnerEndCapAnnulus;
        else if (normal <= primitive->coreHalfWidthPx + 1e-7)
            result.fragment = Fragment::InnerCentralBody;
        else result.fragment = Fragment::InnerLateralAnnulus;
    } else result.fragment = Fragment::OuterSideBand;
    const bool cap = result.fragment == Fragment::CoreStartCapExtension ||
        result.fragment == Fragment::CoreEndCapExtension ||
        result.fragment == Fragment::InnerStartCapAnnulus ||
        result.fragment == Fragment::InnerEndCapAnnulus;
    result.interval = crossOverlap ?
        Interval::CrossSegmentOrCrossSourceOverlap :
        cap ? Interval::DashGapIntrusion : Interval::DashOnInterior;
    result.protectedBodyOrLateral =
        result.fragment == Fragment::CoreBaseBody ||
        result.fragment == Fragment::InnerLateralAnnulus ||
        result.interval == Interval::DashOnInterior;
    result.futureGapCandidate = cap &&
        result.interval == Interval::DashGapIntrusion;
    return result;
}

struct SampleLedger {
    int x = 0;
    int y = 0;
    std::string priorActualSource;
    std::string priorActualPrimitive;
    bool priorActualRouteBody = false;
    std::string finalPredictedSource;
    std::string finalPredictedPrimitive;
    Classification classification;
    std::set<std::string> predictedPrimitives;
};

using PixelKey = std::pair<int, int>;

bool inAnyGap(int x, int y)
{
    for (const auto& gap : kGaps)
        if (std::abs(x - gap[0]) <= kRadius &&
                std::abs(y - gap[1]) <= kRadius)
            return true;
    return false;
}

std::map<PixelKey, SampleLedger> rasterizeLedger(
    const TrajectoryOverlayDrawPackage& package,
    const std::vector<ProbeIdentity>& sources)
{
    std::map<PixelKey, SampleLedger> output;
    for (const auto& gap : kGaps)
        for (int y = gap[1] - kRadius; y <= gap[1] + kRadius; ++y)
            for (int x = gap[0] - kRadius; x <= gap[0] + kRadius; ++x)
                output[{x, y}] = SampleLedger{x, y};
    for (const auto& source : sources) {
        if (std::find(kRepairIds.begin(), kRepairIds.end(), source.id) ==
                kRepairIds.end()) continue;
        const auto& probe = *source.probe;
        const int x = probe.componentRole ==
                CompoundComponentRole::OuterHaloEdge ?
            probe.haloXPhysicalPx : probe.strokeXPhysicalPx;
        const int y = probe.componentRole ==
                CompoundComponentRole::OuterHaloEdge ?
            probe.haloYPhysicalPx : probe.strokeYPhysicalPx;
        output.emplace(PixelKey{x, y}, SampleLedger{x, y});
    }
    for (const auto& batch : package.semanticBatches) {
        expect(batch.firstVertex + batch.vertexCount <= package.vertices.size()
            && batch.vertexCount % 3 == 0, "v15_semantic_range_invalid");
        for (std::size_t offset = 0; offset < batch.vertexCount; offset += 3) {
            const auto& a = package.vertices[batch.firstVertex + offset];
            const auto& b = package.vertices[batch.firstVertex + offset + 1];
            const auto& c = package.vertices[batch.firstVertex + offset + 2];
            const int left = static_cast<int>(std::floor(
                std::min({a.xPhysicalPx, b.xPhysicalPx, c.xPhysicalPx})));
            const int right = static_cast<int>(std::ceil(
                std::max({a.xPhysicalPx, b.xPhysicalPx, c.xPhysicalPx})));
            const int top = static_cast<int>(std::floor(
                std::min({a.yPhysicalPx, b.yPhysicalPx, c.yPhysicalPx})));
            const int bottom = static_cast<int>(std::ceil(
                std::max({a.yPhysicalPx, b.yPhysicalPx, c.yPhysicalPx})));
            for (int y = top; y <= bottom; ++y) {
                for (int x = left; x <= right; ++x) {
                    if (!inAnyGap(x, y) || !insideTriangle(a, b, c,
                            x + 0.5, y + 0.5)) continue;
                    auto& sample = output.at({x, y});
                    if (batch.track == TrajectoryContrastTrack::Actual) {
                        const bool routeBody =
                            batch.visualSublayer ==
                                TrajectoryVisualSublayer::Core &&
                            stem(batch.primitiveId).find("actual_segment_") == 0;
                        sample.priorActualRouteBody = routeBody;
                        if (routeBody) {
                            sample.priorActualPrimitive = batch.primitiveId;
                            sample.priorActualSource = sourceForPrimitive(
                                sources, batch.track, batch.primitiveId,
                                batch.arcSegmentId, x, y);
                        } else {
                            sample.priorActualPrimitive.clear();
                            sample.priorActualSource.clear();
                        }
                    } else {
                        const bool cross = crossesDistinctProvenance(
                            sample.predictedPrimitives, batch.arcSegmentId,
                            batch.primitiveId);
                        sample.finalPredictedPrimitive = batch.primitiveId;
                        sample.finalPredictedSource = sourceForPrimitive(
                            sources, batch.track, batch.primitiveId,
                            batch.arcSegmentId, x, y);
                        sample.classification = classify(package, batch,
                            x + 0.5, y + 0.5, cross);
                    }
                }
            }
        }
    }
    return output;
}

std::string escape(const std::string& value)
{
    std::ostringstream out;
    for (char c : value) {
        if (c == '\\' || c == '"') out << '\\';
        out << c;
    }
    return out.str();
}

struct PaletteResult {
    struct RepairSource {
        std::string track;
        std::string componentRole;
        std::string primitiveId;
        std::string arcSegmentId;
        PixelKey coordinate;
    };
    std::string palette;
    std::map<PixelKey, SampleLedger> ledger;
    std::map<std::string, std::size_t> blockingFragments;
    std::size_t blockingCore = 0;
    std::size_t blockingInner = 0;
    std::map<std::string, PixelKey> repairCoordinates;
    std::map<std::string, RepairSource> repairSources;
};

PaletteResult evaluatePalette(const TrajectoryOverlaySnapshot& snapshot,
    GameRuntime& runtime, TrajectoryOverlayPalette palette,
    const char* paletteName)
{
    const auto package = buildTrajectoryOverlayDrawPackage(snapshot,
        cameraFor(runtime, camera::Preset::Broadcast),
        {kWidth, kHeight, 1.0, 1.0}, palette);
    requireSourceInventory(package, kCapture, 81, 69, 12);
    expect(package.v15PredictedDashProvenance.size() ==
        package.predicted.visibleStrokeCount,
        "predicted_provenance_count_mismatch");
    expect(package.v15ActualRouteBodyProvenance.size() ==
        package.actual.visibleStrokeCount,
        "actual_provenance_count_mismatch");
    const auto sources = sourceIdentities(package);
    PaletteResult result;
    result.palette = paletteName;
    result.ledger = rasterizeLedger(package, sources);
    for (const auto& source : sources) {
        if (std::find(kRepairIds.begin(), kRepairIds.end(), source.id) ==
                kRepairIds.end()) continue;
        const auto& probe = *source.probe;
        const PixelKey coordinate{
            probe.componentRole == CompoundComponentRole::OuterHaloEdge ?
                probe.haloXPhysicalPx : probe.strokeXPhysicalPx,
            probe.componentRole == CompoundComponentRole::OuterHaloEdge ?
                probe.haloYPhysicalPx : probe.strokeYPhysicalPx};
        result.repairCoordinates[source.id] = coordinate;
        result.repairSources[source.id] = {
            probe.track == TrajectoryContrastTrack::Predicted ?
                "predicted" : "actual",
            compoundComponentRoleName(probe.componentRole),
            probe.primitiveId, probe.arcSegmentId, coordinate};
    }
    const auto blocking = kGaps[2];
    for (const auto& item : result.ledger) {
        if (std::abs(item.first.first - blocking[0]) > kRadius ||
                std::abs(item.first.second - blocking[1]) > kRadius ||
                !item.second.priorActualRouteBody ||
                item.second.finalPredictedPrimitive.empty()) continue;
        const std::string fragment = name(item.second.classification.fragment);
        ++result.blockingFragments[fragment];
        if (item.second.classification.fragment == Fragment::CoreBaseBody ||
                item.second.classification.fragment ==
                    Fragment::CoreStartCapExtension ||
                item.second.classification.fragment ==
                    Fragment::CoreEndCapExtension)
            ++result.blockingCore;
        if (item.second.classification.fragment == Fragment::InnerCentralBody ||
                item.second.classification.fragment ==
                    Fragment::InnerLateralAnnulus ||
                item.second.classification.fragment ==
                    Fragment::InnerStartCapAnnulus ||
                item.second.classification.fragment ==
                    Fragment::InnerEndCapAnnulus)
            ++result.blockingInner;
    }
    expect(result.blockingCore == 81, "blocking_core_partition_not_81:" +
        std::to_string(result.blockingCore));
    expect(result.blockingInner == 7, "blocking_inner_partition_not_7:" +
        std::to_string(result.blockingInner));
    return result;
}

std::string canonicalTopology(const PaletteResult& result)
{
    std::ostringstream out;
    for (const auto& item : result.ledger) {
        if (!item.second.priorActualRouteBody) continue;
        out << item.first.first << ',' << item.first.second << '|'
            << item.second.priorActualPrimitive << '|'
            << item.second.finalPredictedPrimitive << '|'
            << name(item.second.classification.fragment) << '|'
            << name(item.second.classification.interval) << '|'
            << item.second.classification.protectedBodyOrLateral << '|'
            << item.second.classification.futureGapCandidate << '\n';
    }
    return out.str();
}

void verifyGapFeasibility(const PaletteResult& result)
{
    const std::array<std::size_t, 3> exactEligible{{18, 1, 0}};
    std::size_t gapIndex = 0;
    for (const auto& gap : kGaps) {
        std::set<PixelKey> eligible;
        std::set<PixelKey> protectedPixels;
        for (const auto& item : result.ledger) {
            if (std::abs(item.first.first - gap[0]) > kRadius ||
                    std::abs(item.first.second - gap[1]) > kRadius ||
                    !item.second.priorActualRouteBody) continue;
            if (item.second.finalPredictedPrimitive.empty())
                eligible.insert(item.first);
            else if (item.second.classification.futureGapCandidate)
                eligible.insert(item.first);
            else protectedPixels.insert(item.first);
        }
        std::size_t largest = 0;
        std::set<PixelKey> remaining = eligible;
        while (!remaining.empty()) {
            std::vector<PixelKey> stack{*remaining.begin()};
            remaining.erase(remaining.begin());
            std::size_t size = 0;
            while (!stack.empty()) {
                const auto point = stack.back(); stack.pop_back(); ++size;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (!dx && !dy) continue;
                        const PixelKey next{point.first + dx, point.second + dy};
                        const auto found = remaining.find(next);
                        if (found != remaining.end()) {
                            stack.push_back(*found); remaining.erase(found);
                        }
                    }
            }
            largest = std::max(largest, size);
        }
        std::cerr << "V15_GAP palette=" << result.palette << " x=" << gap[0]
            << " y=" << gap[1] << " eligible=" << eligible.size()
            << " protected=" << protectedPixels.size()
            << " largest8=" << largest << '\n';
        expect(eligible.size() == exactEligible[gapIndex],
            "eligible_gap_partition_drift:" + result.palette + ":" +
                std::to_string(gap[0]) + "," + std::to_string(gap[1]));
        expect(largest == exactEligible[gapIndex],
            "eligible_gap_topology_drift:" + result.palette + ":" +
                std::to_string(gap[0]) + "," + std::to_string(gap[1]));
        ++gapIndex;
    }
}

void emit(const PaletteResult& standard, const PaletteResult& cvd)
{
    std::cout << "{\"schema\":\"billiardgl.phase4.v15.broadcast_overwrite_provenance.v1\",";
    std::cout << "\"capture_id\":\"" << kCapture << "\",";
    std::cout << "\"ordered_source_count\":81,\"ordered_source_digest\":\""
        << kDigest << "\",\"palettes\":[";
    const std::array<const PaletteResult*, 2> results{{&standard, &cvd}};
    for (std::size_t p = 0; p < results.size(); ++p) {
        if (p) std::cout << ',';
        const auto& result = *results[p];
        std::cout << "{\"palette\":\"" << result.palette
            << "\",\"blocking_predicted_core\":" << result.blockingCore
            << ",\"blocking_inner_body\":" << result.blockingInner
            << ",\"samples\":[";
        bool first = true;
        for (const auto& item : result.ledger) {
            const auto& sample = item.second;
            if (!sample.priorActualRouteBody) continue;
            if (!first) std::cout << ',';
            first = false;
            std::cout << "{\"palette\":\"" << result.palette
                << "\",\"physical_pixel\":[" << sample.x << ',' << sample.y
                << "],\"sample_index_and_position\":{\"index\":0,\"position\":[0.5,0.5]},"
                << "\"prior_actual_source_probe_id\":\""
                << escape(sample.priorActualSource)
                << "\",\"prior_actual_primitive_id\":\""
                << escape(sample.priorActualPrimitive)
                << "\",\"prior_actual_route_body_boolean\":true,"
                << "\"final_predicted_source_probe_id\":\""
                << escape(sample.finalPredictedSource)
                << "\",\"final_predicted_primitive_id\":\""
                << escape(sample.finalPredictedPrimitive)
                << "\",\"predicted_fragment_class\":\""
                << name(sample.classification.fragment)
                << "\",\"semantic_interval_class\":\""
                << name(sample.classification.interval)
                << "\",\"dash_ordinal_and_local_tangent_parameter\":{\"ordinal\":"
                << sample.classification.dashOrdinal << ",\"t\":"
                << std::setprecision(17) << sample.classification.localTangent
                << "},\"is_protected_predicted_body_or_lateral\":"
                << (sample.classification.protectedBodyOrLateral ? "true" : "false")
                << ",\"would_be_candidate_for_future_gap_only_non_overwrite\":"
                << (sample.classification.futureGapCandidate ? "true" : "false")
                << '}';
        }
        std::cout << "],\"repair_probe_owner_chains\":[";
        bool firstRepair = true;
        for (const char* repairId : kRepairIds) {
            const auto coordinate = result.repairCoordinates.find(repairId);
            expect(coordinate != result.repairCoordinates.end(),
                std::string("repair_coordinate_missing:") + repairId);
            const auto ledger = result.ledger.find(coordinate->second);
            expect(ledger != result.ledger.end(),
                std::string("repair_ledger_sample_missing:") + repairId);
            const auto& sample = ledger->second;
            const auto source = result.repairSources.find(repairId);
            expect(source != result.repairSources.end(),
                std::string("repair_source_missing:") + repairId);
            const char* finalOwnerKind = !sample.finalPredictedPrimitive.empty() ?
                "predicted" : sample.priorActualRouteBody ?
                    "actual_route_body" : "scene_or_background";
                if (!firstRepair) std::cout << ',';
                firstRepair = false;
                std::cout << "{\"repair_probe_id\":\"" << repairId
                    << "\",\"physical_pixel\":[" << sample.x << ','
                    << sample.y << "],\"repair_source_track\":\""
                    << source->second.track
                    << "\",\"repair_source_component_role\":\""
                    << source->second.componentRole
                    << "\",\"repair_source_primitive_id\":\""
                    << escape(source->second.primitiveId)
                    << "\",\"repair_source_arc_segment_id\":\""
                    << escape(source->second.arcSegmentId)
                    << "\",\"final_owner_kind\":\"" << finalOwnerKind
                    << "\",\"prior_actual_source_probe_id\":\""
                    << escape(sample.priorActualSource)
                    << "\",\"prior_actual_primitive_id\":\""
                    << escape(sample.priorActualPrimitive)
                    << "\",\"final_predicted_source_probe_id\":\""
                    << escape(sample.finalPredictedSource)
                    << "\",\"final_predicted_primitive_id\":\""
                    << escape(sample.finalPredictedPrimitive)
                    << "\",\"predicted_fragment_class\":\""
                    << name(sample.classification.fragment)
                    << "\",\"semantic_interval_class\":\""
                    << name(sample.classification.interval) << "\"}";
        }
        std::cout << "],\"foreign_evidence_count\":0,"
            << "\"double_assignment_count\":0,"
            << "\"unowned_evidence_count\":0}";
    }
    std::cout << "]}\n";
}

}  // namespace v15

int main()
{
    try {
        v15::testCrossClassificationContract();
        billiardgl::GameRuntime runtime;
        const auto snapshot = frozenComparisonSnapshot(runtime);
        const auto standard = v15::evaluatePalette(snapshot, runtime,
            billiardgl::training::TrajectoryOverlayPalette::Standard,
            "standard");
        const auto cvd = v15::evaluatePalette(snapshot, runtime,
            billiardgl::training::TrajectoryOverlayPalette::ColorVisionSafe,
            "color_vision_safe");
        expect(v15::canonicalTopology(standard) == v15::canonicalTopology(cvd),
            "standard_cvd_topology_drift");
        v15::verifyGapFeasibility(standard);
        v15::verifyGapFeasibility(cvd);
        v15::emit(standard, cvd);
        std::cerr << "PHASE4_V15_PROVENANCE_DIAGNOSTIC_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PHASE4_V15_PROVENANCE_DIAGNOSTIC_FAIL "
            << error.what() << '\n';
        return 1;
    }
}
