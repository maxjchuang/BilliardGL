#include "training/trajectory_inset_renderer.h"

#include "deterministic_hash.h"
#include "table_specs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>

namespace billiardgl::training {
namespace {

constexpr float kActualRailOffsetPx = 6.0f;
constexpr float kActualCoreWidthPx = 2.5f;
constexpr float kActualHaloWidthPx = 6.5f;
constexpr float kPredictedCoreWidthPx = 3.0f;
constexpr float kPredictedInnerWidthPx = 7.0f;
constexpr float kPredictedOuterWidthPx = 10.0f;
constexpr float kDashOnPx = 14.0f;
constexpr float kDashOffPx = 8.0f;
constexpr std::uint32_t kFirstInsetToken = 0x80000001u;

struct P2 { float x = 0.0f; float y = 0.0f; };

bool finite(P2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
P2 add(P2 a, P2 b) { return {a.x + b.x, a.y + b.y}; }
P2 sub(P2 a, P2 b) { return {a.x - b.x, a.y - b.y}; }
P2 mul(P2 a, float scale) { return {a.x * scale, a.y * scale}; }
float length(P2 value) { return std::sqrt(value.x * value.x + value.y * value.y); }

std::string canonicalDoubleBits(double value)
{
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::hex << std::setfill('0') << std::setw(16) << bits;
    return out.str();
}

std::string joinedDigest(const std::vector<std::string>& values)
{
    std::string bytes;
    for (const std::string& value : values) {
        bytes += std::to_string(value.size()) + ":" + value + "\n";
    }
    return sha256Hex(bytes);
}

TrajectoryInsetRect canonicalTableRect(const TrajectoryInsetLayout& layout)
{
    const float minimumGutter = 12.0f * layout.uiScale;
    const float availableWidth = layout.plotRect.width - 2.0f * minimumGutter;
    const float availableHeight = layout.plotRect.height - 2.0f * minimumGutter;
    const float scale = std::min(availableWidth / kChinesePlayfieldLengthCm,
        availableHeight / kChinesePlayfieldWidthCm);
    const float width = kChinesePlayfieldLengthCm * scale;
    const float height = kChinesePlayfieldWidthCm * scale;
    return {layout.plotRect.x + (layout.plotRect.width - width) * 0.5f,
        layout.plotRect.y + (layout.plotRect.height - height) * 0.5f,
        width, height};
}

P2 mapTablePoint(const TrajectoryInsetRect& table, const Point3& point)
{
    // Corrected V19 landscape orientation: canonical z is horizontal with the
    // head rail (-z) at left; canonical x is vertical with -x at top.
    return {table.x + (point.z / kChinesePlayfieldLengthCm + 0.5f) * table.width,
        table.y + (point.x / kChinesePlayfieldWidthCm + 0.5f) * table.height};
}

bool insideTable(const Point3& point)
{
    return std::isfinite(point.x) && std::isfinite(point.z) &&
        std::abs(point.x) <= kChinesePlayfieldWidthCm * 0.5f + 1e-4f &&
        std::abs(point.z) <= kChinesePlayfieldLengthCm * 0.5f + 1e-4f;
}

bool clipTest(float p, float q, float& low, float& high)
{
    if (std::abs(p) < 1e-8f) return q >= 0.0f;
    const float ratio = q / p;
    if (p < 0.0f) {
        if (ratio > high) return false;
        low = std::max(low, ratio);
    } else {
        if (ratio < low) return false;
        high = std::min(high, ratio);
    }
    return true;
}

bool clipToPlaySurface(const Point3& first, const Point3& second,
    Point3& clippedFirst, Point3& clippedSecond)
{
    if (!std::isfinite(first.x) || !std::isfinite(first.z) ||
        !std::isfinite(second.x) || !std::isfinite(second.z)) return false;
    float low = 0.0f, high = 1.0f;
    const float dx = second.x - first.x;
    const float dz = second.z - first.z;
    const float halfWidth = kChinesePlayfieldWidthCm * 0.5f;
    const float halfLength = kChinesePlayfieldLengthCm * 0.5f;
    if (!clipTest(-dx, first.x + halfWidth, low, high) ||
        !clipTest(dx, halfWidth - first.x, low, high) ||
        !clipTest(-dz, first.z + halfLength, low, high) ||
        !clipTest(dz, halfLength - first.z, low, high) || low > high)
        return false;
    clippedFirst = first;
    clippedSecond = second;
    clippedFirst.x = first.x + dx * low;
    clippedFirst.z = first.z + dz * low;
    clippedSecond.x = first.x + dx * high;
    clippedSecond.z = first.z + dz * high;
    return insideTable(clippedFirst) && insideTable(clippedSecond);
}

std::array<float, 4> color(TrajectoryInsetPalette palette,
    TrajectoryInsetTrack track, TrajectoryInsetRole role)
{
    const bool actual = track == TrajectoryInsetTrack::Actual;
    const bool halo = role == TrajectoryInsetRole::ActualLeftHalo ||
        role == TrajectoryInsetRole::ActualRightHalo ||
        role == TrajectoryInsetRole::PredictedInnerHalo ||
        role == TrajectoryInsetRole::PredictedEventVisitMarkerHalo;
    const bool outer = role == TrajectoryInsetRole::PredictedOuterRim;
    if (halo) return actual ? std::array<float, 4>{{1, 1, 1, 1}} :
        std::array<float, 4>{{0.01f, 0.01f, 0.012f, 1}};
    if (outer) return {{0.01f, 0.01f, 0.012f, 1}};
    if (actual) return palette == TrajectoryInsetPalette::Standard ?
        std::array<float, 4>{{0.01f, 0.28f, 0.55f, 1}} :
        std::array<float, 4>{{0.05f, 0.22f, 0.80f, 1}};
    return palette == TrajectoryInsetPalette::Standard ?
        std::array<float, 4>{{1.0f, 0.78f, 0.08f, 1}} :
        std::array<float, 4>{{1.0f, 0.40f, 0.02f, 1}};
}

void appendTriangle(TrajectoryInsetDrawPackage& package, P2 a, P2 b, P2 c,
    const std::array<float, 4>& rgba, std::uint32_t token)
{
    for (P2 p : {a, b, c}) package.vertices.push_back(
        {p.x, p.y, rgba, token});
}

void appendSegment(TrajectoryInsetDrawPackage& package, P2 first, P2 second,
    float width, const std::array<float, 4>& rgba, std::uint32_t token)
{
    const P2 direction = sub(second, first);
    const float extent = length(direction);
    if (extent <= 1e-4f) return;
    const P2 normal{-direction.y / extent * width * 0.5f,
        direction.x / extent * width * 0.5f};
    const P2 a = add(first, normal), b = sub(first, normal);
    const P2 c = sub(second, normal), d = add(second, normal);
    appendTriangle(package, a, b, c, rgba, token);
    appendTriangle(package, a, c, d, rgba, token);
}

void appendDiamond(TrajectoryInsetDrawPackage& package, P2 center, float radius,
    const std::array<float, 4>& rgba, std::uint32_t token)
{
    const P2 top{center.x, center.y - radius};
    const P2 right{center.x + radius, center.y};
    const P2 bottom{center.x, center.y + radius};
    const P2 left{center.x - radius, center.y};
    appendTriangle(package, top, right, bottom, rgba, token);
    appendTriangle(package, top, bottom, left, rgba, token);
}

void appendDiamondBand(TrajectoryInsetDrawPackage& package, P2 center,
    float outerRadius, float innerRadius,
    const std::array<float, 4>& rgba, std::uint32_t token)
{
    const std::array<P2, 4> outer{{
        {center.x, center.y - outerRadius},
        {center.x + outerRadius, center.y},
        {center.x, center.y + outerRadius},
        {center.x - outerRadius, center.y}}};
    const std::array<P2, 4> inner{{
        {center.x, center.y - innerRadius},
        {center.x + innerRadius, center.y},
        {center.x, center.y + innerRadius},
        {center.x - innerRadius, center.y}}};
    for (std::size_t side = 0; side < outer.size(); ++side) {
        const std::size_t next = (side + 1) % outer.size();
        appendTriangle(package, outer[side], outer[next], inner[next], rgba,
            token);
        appendTriangle(package, outer[side], inner[next], inner[side], rgba,
            token);
    }
}

void appendDisc(TrajectoryInsetDrawPackage& package, P2 center, float radius,
    const std::array<float, 4>& rgba, std::uint32_t token)
{
    constexpr int kSlices = 12;
    constexpr float kTau = 6.2831853071795864769f;
    for (int slice = 0; slice < kSlices; ++slice) {
        const float a0 = kTau * slice / kSlices;
        const float a1 = kTau * (slice + 1) / kSlices;
        appendTriangle(package, center,
            {center.x + std::cos(a0) * radius,
                center.y + std::sin(a0) * radius},
            {center.x + std::cos(a1) * radius,
                center.y + std::sin(a1) * radius}, rgba, token);
    }
}

std::string arcId(TrajectoryInsetTrack track, const OverlaySample& first,
    const OverlaySample& second, std::size_t ordinal)
{
    (void)first;
    (void)second;
    // This is deliberately the same canonical analytic arc spelling used by
    // the main-world semantic batches. Ticks remain source evidence, not arc
    // identity.
    return std::string(trajectoryInsetTrackName(track)) + "_arc_" +
        std::to_string(ordinal);
}

std::string parentKey(const TrajectoryInsetBuildRequest& request,
    TrajectoryInsetTrack track, const std::string& arc, std::size_t ordinal,
    int collisionLineage)
{
    return request.generationId + "|" + trajectoryInsetTrackName(track) +
        "|" + arc + "|route_" + std::to_string(ordinal) + "|rail_events_" +
        std::to_string(std::max(0, collisionLineage));
}

struct Builder {
    struct ProbeRequest {
        std::string parent;
        std::string member;
        std::string companionMember;
        std::uint32_t token = 0;
        InsetTrajectoryProbe evidence;
    };
    TrajectoryInsetDrawPackage package;
    std::map<std::string, std::size_t> mappingIndex;
    std::map<std::string, TrajectoryInsetBuildRequest::WorldMappingInput>
        worldInputs;
    std::set<std::string> consumedWorldInputs;
    std::vector<ProbeRequest> probeRequests;
    std::set<std::uint32_t> forbidden;
    std::uint32_t nextToken = kFirstInsetToken;

    TrajectoryInsetSourceMappingRow& row(const std::string& parent,
        const std::string& worldReason)
    {
        const auto existing = mappingIndex.find(parent);
        if (existing != mappingIndex.end())
            return package.mappingRows[existing->second];
        TrajectoryInsetSourceMappingRow value;
        value.parentSourceKey = parent;
        const auto supplied = worldInputs.find(parent);
        if (supplied == worldInputs.end()) {
            if (!worldInputs.empty())
                package.errorCode = "trajectory_inset_world_mapping_partial";
            else
                value.worldNotEmittedReason = worldReason;
        } else {
            value.orderedWorldMemberKeys = supplied->second.orderedWorldMemberKeys;
            value.worldNotEmittedReason = supplied->second.worldNotEmittedReason;
            consumedWorldInputs.insert(parent);
        }
        mappingIndex[parent] = package.mappingRows.size();
        package.mappingRows.push_back(value);
        package.orderedParentSourceKeys.push_back(parent);
        return package.mappingRows.back();
    }

    std::uint32_t token()
    {
        while (nextToken == 0 || forbidden.count(nextToken) != 0) ++nextToken;
        return nextToken++;
    }

    std::string batch(const std::string& parent, const std::string& arc,
        TrajectoryInsetTrack track, TrajectoryInsetRole role,
        const std::string& primitive, std::size_t firstVertex,
        std::uint32_t ownership, const std::string& worldReason)
    {
        const std::size_t count = package.vertices.size() - firstVertex;
        if (count == 0 || ownership == 0) {
            package.errorCode = "trajectory_inset_semantic_batch_empty";
            return {};
        }
        const std::string member = parent + "|inset_v1|" + primitive + "|" +
            trajectoryInsetRoleName(role) + "|" + std::to_string(ownership);
        package.semanticBatches.push_back({track, role, parent, primitive, arc,
            member, firstVertex, count, ownership});
        row(parent, worldReason).orderedInsetMemberKeys.push_back(member);
        ++package.semanticMemberDenominator;
        return member;
    }

    std::string eventBatch(const std::string& eventKey,
        TrajectoryInsetRole role, const std::string& primitive,
        std::size_t firstVertex, std::uint32_t ownership)
    {
        const std::size_t count = package.vertices.size() - firstVertex;
        if (count == 0 || ownership == 0) {
            package.errorCode = "trajectory_inset_semantic_batch_empty";
            return {};
        }
        const std::string member = eventKey + "|inset_v1|" + primitive + "|" +
            trajectoryInsetRoleName(role) + "|" + std::to_string(ownership);
        package.semanticBatches.push_back({TrajectoryInsetTrack::Predicted,
            role, eventKey, primitive, eventKey, member, firstVertex, count,
            ownership});
        ++package.semanticMemberDenominator;
        return member;
    }

    void probe(const std::string& parent, const std::string& exactMemberKey,
        const std::string& companionMemberKey, std::uint32_t ownership,
        P2, P2, P2)
    {
        probeRequests.push_back({parent, exactMemberKey, companionMemberKey,
            ownership});
    }
};

bool validateRequest(const TrajectoryInsetBuildRequest& request,
    std::string& error)
{
    const auto& layout = request.layout;
    if (request.generationId.empty()) error = "trajectory_inset_generation_missing";
    else if (layout.profileId.empty() || layout.physicalWidth <= 0 ||
        layout.physicalHeight <= 0 || !std::isfinite(layout.uiScale) ||
        layout.uiScale < 0.85f || layout.uiScale > 1.35f)
        error = "trajectory_inset_layout_invalid";
    else if (layout.panelRect.width < 336.0f || layout.panelRect.height < 216.0f ||
        layout.plotRect.width <= 0.0f || layout.plotRect.height <= 0.0f ||
        std::abs(layout.plotRect.width / layout.plotRect.height - 2.0f) > 1e-4f)
        error = "trajectory_inset_layout_below_contract";
    else if (layout.panelRect.x < 0 || layout.panelRect.y < 0 ||
        layout.panelRect.x + layout.panelRect.width > layout.physicalWidth ||
        layout.panelRect.y + layout.panelRect.height > layout.physicalHeight ||
        layout.plotRect.x < layout.panelRect.x || layout.plotRect.y < layout.panelRect.y ||
        layout.plotRect.x + layout.plotRect.width > layout.panelRect.x + layout.panelRect.width ||
        layout.plotRect.y + layout.plotRect.height > layout.panelRect.y + layout.panelRect.height)
        error = "trajectory_inset_layout_out_of_bounds";
    else if (request.worldMappings.empty() && request.worldNotEmittedReason.empty())
        error = "trajectory_inset_world_mapping_missing";
    return error.empty();
}

void buildActual(Builder& builder, const TrajectoryInsetBuildRequest& request)
{
    const OverlayTrack& track = request.snapshot.actual;
    if (!track.visible) return;
    std::string lastParent, lastArc;
    P2 lastCenter, lastDirection;
    P2 previousLeft, previousRight;
    bool previousConnected = false;
    const P2 background{request.layout.plotRect.x + 0.5f,
        request.layout.plotRect.y + 0.5f};
    struct Emitted { std::uint32_t token = 0; std::string member; };
    constexpr float kPredictedEndpointCompleteNormalSupportPx = 4.5f;
    constexpr float kCapsuleHalfThicknessPx = 1.25f;
    constexpr float kEndpointSeparationPx = 2.0f;
    constexpr float kEndpointRailOffsetPx =
        kPredictedEndpointCompleteNormalSupportPx +
        kCapsuleHalfThicknessPx + kEndpointSeparationPx;
    std::size_t lastEligibleSegment = 0;
    std::vector<float> segmentExtents(track.samples.size(), 0.0f);
    std::vector<bool> segmentEligible(track.samples.size(), false);
    struct ActualSegment {
        std::string parent;
        std::string arc;
        P2 a;
        P2 b;
        P2 leftA;
        P2 leftB;
        P2 rightA;
        P2 rightB;
        P2 previousLeft;
        P2 previousRight;
        P2 direction;
        bool connectsPrevious = false;
    };
    std::vector<ActualSegment> segments;
    for (std::size_t i = 1; i < track.samples.size(); ++i) {
        Point3 clippedA, clippedB;
        if (track.samples[i - 1].visible && track.samples[i].visible &&
            track.samples[i].tick > track.samples[i - 1].tick &&
            clipToPlaySurface(track.samples[i - 1].position,
                track.samples[i].position, clippedA, clippedB)) {
            const P2 a = mapTablePoint(builder.package.canonicalTableRect,
                clippedA);
            const P2 b = mapTablePoint(builder.package.canonicalTableRect,
                clippedB);
            segmentExtents[i] = length(sub(b, a));
            segmentEligible[i] = segmentExtents[i] > 1e-4f;
            lastEligibleSegment = i;
        }
    }
    std::vector<float> remainingAfter(track.samples.size(),
        std::numeric_limits<float>::infinity());
    float remaining = 0.0f;
    for (std::size_t i = lastEligibleSegment; i > 0; --i) {
        if (!segmentEligible[i]) break;
        remainingAfter[i] = remaining;
        remaining += segmentExtents[i];
    }
    const auto railOffset = [&](float distanceToEndpoint) {
        if (!track.endpointVisible || !std::isfinite(distanceToEndpoint))
            return kActualRailOffsetPx;
        const float progress = 1.0f - std::clamp(distanceToEndpoint /
            kPredictedEndpointCompleteNormalSupportPx, 0.0f, 1.0f);
        return kActualRailOffsetPx +
            (kEndpointRailOffsetPx - kActualRailOffsetPx) * progress;
    };
    for (std::size_t i = 1; i < track.samples.size(); ++i) {
        const OverlaySample& aSource = track.samples[i - 1];
        const OverlaySample& bSource = track.samples[i];
        if (!aSource.visible || !bSource.visible || bSource.tick <= aSource.tick) {
            previousConnected = false;
            continue;
        }
        Point3 clippedA, clippedB;
        if (!clipToPlaySurface(aSource.position, bSource.position,
                clippedA, clippedB)) {
            previousConnected = false;
            continue;
        }
        const P2 a = mapTablePoint(builder.package.canonicalTableRect, clippedA);
        const P2 b = mapTablePoint(builder.package.canonicalTableRect, clippedB);
        const P2 delta = sub(b, a);
        const float extent = length(delta);
        if (extent <= 1e-4f) {
            previousConnected = false;
            continue;
        }
        const P2 normal{-delta.y / extent, delta.x / extent};
        const P2 direction = mul(delta, 1.0f / extent);
        const float endOffset = railOffset(remainingAfter[i]);
        const float startOffset = railOffset(remainingAfter[i] + extent);
        const P2 leftA = add(a, mul(normal, startOffset));
        const P2 leftB = add(b, mul(normal, endOffset));
        const P2 rightA = sub(a, mul(normal, startOffset));
        const P2 rightB = sub(b, mul(normal, endOffset));
        const std::string arc = arcId(TrajectoryInsetTrack::Actual,
            aSource, bSource, i - 1);
        const std::string parent = parentKey(request,
            TrajectoryInsetTrack::Actual, arc, i - 1, track.railCollisionEvents);
        builder.row(parent, request.worldNotEmittedReason);
        builder.package.actualMidpointWitnesses.push_back({parent,
            {{a.x, a.y}}, {{b.x, b.y}}, {{leftA.x, leftA.y}},
            {{leftB.x, leftB.y}}, {{rightA.x, rightA.y}},
            {{rightB.x, rightB.y}}});

        segments.push_back({parent, arc, a, b, leftA, leftB, rightA, rightB,
            previousLeft, previousRight, direction, previousConnected});
        lastParent = parent; lastArc = arc; lastCenter = b;
        lastDirection = direction;
        previousLeft = leftB;
        previousRight = rightB;
        previousConnected = true;
    }
    struct SegmentMembers {
        Emitted leftHalo;
        Emitted rightHalo;
        Emitted leftCore;
        Emitted rightCore;
    };
    std::vector<SegmentMembers> members(segments.size());
    const auto emit = [&](const ActualSegment& segment,
        TrajectoryInsetRole role, P2 first, P2 second, P2 previous,
        float width, const char* suffix) {
        Emitted emitted;
        const std::uint32_t ownership = builder.token();
        const std::size_t begin = builder.package.vertices.size();
        const auto rgba = color(request.palette,
            TrajectoryInsetTrack::Actual, role);
        if (segment.connectsPrevious) {
            appendSegment(builder.package, previous, first, width, rgba,
                ownership);
            appendDisc(builder.package, previous, width * 0.5f, rgba,
                ownership);
        }
        appendSegment(builder.package, first, second, width, rgba, ownership);
        appendDisc(builder.package, first, width * 0.5f, rgba, ownership);
        appendDisc(builder.package, second, width * 0.5f, rgba, ownership);
        const std::string primitive = segment.arc + "_" + suffix;
        emitted.member = builder.batch(segment.parent, segment.arc,
            TrajectoryInsetTrack::Actual, role, primitive, begin, ownership,
            request.worldNotEmittedReason);
        emitted.token = ownership;
        return emitted;
    };
    // Complete the whole-route halo layer before any core is submitted. This
    // preserves each core through source joins without changing rail widths,
    // identities, or the predicted-last product grammar.
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const ActualSegment& segment = segments[i];
        members[i].leftHalo = emit(segment,
            TrajectoryInsetRole::ActualLeftHalo, segment.leftA, segment.leftB,
            segment.previousLeft, kActualHaloWidthPx, "left_halo");
        members[i].rightHalo = emit(segment,
            TrajectoryInsetRole::ActualRightHalo, segment.rightA, segment.rightB,
            segment.previousRight, kActualHaloWidthPx, "right_halo");
    }
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const ActualSegment& segment = segments[i];
        members[i].leftCore = emit(segment,
            TrajectoryInsetRole::ActualLeftCore, segment.leftA, segment.leftB,
            segment.previousLeft, kActualCoreWidthPx, "left_core");
        members[i].rightCore = emit(segment,
            TrajectoryInsetRole::ActualRightCore, segment.rightA, segment.rightB,
            segment.previousRight, kActualCoreWidthPx, "right_core");
        builder.probe(segment.parent, members[i].leftCore.member,
            members[i].leftHalo.member, members[i].leftCore.token,
            {}, {}, background);
        builder.probe(segment.parent, members[i].rightCore.member,
            members[i].rightHalo.member, members[i].rightCore.token,
            {}, {}, background);
    }
    if (!lastParent.empty() && track.endpointVisible) {
        const P2 normal{-lastDirection.y, lastDirection.x};
        struct Marker {
            P2 center;
            P2 stroke;
            std::uint32_t token;
            std::string member;
        };
        std::array<Marker, 2> capsules;
        std::size_t capsuleIndex = 0;
        for (int side : {-1, 1}) {
            const P2 center = add(lastCenter,
                mul(normal, static_cast<float>(side) * kEndpointRailOffsetPx));
            const std::uint32_t ownership = builder.token();
            const std::size_t begin = builder.package.vertices.size();
            constexpr float kCapsuleTangentHalfSegmentPx = 2.25f;
            const P2 capsuleFirst = sub(center, mul(lastDirection,
                kCapsuleTangentHalfSegmentPx));
            const P2 capsuleSecond = add(center, mul(lastDirection,
                kCapsuleTangentHalfSegmentPx));
            const auto capsuleColor = color(request.palette,
                TrajectoryInsetTrack::Actual,
                TrajectoryInsetRole::ActualEndpointCapsule);
            appendSegment(builder.package, capsuleFirst, capsuleSecond, 2.5f,
                capsuleColor, ownership);
            appendDisc(builder.package, capsuleFirst, 1.25f, capsuleColor,
                ownership);
            appendDisc(builder.package, capsuleSecond, 1.25f, capsuleColor,
                ownership);
            const std::string primitive = lastArc + (side < 0 ?
                "_right_endpoint_capsule" : "_left_endpoint_capsule");
            const std::string member = builder.batch(lastParent, lastArc,
                TrajectoryInsetTrack::Actual,
                TrajectoryInsetRole::ActualEndpointCapsule, primitive, begin,
                ownership, request.worldNotEmittedReason);
            capsules[capsuleIndex++] = {center,
                add(center, mul(normal, static_cast<float>(side) * 2.0f)),
                ownership, member};
        }
        builder.probe(lastParent, capsules[0].member, capsules[1].member,
            capsules[0].token, capsules[0].stroke, capsules[1].stroke,
            background);
        builder.probe(lastParent, capsules[1].member, capsules[0].member,
            capsules[1].token, capsules[1].stroke, capsules[0].stroke,
            background);
        std::string terminalOrderError;
        if(!applyActualTerminalOwnershipOrder(builder.package,lastParent,terminalOrderError)){
            builder.package.errorCode=terminalOrderError;
            return;
        }
        const std::uint32_t ownership = builder.token();
        const std::size_t begin = builder.package.vertices.size();
        const P2 base = add(lastCenter, mul(lastDirection, 10.0f));
        const P2 tip = add(lastCenter, mul(lastDirection, 16.0f));
        appendTriangle(builder.package, tip,
            add(base, mul(normal, 3.0f)),
            sub(base, mul(normal, 3.0f)),
            color(request.palette, TrajectoryInsetTrack::Actual,
                TrajectoryInsetRole::ActualDirectionNotch), ownership);
        const std::string primitive = lastArc + "_direction_notch";
        const std::string member = builder.batch(lastParent, lastArc,
            TrajectoryInsetTrack::Actual,
            TrajectoryInsetRole::ActualDirectionNotch, primitive, begin,
            ownership, request.worldNotEmittedReason);
        builder.probe(lastParent, member,
            capsules[0].member, ownership,
            add(lastCenter, mul(lastDirection, 12.0f)), capsules[0].stroke,
            background);
    }
}

void buildPredicted(Builder& builder, const TrajectoryInsetBuildRequest& request)
{
    const OverlayTrack& track = request.snapshot.predicted;
    if (!track.visible) return;
    std::string lastParent, lastArc;
    P2 lastCenter;
    const P2 background{request.layout.plotRect.x + 0.5f,
        request.layout.plotRect.y + 0.5f};
    struct Emitted { std::uint32_t token = 0; std::string member; };
    struct SourceSegment {
        std::string parent;
        std::string arc;
        std::size_t routeOrderOrdinal = 0;
        std::size_t routeSpanOrdinal = 0;
        P2 first;
        P2 second;
        double extent = 0.0;
        double routeStart = 0.0;
        double routeEnd = 0.0;
    };
    struct EventGeometry {
        TrajectoryInsetDashEvent event;
        std::vector<std::array<P2, 2>> orderedPieces;
    };
    std::vector<SourceSegment> sources;
    std::vector<double> spanLengths;
    double routeDistance = 0.0;
    std::size_t routeSpanOrdinal = 0;
    bool previousEligible = false;
    P2 previousEnd;
    for (std::size_t i = 1; i < track.samples.size(); ++i) {
        const OverlaySample& aSource = track.samples[i - 1];
        const OverlaySample& bSource = track.samples[i];
        if (!aSource.visible || !bSource.visible || bSource.tick <= aSource.tick) {
            previousEligible = false;
            continue;
        }
        Point3 clippedA, clippedB;
        if (!clipToPlaySurface(aSource.position, bSource.position,
                clippedA, clippedB)) {
            previousEligible = false;
            continue;
        }
        const P2 a = mapTablePoint(builder.package.canonicalTableRect, clippedA);
        const P2 b = mapTablePoint(builder.package.canonicalTableRect, clippedB);
        const double extent = static_cast<double>(length(sub(b, a)));
        if (extent <= 1e-4) {
            previousEligible = false;
            continue;
        }
        const bool continuous = previousEligible &&
            length(sub(a, previousEnd)) <= 1e-4f;
        if (!continuous) {
            if (!sources.empty()) ++routeSpanOrdinal;
            routeDistance = 0.0;
            spanLengths.push_back(0.0);
        }
        const std::string arc = arcId(TrajectoryInsetTrack::Predicted,
            aSource, bSource, i - 1);
        const std::string parent = parentKey(request,
            TrajectoryInsetTrack::Predicted, arc, i - 1,
            track.railCollisionEvents);
        builder.row(parent, request.worldNotEmittedReason);
        sources.push_back({parent, arc, i - 1, routeSpanOrdinal, a, b, extent,
            routeDistance, routeDistance + extent});
        routeDistance += extent;
        spanLengths.at(routeSpanOrdinal) = routeDistance;
        previousEnd = b;
        previousEligible = true;
        lastParent = parent; lastArc = arc; lastCenter = b;
    }
    builder.package.predictedRouteSpanCount = spanLengths.size();
    std::vector<EventGeometry> events;
    std::map<std::pair<std::size_t, std::size_t>, std::size_t> cycleToEvent;
    constexpr double kCycle = kDashOnPx + kDashOffPx;
    constexpr double kIntervalEpsilon = 1e-8;
    for (const SourceSegment& source : sources) {
        auto& row = builder.row(source.parent, request.worldNotEmittedReason);
        double cursor = source.routeStart;
        const double cadenceEnd = source.routeEnd;
        while (cursor < cadenceEnd - kIntervalEpsilon) {
            const std::size_t cycle = static_cast<std::size_t>(
                std::floor((cursor + kIntervalEpsilon) / kCycle));
            const double cycleStart = cycle * kCycle;
            const double dashEnd = cycleStart + kDashOnPx;
            const bool dashOn = cursor < dashEnd - kIntervalEpsilon;
            const double intervalEnd = std::min(cadenceEnd,
                dashOn ? dashEnd : cycleStart + kCycle);
            const double localStart = (cursor - source.routeStart) /
                source.extent;
            const double localEnd = (intervalEnd - source.routeStart) /
                source.extent;
            TrajectoryInsetSourceMappingRow::PredictedRouteInterval interval;
            interval.arcSegmentId = source.arc;
            interval.routeOrderOrdinal = source.routeOrderOrdinal;
            interval.routeSpanOrdinal = source.routeSpanOrdinal;
            interval.sourceLocalTStart = std::clamp(localStart, 0.0, 1.0);
            interval.sourceLocalTEnd = std::clamp(localEnd, 0.0, 1.0);
            interval.routeDistanceStartPx = cursor;
            interval.routeDistanceEndPx = intervalEnd;
            if (dashOn) {
                const auto cycleKey = std::make_pair(source.routeSpanOrdinal,
                    cycle);
                auto existing = cycleToEvent.find(cycleKey);
                if (existing == cycleToEvent.end()) {
                    EventGeometry geometry;
                    geometry.event.routeWideDashEventOrdinal = events.size();
                    geometry.event.routeSpanOrdinal = source.routeSpanOrdinal;
                    geometry.event.routeDistanceStartPx = cycleStart;
                    geometry.event.routeDistanceEndPx = std::min(
                        cycleStart + kDashOnPx,
                        spanLengths.at(source.routeSpanOrdinal));
                    const std::string eventIdentity = request.generationId +
                        "|predicted|route_wide_dash_event_" +
                        std::to_string(geometry.event.routeWideDashEventOrdinal) +
                        "|span_" + std::to_string(source.routeSpanOrdinal) +
                        "|" + canonicalDoubleBits(
                            geometry.event.routeDistanceStartPx) + "|" +
                        canonicalDoubleBits(
                            geometry.event.routeDistanceEndPx);
                    geometry.event.eventKey = eventIdentity + "|" +
                        sha256Hex(eventIdentity).substr(0, 16);
                    existing = cycleToEvent.emplace(cycleKey,
                        events.size()).first;
                    events.push_back(std::move(geometry));
                }
                EventGeometry& geometry = events[existing->second];
                interval.dashEventKey = geometry.event.eventKey;
                geometry.event.orderedContributors.push_back({source.parent,
                    source.arc, source.routeOrderOrdinal,
                    interval.sourceLocalTStart, interval.sourceLocalTEnd,
                    cursor, intervalEnd});
                const P2 delta = sub(source.second, source.first);
                geometry.orderedPieces.push_back({{
                    add(source.first, mul(delta,
                        static_cast<float>(interval.sourceLocalTStart))),
                    add(source.first, mul(delta,
                        static_cast<float>(interval.sourceLocalTEnd)))}});
            } else {
                interval.nonRenderedReason =
                    "predicted_dash_gap_by_route_wide_cadence";
            }
            row.orderedPredictedRouteIntervals.push_back(std::move(interval));
            cursor = intervalEnd;
        }
    }
    builder.package.predictedDashEvents.reserve(events.size());
    for (const auto& geometry : events)
        builder.package.predictedDashEvents.push_back(geometry.event);
    const auto emitEventLayer = [&](std::size_t eventIndex,
        TrajectoryInsetRole role, float width, const char* suffix,
        std::size_t roleIndex) {
        Emitted emitted;
        EventGeometry& geometry = events[eventIndex];
        const std::uint32_t ownership = builder.token();
        const std::size_t begin = builder.package.vertices.size();
        for (const auto& piece : geometry.orderedPieces)
            appendSegment(builder.package, piece[0], piece[1], width,
                color(request.palette, TrajectoryInsetTrack::Predicted, role),
                ownership);
        const std::string primitive = "route_wide_dash_event_" +
            std::to_string(geometry.event.routeWideDashEventOrdinal) + "_" +
            suffix;
        emitted.member = builder.eventBatch(geometry.event.eventKey, role,
            primitive, begin, ownership);
        emitted.token = ownership;
        builder.package.predictedDashEvents[eventIndex].orderedRoleMemberKeys[
            roleIndex] = emitted.member;
        builder.package.predictedDashEvents[eventIndex].orderedRoleTokens[
            roleIndex] = ownership;
        return emitted;
    };
    struct EventMembers { Emitted outer, inner, core; };
    std::vector<EventMembers> eventMembers(events.size());
    for (std::size_t i = 0; i < events.size(); ++i)
        eventMembers[i].outer = emitEventLayer(i,
            TrajectoryInsetRole::PredictedOuterRim,
            kPredictedOuterWidthPx, "outer_rim", 0);
    for (std::size_t i = 0; i < events.size(); ++i)
        eventMembers[i].inner = emitEventLayer(i,
            TrajectoryInsetRole::PredictedInnerHalo,
            kPredictedInnerWidthPx, "inner_halo", 1);
    for (std::size_t i = 0; i < events.size(); ++i) {
        eventMembers[i].core = emitEventLayer(i,
            TrajectoryInsetRole::PredictedDashCore,
            kPredictedCoreWidthPx, "core", 2);
        builder.probe(events[i].event.eventKey, eventMembers[i].core.member,
            eventMembers[i].inner.member, eventMembers[i].core.token,
            {}, {}, background);
    }
    if (!lastParent.empty() && track.endpointVisible) {
        const std::uint32_t haloOwnership = builder.token();
        const std::size_t haloBegin = builder.package.vertices.size();
        appendDiamondBand(builder.package, lastCenter, 4.5f, 3.5f,
            color(request.palette, TrajectoryInsetTrack::Predicted,
                TrajectoryInsetRole::PredictedInnerHalo), haloOwnership);
        const std::string haloPrimitive = lastArc + "_endpoint_inner_halo";
        const std::string endpointHaloMember = builder.batch(lastParent,
            lastArc, TrajectoryInsetTrack::Predicted,
            TrajectoryInsetRole::PredictedInnerHalo, haloPrimitive, haloBegin,
            haloOwnership, request.worldNotEmittedReason);
        const std::uint32_t ownership = builder.token();
        const std::size_t begin = builder.package.vertices.size();
        appendDiamondBand(builder.package, lastCenter, 3.5f, 2.5f,
            color(request.palette, TrajectoryInsetTrack::Predicted,
                TrajectoryInsetRole::PredictedEndpointDiamond), ownership);
        const std::string primitive = lastArc + "_endpoint_diamond";
        const std::string member = builder.batch(lastParent, lastArc,
            TrajectoryInsetTrack::Predicted,
            TrajectoryInsetRole::PredictedEndpointDiamond, primitive, begin,
            ownership, request.worldNotEmittedReason);
        builder.probe(lastParent, member, endpointHaloMember, ownership,
            lastCenter, {lastCenter.x + 7.0f, lastCenter.y}, background);
    }
}

double edge(P2 a, P2 b, P2 p)
{
    return static_cast<double>(b.x - a.x) * (p.y - a.y) -
        static_cast<double>(b.y - a.y) * (p.x - a.x);
}

bool topLeftYDown(P2 a, P2 b)
{
    const float dy = b.y - a.y;
    const float dx = b.x - a.x;
    return dy < 0.0f || (std::abs(dy) <= 1e-7f && dx > 0.0f);
}

bool triangleCovers(P2 a, P2 b, P2 c, P2 p)
{
    if (edge(a, b, c) < 0.0) std::swap(b, c);
    const auto accepts = [&](P2 first, P2 second) {
        const double value = edge(first, second, p);
        return value > 1e-7 || (std::abs(value) <= 1e-7 &&
            topLeftYDown(first, second));
    };
    return accepts(a, b) && accepts(b, c) && accepts(c, a);
}

using VisitRasters = std::array<TrajectoryInsetSemanticRaster,3>;

VisitRasters visitRasters(TrajectoryInsetDrawPackage package)
{
    package.ok=true;
    return {{rasterTrajectoryInsetSemantics(package,trajectoryInsetSampleProfile1x()),
        rasterTrajectoryInsetSemantics(package,trajectoryInsetSampleProfile2x()),
        rasterTrajectoryInsetSemantics(package,trajectoryInsetSampleProfile4x())}};
}

bool commonTokenPixel(const VisitRasters& rasters,std::uint32_t token)
{
    if(!token || !rasters[0].ok || !rasters[1].ok || !rasters[2].ok)return false;
    for(std::size_t pixel=0;pixel<rasters[0].perSampleTokens.size();++pixel){
        bool all=true;
        for(const auto& raster:rasters){
            bool found=false;
            for(std::size_t s=0;s<raster.sampleCount;++s)
                found|=raster.perSampleTokens[pixel*raster.sampleCount+s]==token;
            all&=found;
        }
        if(all)return true;
    }
    return false;
}

bool addEventVisitMarkers(Builder& builder,const TrajectoryInsetBuildRequest& request)
{
    auto& package=builder.package;
    const auto rasters=visitRasters(package);
    std::map<std::uint32_t,std::size_t> eventByCore;
    for(std::size_t i=0;i<package.predictedDashEvents.size();++i)
        eventByCore[package.predictedDashEvents[i].orderedRoleTokens[2]]=i;
    std::set<std::size_t> selected;
    for(std::size_t i=0;i<package.predictedDashEvents.size();++i){
        const auto& event=package.predictedDashEvents[i];
        if(commonTokenPixel(rasters,event.orderedRoleTokens[2]) &&
            commonTokenPixel(rasters,event.orderedRoleTokens[1]))continue;
        selected.insert(i);
        // A visible later pass receives its own label too: token redistribution
        // alone would leave two coincident visits indistinguishable to a person.
        const auto batch=std::find_if(package.semanticBatches.begin(),package.semanticBatches.end(),
            [&](const auto& b){return b.insetOwnershipToken==event.orderedRoleTokens[2];});
        if(batch==package.semanticBatches.end())return false;
        auto isolated=package;
        isolated.ok=true;
        isolated.vertices.assign(package.vertices.begin()+batch->firstVertex,
            package.vertices.begin()+batch->firstVertex+batch->vertexCount);
        const auto alone=rasterTrajectoryInsetSemantics(isolated,trajectoryInsetSampleProfile4x());
        for(std::size_t s=0;s<alone.perSampleTokens.size();++s){
            if(alone.perSampleTokens[s]!=event.orderedRoleTokens[2])continue;
            const auto owner=eventByCore.find(rasters[2].perSampleTokens[s]);
            if(owner!=eventByCore.end() && owner->second!=i)selected.insert(owner->second);
        }
    }
    if(selected.empty())return true;
    const auto& samples=request.snapshot.predicted.samples;
    std::vector<std::size_t> ordered(selected.begin(),selected.end());
    const auto firstTick=[&](std::size_t i){return samples.at(
        package.predictedDashEvents[i].orderedContributors.front().routeOrderOrdinal).tick;};
    std::stable_sort(ordered.begin(),ordered.end(),[&](auto a,auto b){
        return firstTick(a)<firstTick(b);});
    std::vector<TrajectoryInsetRect> occupied;
    std::size_t number=0;
    const float ui=request.layout.uiScale;
    for(std::size_t index:ordered){
        const auto event=package.predictedDashEvents[index];
        const double mid=(event.routeDistanceStartPx+event.routeDistanceEndPx)*0.5;
        const auto contributor=std::find_if(event.orderedContributors.begin(),event.orderedContributors.end(),
            [&](const auto& c){return mid>=c.eventRouteDistanceStartPx && mid<=c.eventRouteDistanceEndPx;});
        if(contributor==event.orderedContributors.end())return false;
        const auto ordinal=contributor->routeOrderOrdinal;
        if(ordinal+1>=samples.size())return false;
        Point3 a,b;
        if(!clipToPlaySurface(samples[ordinal].position,samples[ordinal+1].position,a,b))return false;
        const float f=static_cast<float>(contributor->sourceLocalTStart+
            (contributor->sourceLocalTEnd-contributor->sourceLocalTStart)*
            (mid-contributor->eventRouteDistanceStartPx)/
            (contributor->eventRouteDistanceEndPx-contributor->eventRouteDistanceStartPx));
        const P2 first=mapTablePoint(package.canonicalTableRect,a);
        const P2 last=mapTablePoint(package.canonicalTableRect,b);
        const float segmentLength=length(sub(last,first));
        if(!finite(first) || !finite(last) || !std::isfinite(f) ||
            !std::isfinite(segmentLength) || segmentLength<=1e-4f){
            package.errorCode="trajectory_inset_visit_marker_degenerate_source";
            return false;
        }
        const P2 anchor=add(first,mul(sub(last,first),f));
        const P2 tangent=mul(sub(last,first),1.0f/segmentLength);
        const P2 normal{-tangent.y,tangent.x};
        const bool tail=event.routeDistanceEndPx-event.routeDistanceStartPx<2.0;
        const std::string label=tail?"T":std::to_string(++number);
        const float width=std::max(20.0f,10.0f*label.size()+10.0f)*ui;
        TrajectoryInsetRect rect;bool placed=false;
        for(float distance:{24.f,-24.f,40.f,-40.f}){
            const P2 center=add(anchor,mul(normal,distance*ui));
            TrajectoryInsetRect candidate{center.x-width*.5f,center.y-10*ui,width,20*ui};
            const auto& plot=package.layout.plotRect;
            if(candidate.x<plot.x+ui || candidate.y<plot.y+ui ||
                candidate.x+candidate.width>plot.x+plot.width-ui ||
                candidate.y+candidate.height>plot.y+plot.height-ui)continue;
            bool clear=true;
            for(const auto& previous:occupied)
                if(candidate.x<previous.x+previous.width+2*ui && candidate.x+candidate.width+2*ui>previous.x &&
                    candidate.y<previous.y+previous.height+2*ui && candidate.y+candidate.height+2*ui>previous.y)clear=false;
            for(const auto& raster:rasters)
                for(int y=static_cast<int>(std::floor(candidate.y-plot.y));
                    clear && y<raster.height && y<=static_cast<int>(std::ceil(candidate.y+candidate.height-plot.y));++y)
                    for(int x=static_cast<int>(std::floor(candidate.x-plot.x));
                        clear && x<raster.width && x<=static_cast<int>(std::ceil(candidate.x+candidate.width-plot.x));++x)
                        for(std::size_t s=0;s<raster.sampleCount;++s)
                            if(raster.perSampleTokens[(static_cast<std::size_t>(y)*raster.width+x)*raster.sampleCount+s])clear=false;
            if(clear){rect=candidate;placed=true;break;}
        }
        if(!placed){package.errorCode="trajectory_inset_visit_marker_no_safe_position";return false;}
        occupied.push_back(rect);
        const P2 center{rect.x+rect.width*.5f,rect.y+rect.height*.5f};
        // Leaders are below the unchanged original route order; they cannot
        // replace a route/endpoint pixel with callout color or ownership.
        const auto leaderToken=builder.token();
        auto begin=package.vertices.size();
        appendSegment(package,anchor,center,1.5f*ui,color(request.palette,
            TrajectoryInsetTrack::Predicted,TrajectoryInsetRole::PredictedEventVisitLeader),leaderToken);
        builder.eventBatch(event.eventKey,TrajectoryInsetRole::PredictedEventVisitLeader,
            "visit_leader_"+std::to_string(index),begin,leaderToken);
        auto leader=package.semanticBatches.back();package.semanticBatches.pop_back();
        const auto count=package.vertices.size()-begin;
        std::rotate(package.vertices.begin(),package.vertices.begin()+begin,package.vertices.end());
        for(auto& batch:package.semanticBatches)batch.firstVertex+=count;
        leader.firstVertex=0;package.semanticBatches.insert(package.semanticBatches.begin(),leader);
        const auto haloToken=builder.token();begin=package.vertices.size();
        appendSegment(package,{rect.x,center.y},{rect.x+rect.width,center.y},rect.height,
            color(request.palette,TrajectoryInsetTrack::Predicted,TrajectoryInsetRole::PredictedEventVisitMarkerHalo),haloToken);
        const auto halo=builder.eventBatch(event.eventKey,TrajectoryInsetRole::PredictedEventVisitMarkerHalo,
            "visit_marker_halo_"+std::to_string(index),begin,haloToken);
        const auto markerToken=builder.token();begin=package.vertices.size();
        const auto rgba=color(request.palette,TrajectoryInsetTrack::Predicted,TrajectoryInsetRole::PredictedEventVisitMarker);
        const char* patterns[]={"111101101101111","010110010010111","111001111100111","111001111001111",
            "101101111001001","111100111001111","111100111101111","111001001001001","111101111101111","111101111001111"};
        for(std::size_t c=0;c<label.size();++c){
            const char* bits=label[c]=='T'?"111010010010010":patterns[label[c]-'0'];
            const float left=center.x-(label.size()*10.0f-0.4f)*ui*.5f+c*10.0f*ui;
            for(int row=0;row<5;++row)for(int col=0;col<3;++col)if(bits[row*3+col]=='1'){
                const float x=left+col*3.2f*ui,y=rect.y+2*ui+row*3.2f*ui;
                appendSegment(package,{x,y+1.6f*ui},{x+3.2f*ui,y+1.6f*ui},3.2f*ui,rgba,markerToken);
            }
        }
        const auto marker=builder.eventBatch(event.eventKey,TrajectoryInsetRole::PredictedEventVisitMarker,
            "visit_marker_"+std::to_string(index),begin,markerToken);
        const auto probe=std::find_if(builder.probeRequests.begin(),builder.probeRequests.end(),
            [&](const auto& p){return p.parent==event.eventKey;});
        if(probe==builder.probeRequests.end())return false;
        probe->evidence.sourceCoreMemberKey=probe->member;
        probe->evidence.sourceCoreVisible=commonTokenPixel(rasters,event.orderedRoleTokens[2]);
        probe->evidence.evidenceRole="event_visit_marker";
        probe->evidence.visitLabel=label;
        probe->evidence.sourceFirstTick=firstTick(index);
        probe->evidence.sourceLastTick=samples.at(event.orderedContributors.back().routeOrderOrdinal+1).tick;
        probe->evidence.sourceAnchor={{anchor.x,anchor.y}};
        probe->evidence.visitMarkerRect=rect;
        probe->member=marker;probe->companionMember=halo;probe->token=markerToken;
    }
    return true;
}

bool resolveProbeRequests(Builder& builder)
{
    TrajectoryInsetDrawPackage& package = builder.package;
    package.ok = true;
    const std::array<TrajectoryInsetSemanticRaster, 3> rasters{{
        rasterTrajectoryInsetSemantics(package, trajectoryInsetSampleProfile1x()),
        rasterTrajectoryInsetSemantics(package, trajectoryInsetSampleProfile2x()),
        rasterTrajectoryInsetSemantics(package, trajectoryInsetSampleProfile4x())}};
    for (const auto& raster : rasters) {
        if (!raster.ok) {
            package.ok = false;
            package.errorCode = "trajectory_inset_probe_unresolved";
            package.probeResolutionFailureMember = "sample_profile:" +
                raster.errorCode;
            return false;
        }
    }
    std::map<std::string, const TrajectoryInsetSemanticBatch*> byMember;
    for (const auto& batch : package.semanticBatches)
        byMember.emplace(batch.insetMemberKey, &batch);
    const auto pixelHasToken = [&](int x, int y, std::uint32_t token) {
        for (const auto& raster : rasters) {
            if (x < 0 || y < 0 || x >= raster.width || y >= raster.height)
                return false;
            bool found = false;
            for (std::size_t sample = 0; sample < raster.sampleCount; ++sample) {
                const std::size_t offset = (static_cast<std::size_t>(y) *
                    raster.width + x) * raster.sampleCount + sample;
                found = found || raster.perSampleTokens[offset] == token;
            }
            if (!found) return false;
        }
        return true;
    };
    const auto findMemberPixel = [&](const TrajectoryInsetSemanticBatch& batch,
        std::uint32_t token, int& selectedX, int& selectedY) {
        float minX = std::numeric_limits<float>::infinity();
        float minY = std::numeric_limits<float>::infinity();
        float maxX = -std::numeric_limits<float>::infinity();
        float maxY = -std::numeric_limits<float>::infinity();
        for (std::size_t i = batch.firstVertex;
             i < batch.firstVertex + batch.vertexCount; ++i) {
            minX = std::min(minX, package.vertices[i].xPhysicalPx -
                package.layout.plotRect.x);
            minY = std::min(minY, package.vertices[i].yPhysicalPx -
                package.layout.plotRect.y);
            maxX = std::max(maxX, package.vertices[i].xPhysicalPx -
                package.layout.plotRect.x);
            maxY = std::max(maxY, package.vertices[i].yPhysicalPx -
                package.layout.plotRect.y);
        }
        for (int y = std::max(0, static_cast<int>(std::floor(minY)));
             y <= std::min(rasters[0].height - 1,
                 static_cast<int>(std::ceil(maxY))); ++y)
            for (int x = std::max(0, static_cast<int>(std::floor(minX)));
                 x <= std::min(rasters[0].width - 1,
                     static_cast<int>(std::ceil(maxX))); ++x)
                if (pixelHasToken(x, y, token)) {
                    selectedX = x;
                    selectedY = y;
                    return true;
                }
        return false;
    };
    int backgroundX = -1, backgroundY = -1;
    const float tableLeft = package.canonicalTableRect.x -
        package.layout.plotRect.x;
    const float tableTop = package.canonicalTableRect.y -
        package.layout.plotRect.y;
    const float tableRight = tableLeft + package.canonicalTableRect.width;
    const float tableBottom = tableTop + package.canonicalTableRect.height;
    for (int y = 0; y < rasters[0].height && backgroundX < 0; ++y)
        for (int x = 0; x < rasters[0].width; ++x) {
            const bool outsideTable = x + 1.0f <= tableLeft ||
                y + 1.0f <= tableTop || x >= tableRight || y >= tableBottom;
            if (!outsideTable) continue;
            bool zero = true;
            for (const auto& raster : rasters)
                for (std::size_t sample = 0; sample < raster.sampleCount; ++sample)
                    zero = zero && raster.perSampleTokens[
                        (static_cast<std::size_t>(y) * raster.width + x) *
                            raster.sampleCount + sample] == 0;
            if (zero) {
                backgroundX = x;
                backgroundY = y;
                break;
            }
        }
    if (backgroundX < 0) {
        package.ok = false;
        package.errorCode = "trajectory_inset_probe_unresolved";
        package.probeResolutionFailureMember = "fixed_panel_background";
        return false;
    }
    package.probes.clear();
    package.orderedProbeIds.clear();
    for (const auto& request : builder.probeRequests) {
        const auto strokeBatch = byMember.find(request.member);
        const auto companionBatch = byMember.find(request.companionMember);
        if (strokeBatch == byMember.end() || companionBatch == byMember.end() ||
            strokeBatch->second->parentSourceKey != request.parent ||
            companionBatch->second->parentSourceKey != request.parent ||
            strokeBatch->second->insetOwnershipToken != request.token) {
            package.ok = false;
            package.errorCode = "trajectory_inset_probe_unresolved";
            package.probeResolutionFailureMember = request.member;
            package.probes.clear();
            return false;
        }
        int strokeX = -1, strokeY = -1, companionX = -1, companionY = -1;
        const bool strokeResolved = findMemberPixel(*strokeBatch->second,
            request.token, strokeX, strokeY);
        const bool companionResolved = findMemberPixel(*companionBatch->second,
            companionBatch->second->insetOwnershipToken,
            companionX, companionY);
        if (!strokeResolved || !companionResolved) {
            package.ok = false;
            package.errorCode = "trajectory_inset_probe_unresolved";
            package.probeResolutionFailureMember =
                std::string(strokeResolved ? "companion:" : "stroke:") +
                (strokeResolved ? request.companionMember : request.member);
            package.probes.clear();
            return false;
        }
        const std::string id = sha256Hex(package.generationId +
            "|trajectory_inset_probe_v1|" + request.parent + "|" +
            (request.evidence.sourceCoreMemberKey.empty()?request.member:request.evidence.sourceCoreMemberKey) + "|" + std::to_string(package.probes.size()) +
            "|" + package.layout.profileId);
        const int physicalOriginX = static_cast<int>(std::lround(
            package.layout.plotRect.x));
        const int physicalOriginY = static_cast<int>(std::lround(
            package.layout.plotRect.y));
        package.probes.push_back({id, request.parent, request.member,
            request.companionMember, request.token,
            physicalOriginX + strokeX, physicalOriginY + strokeY,
            physicalOriginX + companionX, physicalOriginY + companionY,
            physicalOriginX + backgroundX, physicalOriginY + backgroundY});
        auto& probe=package.probes.back();
        probe.sourceCoreMemberKey=request.evidence.sourceCoreMemberKey;
        probe.sourceCoreVisible=request.evidence.sourceCoreVisible;
        probe.evidenceRole=request.evidence.evidenceRole;
        probe.visitLabel=request.evidence.visitLabel;
        probe.sourceFirstTick=request.evidence.sourceFirstTick;
        probe.sourceLastTick=request.evidence.sourceLastTick;
        probe.sourceAnchor=request.evidence.sourceAnchor;
        probe.visitMarkerRect=request.evidence.visitMarkerRect;
        package.orderedProbeIds.push_back(id);
    }
    return true;
}

}  // namespace

TrajectoryInsetDrawPackage buildTrajectoryInsetDrawPackage(
    const TrajectoryInsetBuildRequest& request)
{
    Builder builder;
    builder.package.generationId = request.generationId;
    builder.package.layout = request.layout;
    builder.package.palette = request.palette;
    builder.package.canonicalTableRect = canonicalTableRect(request.layout);
    builder.forbidden.insert(request.forbiddenWorldOwnershipTokens.begin(),
        request.forbiddenWorldOwnershipTokens.end());
    if (!validateRequest(request, builder.package.errorCode))
        return builder.package;
    const auto& table = builder.package.canonicalTableRect;
    const auto& plot = request.layout.plotRect;
    const float xScale = table.width / kChinesePlayfieldLengthCm;
    const float yScale = table.height / kChinesePlayfieldWidthCm;
    const float minimumClearance = 12.0f * request.layout.uiScale;
    constexpr float kPhysicalMarginRepresentationEpsilonPx = 1e-4f;
    if (std::abs(xScale - yScale) > 1e-6f ||
        table.x - plot.x + kPhysicalMarginRepresentationEpsilonPx <
            minimumClearance ||
        table.y - plot.y + kPhysicalMarginRepresentationEpsilonPx <
            minimumClearance ||
        plot.x + plot.width - (table.x + table.width) +
            kPhysicalMarginRepresentationEpsilonPx <
            minimumClearance ||
        plot.y + plot.height - (table.y + table.height) +
            kPhysicalMarginRepresentationEpsilonPx <
            minimumClearance) {
        builder.package.errorCode = "trajectory_inset_canonical_gutter_invalid";
        return builder.package;
    }
    for (const auto& mapping : request.worldMappings) {
        const bool emitted = !mapping.orderedWorldMemberKeys.empty();
        const bool absent = !mapping.worldNotEmittedReason.empty();
        if (mapping.parentSourceKey.empty() || emitted == absent ||
            !builder.worldInputs.emplace(mapping.parentSourceKey, mapping).second) {
            builder.package.errorCode = "trajectory_inset_world_mapping_invalid";
            return builder.package;
        }
    }
    buildActual(builder, request);
    buildPredicted(builder, request);
    if (!builder.package.errorCode.empty()) return builder.package;
    if(!addEventVisitMarkers(builder,request)){
        if(builder.package.errorCode.empty())builder.package.errorCode="trajectory_inset_visit_marker_source_invalid";
        return builder.package;
    }
    if (builder.consumedWorldInputs.size() != builder.worldInputs.size()) {
        builder.package.errorCode = "trajectory_inset_world_mapping_foreign";
        return builder.package;
    }
    if (builder.package.mappingRows.empty() || builder.package.vertices.empty()) {
        builder.package.errorCode = "trajectory_inset_source_denominator_empty";
        return builder.package;
    }
    std::set<std::string> parents, members, mappingMemberReferences, probes;
    std::set<std::uint32_t> tokens;
    std::string sourceIntervalLedger;
    const auto appendLedgerField = [](std::string& ledger,
        const std::string& field) {
        ledger += std::to_string(field.size()) + ":" + field + "\n";
    };
    for (const auto& row : builder.package.mappingRows) {
        if (!parents.insert(row.parentSourceKey).second ||
            (row.orderedInsetMemberKeys.empty() &&
                row.orderedPredictedRouteIntervals.empty()) ||
            (!row.orderedWorldMemberKeys.empty() &&
                !row.worldNotEmittedReason.empty()) ||
            (row.orderedWorldMemberKeys.empty() &&
                row.worldNotEmittedReason.empty())) {
            builder.package.errorCode = "trajectory_inset_mapping_invalid";
            return builder.package;
        }
        for (const std::string& member : row.orderedInsetMemberKeys)
            if (!mappingMemberReferences.insert(member).second) {
                builder.package.errorCode = "trajectory_inset_member_duplicate";
                return builder.package;
        }
        if (!row.orderedPredictedRouteIntervals.empty()) {
            appendLedgerField(sourceIntervalLedger, row.parentSourceKey);
            appendLedgerField(sourceIntervalLedger,
                std::to_string(row.orderedPredictedRouteIntervals.size()));
        }
        double expectedT = 0.0;
        for (const auto& interval : row.orderedPredictedRouteIntervals) {
            const bool event = !interval.dashEventKey.empty();
            const bool gap = !interval.nonRenderedReason.empty();
            if (!std::isfinite(interval.sourceLocalTStart) ||
                !std::isfinite(interval.sourceLocalTEnd) || event == gap ||
                std::abs(interval.sourceLocalTStart - expectedT) > 1e-7 ||
                interval.sourceLocalTEnd <= interval.sourceLocalTStart ||
                interval.sourceLocalTEnd > 1.0 + 1e-7 ||
                interval.routeDistanceEndPx <= interval.routeDistanceStartPx) {
                builder.package.errorCode =
                    "trajectory_inset_predicted_interval_invalid";
                return builder.package;
            }
            appendLedgerField(sourceIntervalLedger, interval.arcSegmentId);
            appendLedgerField(sourceIntervalLedger,
                std::to_string(interval.routeOrderOrdinal));
            appendLedgerField(sourceIntervalLedger,
                std::to_string(interval.routeSpanOrdinal));
            appendLedgerField(sourceIntervalLedger,
                canonicalDoubleBits(interval.sourceLocalTStart));
            appendLedgerField(sourceIntervalLedger,
                canonicalDoubleBits(interval.sourceLocalTEnd));
            appendLedgerField(sourceIntervalLedger,
                canonicalDoubleBits(interval.routeDistanceStartPx));
            appendLedgerField(sourceIntervalLedger,
                canonicalDoubleBits(interval.routeDistanceEndPx));
            appendLedgerField(sourceIntervalLedger, interval.dashEventKey);
            appendLedgerField(sourceIntervalLedger,
                interval.nonRenderedReason);
            expectedT = interval.sourceLocalTEnd;
        }
        if (!row.orderedPredictedRouteIntervals.empty() &&
            std::abs(expectedT - 1.0) > 1e-7) {
            builder.package.errorCode =
                "trajectory_inset_predicted_interval_incomplete";
            return builder.package;
        }
    }
    for (const auto& batch : builder.package.semanticBatches) {
        if (batch.insetOwnershipToken == 0 ||
            builder.forbidden.count(batch.insetOwnershipToken) != 0 ||
            !tokens.insert(batch.insetOwnershipToken).second ||
            !members.insert(batch.insetMemberKey).second) {
            builder.package.errorCode = "trajectory_inset_token_invalid";
            return builder.package;
        }
    }
    for (const std::string& member : mappingMemberReferences)
        if (members.count(member) == 0) {
            builder.package.errorCode = "trajectory_inset_mapping_member_missing";
            return builder.package;
        }
    std::set<std::string> eventKeys;
    std::string eventLedger, contributorLedger;
    for (const auto& event : builder.package.predictedDashEvents) {
        if (event.eventKey.empty() || !eventKeys.insert(event.eventKey).second ||
            event.orderedContributors.empty() ||
            event.routeDistanceEndPx <= event.routeDistanceStartPx) {
            builder.package.errorCode = "trajectory_inset_dash_event_invalid";
            return builder.package;
        }
        appendLedgerField(eventLedger, event.eventKey);
        appendLedgerField(eventLedger,
            std::to_string(event.routeWideDashEventOrdinal));
        appendLedgerField(eventLedger,
            std::to_string(event.routeSpanOrdinal));
        appendLedgerField(eventLedger,
            canonicalDoubleBits(event.routeDistanceStartPx));
        appendLedgerField(eventLedger,
            canonicalDoubleBits(event.routeDistanceEndPx));
        for (std::size_t role = 0; role < event.orderedRoleMemberKeys.size();
             ++role) {
            appendLedgerField(eventLedger, event.orderedRoleMemberKeys[role]);
            appendLedgerField(eventLedger,
                std::to_string(event.orderedRoleTokens[role]));
        }
        double contributorCursor = event.routeDistanceStartPx;
        for (const auto& contributor : event.orderedContributors) {
            if (parents.count(contributor.parentSourceKey) == 0 ||
                std::abs(contributor.eventRouteDistanceStartPx -
                    contributorCursor) > 1e-7 ||
                contributor.eventRouteDistanceEndPx <=
                    contributor.eventRouteDistanceStartPx ||
                contributor.sourceLocalTStart < 0.0 ||
                contributor.sourceLocalTEnd > 1.0 ||
                contributor.sourceLocalTEnd <= contributor.sourceLocalTStart) {
                builder.package.errorCode =
                    "trajectory_inset_dash_contributor_invalid";
                return builder.package;
            }
            contributorCursor = contributor.eventRouteDistanceEndPx;
            appendLedgerField(contributorLedger, event.eventKey);
            appendLedgerField(contributorLedger,
                contributor.parentSourceKey);
            appendLedgerField(contributorLedger, contributor.arcSegmentId);
            appendLedgerField(contributorLedger,
                std::to_string(contributor.routeOrderOrdinal));
            appendLedgerField(contributorLedger,
                canonicalDoubleBits(contributor.sourceLocalTStart));
            appendLedgerField(contributorLedger,
                canonicalDoubleBits(contributor.sourceLocalTEnd));
            appendLedgerField(contributorLedger,
                canonicalDoubleBits(contributor.eventRouteDistanceStartPx));
            appendLedgerField(contributorLedger,
                canonicalDoubleBits(contributor.eventRouteDistanceEndPx));
        }
        if (std::abs(contributorCursor - event.routeDistanceEndPx) > 1e-7)
        {
            builder.package.errorCode =
                "trajectory_inset_dash_contributor_union_invalid";
            return builder.package;
        }
        for (const std::string& member : event.orderedRoleMemberKeys)
            if (members.count(member) == 0) {
                builder.package.errorCode =
                    "trajectory_inset_dash_event_member_missing";
                return builder.package;
            }
    }
    for (const auto& row : builder.package.mappingRows)
        for (const auto& interval : row.orderedPredictedRouteIntervals)
            if (!interval.dashEventKey.empty() &&
                eventKeys.count(interval.dashEventKey) == 0) {
                builder.package.errorCode =
                    "trajectory_inset_dash_event_reference_missing";
                return builder.package;
            }
    if (!resolveProbeRequests(builder)) return builder.package;
    for (const auto& probe : builder.package.probes) {
        if (!probes.insert(probe.probeId).second ||
            tokens.count(probe.insetOwnershipToken) == 0 ||
            members.count(probe.insetMemberKey) == 0 ||
            members.count(probe.companionInsetMemberKey) == 0) {
            builder.package.errorCode = "trajectory_inset_probe_invalid";
            return builder.package;
        }
    }
    builder.package.sourceGroupDenominator = builder.package.mappingRows.size();
    builder.package.probeDenominator = builder.package.probes.size();
    builder.package.orderedParentSourceDigest = joinedDigest(
        builder.package.orderedParentSourceKeys);
    std::string probeLedger;
    for (const auto& probe : builder.package.probes) {
        appendLedgerField(probeLedger, probe.probeId);
        appendLedgerField(probeLedger, probe.parentSourceKey);
        appendLedgerField(probeLedger, probe.insetMemberKey);
        appendLedgerField(probeLedger, probe.companionInsetMemberKey);
        appendLedgerField(probeLedger,
            std::to_string(probe.insetOwnershipToken));
        appendLedgerField(probeLedger, std::to_string(probe.strokeXPhysicalPx));
        appendLedgerField(probeLedger, std::to_string(probe.strokeYPhysicalPx));
        appendLedgerField(probeLedger,
            std::to_string(probe.companionXPhysicalPx));
        appendLedgerField(probeLedger,
            std::to_string(probe.companionYPhysicalPx));
        appendLedgerField(probeLedger,
            std::to_string(probe.backgroundXPhysicalPx));
        appendLedgerField(probeLedger,
            std::to_string(probe.backgroundYPhysicalPx));
        probeLedger+=trajectoryInsetProbeSourceEvidenceLedger(probe);
    }
    builder.package.orderedProbeDigest = sha256Hex(probeLedger);
    builder.package.orderedDashEventDigest = sha256Hex(eventLedger);
    builder.package.orderedDashContributorDigest = sha256Hex(contributorLedger);
    builder.package.orderedSourceRouteIntervalDigest =
        sha256Hex(sourceIntervalLedger);
    builder.package.cameraIndependent = true;
    builder.package.fixedPanelBackground = true;
    builder.package.worldVisibilityIgnored = true;
    builder.package.orientationLabelsExcludedFromDenominator = true;
    builder.package.ok = true;
    std::string visitError;
    if(!validateTrajectoryInsetVisitEvidence(builder.package,visitError)){
        builder.package.ok=false;builder.package.errorCode=visitError;
    }
    return builder.package;
}

bool applyActualTerminalOwnershipOrder(TrajectoryInsetDrawPackage& package,
    const std::string& terminalParent,std::string& error)
{
    const auto reject=[&](const char* code){error=code;return false;};
    if(terminalParent.empty() || package.actualMidpointWitnesses.empty() ||
        package.actualMidpointWitnesses.back().parentSourceKey!=terminalParent)
        return reject("trajectory_inset_terminal_parent_invalid");
    std::vector<std::size_t> indices;
    unsigned capsuleCount=0;
    for(std::size_t i=0;i<package.semanticBatches.size();++i){
        const auto& b=package.semanticBatches[i];
        if(b.role==TrajectoryInsetRole::ActualEndpointCapsule)++capsuleCount;
        if(b.parentSourceKey==terminalParent && b.track==TrajectoryInsetTrack::Actual &&
            (b.role==TrajectoryInsetRole::ActualLeftCore || b.role==TrajectoryInsetRole::ActualRightCore ||
             b.role==TrajectoryInsetRole::ActualEndpointCapsule))indices.push_back(i);
    }
    if(indices.size()!=4 || capsuleCount!=2)
        return reject("trajectory_inset_terminal_member_inventory_invalid");
    for(std::size_t n=1;n<indices.size();++n)
        if(indices[n]!=indices[n-1]+1)
            return reject("trajectory_inset_terminal_block_not_contiguous");
    const auto& first=package.semanticBatches[indices[0]];
    if(first.role==TrajectoryInsetRole::ActualEndpointCapsule)
        return reject("trajectory_inset_terminal_order_already_applied");
    if(first.role!=TrajectoryInsetRole::ActualLeftCore ||
        package.semanticBatches[indices[1]].role!=TrajectoryInsetRole::ActualRightCore ||
        package.semanticBatches[indices[2]].role!=TrajectoryInsetRole::ActualEndpointCapsule ||
        package.semanticBatches[indices[3]].role!=TrajectoryInsetRole::ActualEndpointCapsule)
        return reject("trajectory_inset_terminal_order_invalid");
    const auto& rightCapsule=package.semanticBatches[indices[2]];
    const auto& leftCapsule=package.semanticBatches[indices[3]];
    if(rightCapsule.primitiveId!=first.arcSegmentId+"_right_endpoint_capsule" ||
        leftCapsule.primitiveId!=first.arcSegmentId+"_left_endpoint_capsule")
        return reject("trajectory_inset_terminal_capsule_identity_invalid");
    std::size_t cursor=first.firstVertex;
    std::array<float,4> rgba{};
    bool haveColor=false;
    for(auto index:indices){
        const auto& b=package.semanticBatches[index];
        if(b.arcSegmentId!=first.arcSegmentId || b.vertexCount==0 || b.firstVertex!=cursor ||
            b.firstVertex>package.vertices.size() || b.vertexCount>package.vertices.size()-b.firstVertex)
            return reject("trajectory_inset_terminal_vertex_range_invalid");
        for(std::size_t i=b.firstVertex;i<b.firstVertex+b.vertexCount;++i){
            const auto& v=package.vertices[i];
            if(!finite({v.xPhysicalPx,v.yPhysicalPx}) || v.insetOwnershipToken!=b.insetOwnershipToken ||
                v.linearRgba[3]!=1.f || std::any_of(v.linearRgba.begin(),v.linearRgba.end(),
                    [](float value){return !std::isfinite(value);}))
                return reject("trajectory_inset_terminal_vertex_invalid");
            if(!haveColor){rgba=v.linearRgba;haveColor=true;}
            if(v.linearRgba!=rgba)return reject("trajectory_inset_terminal_color_mismatch");
        }
        cursor+=b.vertexCount;
    }
    const std::size_t start=first.firstVertex,split=rightCapsule.firstVertex;
    // The four-member block is the entire mutation boundary. Earlier actual
    // members and later notch/predicted/visit layers keep their exact order.
    std::rotate(package.vertices.begin()+start,package.vertices.begin()+split,
        package.vertices.begin()+cursor);
    std::rotate(package.semanticBatches.begin()+indices[0],
        package.semanticBatches.begin()+indices[2],package.semanticBatches.begin()+indices[3]+1);
    cursor=start;
    for(auto index:indices){package.semanticBatches[index].firstVertex=cursor;
        cursor+=package.semanticBatches[index].vertexCount;}
    return true;
}

bool validateTrajectoryInsetVisitEvidence(const TrajectoryInsetDrawPackage& package,
    std::string& error)
{
    const auto reject=[&](const char* code){error=code;return false;};
    std::vector<const InsetTrajectoryProbe*> markers;
    for(const auto& p:package.probes){
        if(!p.sourceCoreMemberKey.empty())markers.push_back(&p);
        else if(std::any_of(package.semanticBatches.begin(),package.semanticBatches.end(),
            [&](const auto& b){return b.insetMemberKey==p.insetMemberKey &&
                b.role==TrajectoryInsetRole::PredictedEventVisitMarker;}))
            return reject("trajectory_inset_visit_marker_source_missing");
    }
    if(markers.empty())return true;
    const auto rasters=visitRasters(package);
    const auto batch=[&](const std::string& member)->const TrajectoryInsetSemanticBatch*{
        const auto found=std::find_if(package.semanticBatches.begin(),package.semanticBatches.end(),
            [&](const auto& b){return b.insetMemberKey==member;});
        return found==package.semanticBatches.end()?nullptr:&*found;
    };
    std::stable_sort(markers.begin(),markers.end(),[](const auto* a,const auto* b){
        return a->sourceFirstTick<b->sourceFirstTick;});
    std::set<std::string> events;
    unsigned ordinal=0;
    for(const auto* p:markers){
        const auto event=std::find_if(package.predictedDashEvents.begin(),package.predictedDashEvents.end(),
            [&](const auto& e){return e.eventKey==p->parentSourceKey;});
        const auto* source=batch(p->sourceCoreMemberKey);
        const auto* marker=batch(p->insetMemberKey);
        const auto* halo=batch(p->companionInsetMemberKey);
        if(event==package.predictedDashEvents.end() || !source || !marker || !halo ||
            !events.insert(p->parentSourceKey).second ||
            event->orderedRoleMemberKeys[2]!=source->insetMemberKey ||
            source->role!=TrajectoryInsetRole::PredictedDashCore ||
            marker->role!=TrajectoryInsetRole::PredictedEventVisitMarker ||
            halo->role!=TrajectoryInsetRole::PredictedEventVisitMarkerHalo ||
            marker->parentSourceKey!=event->eventKey || halo->parentSourceKey!=event->eventKey ||
            marker->track!=TrajectoryInsetTrack::Predicted || halo->track!=TrajectoryInsetTrack::Predicted ||
            marker->insetOwnershipToken!=p->insetOwnershipToken ||
            marker->insetOwnershipToken==halo->insetOwnershipToken ||
            p->sourceFirstTick>p->sourceLastTick || p->evidenceRole!="event_visit_marker")
            return reject("trajectory_inset_visit_marker_identity_invalid");
        const bool tail=event->routeDistanceEndPx-event->routeDistanceStartPx<2.0;
        if(p->visitLabel!=(tail?"T":std::to_string(++ordinal)))
            return reject("trajectory_inset_visit_marker_order_invalid");
        if(commonTokenPixel(rasters,source->insetOwnershipToken)!=p->sourceCoreVisible)
            return reject("trajectory_inset_visit_core_visibility_invalid");
        if(source->firstVertex>package.vertices.size() ||
            source->vertexCount>package.vertices.size()-source->firstVertex)
            return reject("trajectory_inset_visit_marker_geometry_invalid");
        const auto& rect=p->visitMarkerRect; const auto& plot=package.layout.plotRect;
        if(!std::isfinite(rect.x)||!std::isfinite(rect.y)||!std::isfinite(rect.width)||
            !std::isfinite(rect.height)||rect.width<=0||rect.height<=0||
            rect.x<plot.x||rect.y<plot.y||rect.x+rect.width>plot.x+plot.width||
            rect.y+rect.height>plot.y+plot.height)
            return reject("trajectory_inset_visit_marker_bounds_invalid");
        bool anchorOnSource=false;
        for(std::size_t v=source->firstVertex;v+2<source->firstVertex+source->vertexCount;v+=3){
            const auto point=[&](std::size_t n){return P2{package.vertices[n].xPhysicalPx,package.vertices[n].yPhysicalPx};};
            anchorOnSource|=triangleCovers(point(v),point(v+1),point(v+2),
                {p->sourceAnchor[0],p->sourceAnchor[1]});
        }
        if(!anchorOnSource)return reject("trajectory_inset_visit_marker_anchor_invalid");
        for(const auto* b:{marker,halo}){
            if(b->firstVertex>package.vertices.size() || b->vertexCount>package.vertices.size()-b->firstVertex)
                return reject("trajectory_inset_visit_marker_geometry_invalid");
            for(std::size_t v=b->firstVertex;v<b->firstVertex+b->vertexCount;++v){
                const auto& vertex=package.vertices[v];
                if(vertex.insetOwnershipToken!=b->insetOwnershipToken ||
                    vertex.xPhysicalPx<rect.x-1e-4f || vertex.yPhysicalPx<rect.y-1e-4f ||
                    vertex.xPhysicalPx>rect.x+rect.width+1e-4f ||
                    vertex.yPhysicalPx>rect.y+rect.height+1e-4f)
                    return reject("trajectory_inset_visit_marker_geometry_invalid");
            }
            if(!commonTokenPixel(rasters,b->insetOwnershipToken))
                return reject("trajectory_inset_visit_marker_not_visible");
        }
    }
    return true;
}

TrajectoryInsetSemanticRaster rasterTrajectoryInsetSemantics(
    const TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetSampleProfile& sampleProfile)
{
    TrajectoryInsetSemanticRaster result;
    if (!package.ok || sampleProfile.positions.empty() ||
        (sampleProfile.positions.size() != 1 &&
         sampleProfile.positions.size() != 2 &&
         sampleProfile.positions.size() != 4)) {
        result.errorCode = "trajectory_inset_raster_input_invalid";
        return result;
    }
    for (const auto& sample : sampleProfile.positions)
        if (!(sample[0] >= 0.0f && sample[0] < 1.0f &&
              sample[1] >= 0.0f && sample[1] < 1.0f)) {
            result.errorCode = "trajectory_inset_sample_position_invalid";
            return result;
        }
    const auto& plot = package.layout.plotRect;
    result.width = static_cast<int>(std::ceil(plot.width));
    result.height = static_cast<int>(std::ceil(plot.height));
    result.sampleCount = sampleProfile.positions.size();
    result.perSampleTokens.assign(static_cast<std::size_t>(result.width) *
        result.height * result.sampleCount, 0);
    for (std::size_t vertex = 0; vertex + 2 < package.vertices.size();
         vertex += 3) {
        const auto& va = package.vertices[vertex];
        const auto& vb = package.vertices[vertex + 1];
        const auto& vc = package.vertices[vertex + 2];
        const P2 a{va.xPhysicalPx - plot.x, va.yPhysicalPx - plot.y};
        const P2 b{vb.xPhysicalPx - plot.x, vb.yPhysicalPx - plot.y};
        const P2 c{vc.xPhysicalPx - plot.x, vc.yPhysicalPx - plot.y};
        const int minX = std::max(0, static_cast<int>(std::floor(
            std::min({a.x, b.x, c.x}))));
        const int maxX = std::min(result.width - 1, static_cast<int>(std::ceil(
            std::max({a.x, b.x, c.x}))));
        const int minY = std::max(0, static_cast<int>(std::floor(
            std::min({a.y, b.y, c.y}))));
        const int maxY = std::min(result.height - 1, static_cast<int>(std::ceil(
            std::max({a.y, b.y, c.y}))));
        for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x)
            for (std::size_t sample = 0; sample < result.sampleCount; ++sample) {
                const P2 p{x + sampleProfile.positions[sample][0],
                    y + sampleProfile.positions[sample][1]};
                if (triangleCovers(a, b, c, p)) {
                    const std::size_t index = (static_cast<std::size_t>(y) *
                        result.width + x) * result.sampleCount + sample;
                    result.perSampleTokens[index] = va.insetOwnershipToken;
                }
            }
    }
    std::string bytes;
    bytes.resize(result.perSampleTokens.size() * sizeof(std::uint32_t));
    std::memcpy(bytes.data(), result.perSampleTokens.data(), bytes.size());
    result.tokenDigest = sha256Hex(bytes);
    result.ok = true;
    return result;
}

TrajectoryInsetSampleProfile trajectoryInsetSampleProfile1x()
{
    return {"center_1x", {{{0.5f, 0.5f}}}};
}

TrajectoryInsetSampleProfile trajectoryInsetSampleProfile2x()
{
    return {"asymmetric_2x", {{{0.25f, 0.75f}}, {{0.75f, 0.25f}}}};
}

TrajectoryInsetSampleProfile trajectoryInsetSampleProfile4x()
{
    return {"asymmetric_4x", {{{0.375f, 0.125f}}, {{0.875f, 0.375f}},
        {{0.125f, 0.625f}}, {{0.625f, 0.875f}}}};
}

}  // namespace billiardgl::training
