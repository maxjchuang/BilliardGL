#include "deterministic_hash.h"
#include "training/trajectory_inset_renderer.h"
#include "training/trajectory_overlay_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <locale>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::training;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

TrajectoryOverlaySnapshot coaxialFixture()
{
    TrajectoryOverlaySnapshot snapshot;
    snapshot.actual.visible = true;
    snapshot.actual.endpointVisible = true;
    snapshot.actual.railCollisionEvents = 2;
    snapshot.actual.encoding.lineStyle = TrajectoryLineStyle::SolidWithMarkers;
    snapshot.predicted.visible = true;
    snapshot.predicted.endpointVisible = true;
    snapshot.predicted.railCollisionEvents = 2;
    snapshot.predicted.encoding.lineStyle = TrajectoryLineStyle::Dashed;
    const std::array<float, 5> z{{-127.0f, -64.0f, 0.0f, 64.0f, 127.0f}};
    for (std::size_t i = 0; i < z.size(); ++i) {
        const OverlaySample sample{static_cast<std::uint64_t>(i),
            {0.0f, 86.5f, z[i]}, true};
        snapshot.actual.samples.push_back(sample);
        snapshot.predicted.samples.push_back(sample);
    }
    snapshot.actual.endpoint = snapshot.actual.samples.back().position;
    snapshot.predicted.endpoint = snapshot.predicted.samples.back().position;
    return snapshot;
}

TrajectoryOverlaySnapshot edgeCornerFixture()
{
    TrajectoryOverlaySnapshot snapshot = coaxialFixture();
    snapshot.actual.samples = {
        {0, {-63.5f, 86.5f, -127.0f}, true},
        {1, {-63.5f, 86.5f, 0.0f}, true},
        {2, {63.5f, 86.5f, 0.0f}, true},
        {3, {63.5f, 86.5f, 127.0f}, true}};
    snapshot.actual.endpoint = snapshot.actual.samples.back().position;
    snapshot.predicted.samples = snapshot.actual.samples;
    snapshot.predicted.endpoint = snapshot.predicted.samples.back().position;
    return snapshot;
}

TrajectoryOverlaySnapshot gapOnlySourceFixture()
{
    TrajectoryOverlaySnapshot snapshot = coaxialFixture();
    constexpr float kReferenceScale = 368.0f / 254.0f;
    const float z0 = -100.0f;
    const float z1 = z0 + 15.0f / kReferenceScale;
    const float z2 = z1 + 4.0f / kReferenceScale;
    snapshot.predicted.endpointVisible = false;
    snapshot.predicted.samples = {
        {0, {0.0f, 86.5f, z0}, true},
        {1, {0.0f, 86.5f, z1}, true},
        {2, {0.0f, 86.5f, z2}, true}};
    return snapshot;
}

TrajectoryOverlaySnapshot discontinuousPredictedFixture()
{
    TrajectoryOverlaySnapshot snapshot = coaxialFixture();
    snapshot.predicted.endpointVisible = false;
    snapshot.predicted.samples = {
        {0, {0.0f, 86.5f, -110.0f}, true},
        {1, {0.0f, 86.5f, -80.0f}, true},
        {2, {0.0f, 86.5f, -20.0f}, false},
        {3, {0.0f, 86.5f, 40.0f}, true},
        {4, {0.0f, 86.5f, 70.0f}, true}};
    return snapshot;
}

TrajectoryInsetLayout layout(std::size_t index)
{
    struct Definition {
        const char* id; int width; int height; float ui;
        float panelWidth; float panelHeight; float plotX; float plotY;
        float plotWidth; float plotHeight;
    };
    const std::array<Definition, 4> definitions{{
        {"1920x1080_ui100", 1920, 1080, 1.0f, 448, 288, 16, 48, 416, 208},
        {"1920x1080_ui135", 1920, 1080, 1.35f, 604.8f, 388.8f,
            21.6f, 64.8f, 561.6f, 280.8f},
        {"1280x720_ui100", 1280, 720, 1.0f, 336, 216, 24, 48, 288, 144},
        {"1280x720_ui135", 1280, 720, 1.35f, 453.6f, 291.6f,
            21.6f, 64.8f, 410.4f, 205.2f}}};
    const Definition& d = definitions.at(index);
    const float right = 32.0f * d.ui;
    const float bottom = 112.0f * d.ui;
    const TrajectoryInsetRect panel{d.width - right - d.panelWidth,
        d.height - bottom - d.panelHeight, d.panelWidth, d.panelHeight};
    return {d.id, d.width, d.height, d.ui, panel,
        {panel.x + d.plotX, panel.y + d.plotY, d.plotWidth, d.plotHeight}};
}

TrajectoryInsetBuildRequest requestFor(std::size_t layoutIndex,
    TrajectoryInsetPalette palette,
    const TrajectoryOverlaySnapshot& snapshot = coaxialFixture())
{
    TrajectoryInsetBuildRequest request;
    request.generationId = "v19_generation_001";
    request.snapshot = snapshot;
    request.layout = layout(layoutIndex);
    request.palette = palette;
    request.forbiddenWorldOwnershipTokens = {1u, 2u, 0x80000001u};
    return request;
}

template <typename T>
void appendBytes(std::string& bytes, const T& value)
{
    bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::string independentCanonicalDoubleBits(double value)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::hex << std::setfill('0') << std::setw(16) << bits;
    return out.str();
}

void independentLedgerField(std::string& ledger, const std::string& field)
{
    ledger += std::to_string(field.size()) + ":" + field + "\n";
}

struct RecomputedInsetDigests {
    std::string parents;
    std::string probes;
    std::string events;
    std::string contributors;
    std::string sourceIntervals;
};

RecomputedInsetDigests recomputeInsetDigests(
    const TrajectoryInsetDrawPackage& package)
{
    std::string parentLedger, probeLedger, eventLedger, contributorLedger,
        sourceLedger;
    for (const auto& parent : package.orderedParentSourceKeys)
        independentLedgerField(parentLedger, parent);
    for (const auto& probe : package.probes) {
        independentLedgerField(probeLedger, probe.probeId);
        independentLedgerField(probeLedger, probe.parentSourceKey);
        independentLedgerField(probeLedger, probe.insetMemberKey);
        independentLedgerField(probeLedger, probe.companionInsetMemberKey);
        independentLedgerField(probeLedger,
            std::to_string(probe.insetOwnershipToken));
        independentLedgerField(probeLedger,
            std::to_string(probe.strokeXPhysicalPx));
        independentLedgerField(probeLedger,
            std::to_string(probe.strokeYPhysicalPx));
        independentLedgerField(probeLedger,
            std::to_string(probe.companionXPhysicalPx));
        independentLedgerField(probeLedger,
            std::to_string(probe.companionYPhysicalPx));
        independentLedgerField(probeLedger,
            std::to_string(probe.backgroundXPhysicalPx));
        independentLedgerField(probeLedger,
            std::to_string(probe.backgroundYPhysicalPx));
    }
    for (const auto& event : package.predictedDashEvents) {
        independentLedgerField(eventLedger, event.eventKey);
        independentLedgerField(eventLedger,
            std::to_string(event.routeWideDashEventOrdinal));
        independentLedgerField(eventLedger,
            std::to_string(event.routeSpanOrdinal));
        independentLedgerField(eventLedger,
            independentCanonicalDoubleBits(event.routeDistanceStartPx));
        independentLedgerField(eventLedger,
            independentCanonicalDoubleBits(event.routeDistanceEndPx));
        for (std::size_t role = 0; role < event.orderedRoleMemberKeys.size();
             ++role) {
            independentLedgerField(eventLedger,
                event.orderedRoleMemberKeys[role]);
            independentLedgerField(eventLedger,
                std::to_string(event.orderedRoleTokens[role]));
        }
        for (const auto& contributor : event.orderedContributors) {
            independentLedgerField(contributorLedger, event.eventKey);
            independentLedgerField(contributorLedger,
                contributor.parentSourceKey);
            independentLedgerField(contributorLedger,
                contributor.arcSegmentId);
            independentLedgerField(contributorLedger,
                std::to_string(contributor.routeOrderOrdinal));
            independentLedgerField(contributorLedger,
                independentCanonicalDoubleBits(
                    contributor.sourceLocalTStart));
            independentLedgerField(contributorLedger,
                independentCanonicalDoubleBits(
                    contributor.sourceLocalTEnd));
            independentLedgerField(contributorLedger,
                independentCanonicalDoubleBits(
                    contributor.eventRouteDistanceStartPx));
            independentLedgerField(contributorLedger,
                independentCanonicalDoubleBits(
                    contributor.eventRouteDistanceEndPx));
        }
    }
    for (const auto& row : package.mappingRows) {
        if (row.orderedPredictedRouteIntervals.empty()) continue;
        independentLedgerField(sourceLedger, row.parentSourceKey);
        independentLedgerField(sourceLedger,
            std::to_string(row.orderedPredictedRouteIntervals.size()));
        for (const auto& interval : row.orderedPredictedRouteIntervals) {
            independentLedgerField(sourceLedger, interval.arcSegmentId);
            independentLedgerField(sourceLedger,
                std::to_string(interval.routeOrderOrdinal));
            independentLedgerField(sourceLedger,
                std::to_string(interval.routeSpanOrdinal));
            independentLedgerField(sourceLedger,
                independentCanonicalDoubleBits(interval.sourceLocalTStart));
            independentLedgerField(sourceLedger,
                independentCanonicalDoubleBits(interval.sourceLocalTEnd));
            independentLedgerField(sourceLedger,
                independentCanonicalDoubleBits(
                    interval.routeDistanceStartPx));
            independentLedgerField(sourceLedger,
                independentCanonicalDoubleBits(
                    interval.routeDistanceEndPx));
            independentLedgerField(sourceLedger, interval.dashEventKey);
            independentLedgerField(sourceLedger, interval.nonRenderedReason);
        }
    }
    return {sha256Hex(parentLedger), sha256Hex(probeLedger),
        sha256Hex(eventLedger), sha256Hex(contributorLedger),
        sha256Hex(sourceLedger)};
}

bool exactDigestContractValid(const TrajectoryInsetDrawPackage& package)
{
    const auto exact = recomputeInsetDigests(package);
    return exact.parents == package.orderedParentSourceDigest &&
        exact.probes == package.orderedProbeDigest &&
        exact.events == package.orderedDashEventDigest &&
        exact.contributors == package.orderedDashContributorDigest &&
        exact.sourceIntervals == package.orderedSourceRouteIntervalDigest;
}

std::string topologyDigest(const TrajectoryInsetDrawPackage& package)
{
    std::string bytes;
    for (const auto& vertex : package.vertices) {
        appendBytes(bytes, vertex.xPhysicalPx);
        appendBytes(bytes, vertex.yPhysicalPx);
        appendBytes(bytes, vertex.insetOwnershipToken);
    }
    for (const auto& batch : package.semanticBatches) {
        bytes += batch.parentSourceKey + "|" + batch.primitiveId + "|" +
            batch.arcSegmentId + "|" + trajectoryInsetRoleName(batch.role) +
            "|" + std::to_string(batch.firstVertex) + "|" +
            std::to_string(batch.vertexCount) + "|" +
            std::to_string(batch.insetOwnershipToken) + "\n";
    }
    for (const auto& row : package.mappingRows) {
        bytes += row.parentSourceKey + "|" + row.worldNotEmittedReason + "\n";
        for (const auto& member : row.orderedInsetMemberKeys)
            bytes += member + "\n";
        for (const auto& interval : row.orderedPredictedRouteIntervals) {
            bytes += interval.arcSegmentId + "|" +
                std::to_string(interval.routeOrderOrdinal) + "|" +
                std::to_string(interval.routeSpanOrdinal) + "|" +
                interval.dashEventKey + "|" + interval.nonRenderedReason + "\n";
            appendBytes(bytes, interval.sourceLocalTStart);
            appendBytes(bytes, interval.sourceLocalTEnd);
            appendBytes(bytes, interval.routeDistanceStartPx);
            appendBytes(bytes, interval.routeDistanceEndPx);
        }
    }
    for (const auto& event : package.predictedDashEvents) {
        bytes += event.eventKey + "|" +
            std::to_string(event.routeWideDashEventOrdinal) + "|" +
            std::to_string(event.routeSpanOrdinal) + "\n";
        appendBytes(bytes, event.routeDistanceStartPx);
        appendBytes(bytes, event.routeDistanceEndPx);
        for (const auto& member : event.orderedRoleMemberKeys) bytes += member + "\n";
        for (std::uint32_t token : event.orderedRoleTokens) appendBytes(bytes, token);
        for (const auto& contributor : event.orderedContributors) {
            bytes += contributor.parentSourceKey + "|" +
                contributor.arcSegmentId + "|" +
                std::to_string(contributor.routeOrderOrdinal) + "\n";
            appendBytes(bytes, contributor.sourceLocalTStart);
            appendBytes(bytes, contributor.sourceLocalTEnd);
            appendBytes(bytes, contributor.eventRouteDistanceStartPx);
            appendBytes(bytes, contributor.eventRouteDistanceEndPx);
        }
    }
    for (const auto& probe : package.probes) {
        bytes += probe.probeId + "|" + probe.parentSourceKey + "|" +
            probe.insetMemberKey + "|" + probe.companionInsetMemberKey + "\n";
        appendBytes(bytes, probe.insetOwnershipToken);
        appendBytes(bytes, probe.strokeXPhysicalPx);
        appendBytes(bytes, probe.strokeYPhysicalPx);
        appendBytes(bytes, probe.companionXPhysicalPx);
        appendBytes(bytes, probe.companionYPhysicalPx);
        appendBytes(bytes, probe.backgroundXPhysicalPx);
        appendBytes(bytes, probe.backgroundYPhysicalPx);
    }
    bytes += package.orderedDashEventDigest + "\n" +
        package.orderedDashContributorDigest + "\n" +
        package.orderedSourceRouteIntervalDigest + "\n" +
        package.orderedProbeDigest + "\n";
    return sha256Hex(bytes);
}

bool probeInventoryEqual(const TrajectoryInsetDrawPackage& first,
    const TrajectoryInsetDrawPackage& second)
{
    if (first.orderedProbeIds != second.orderedProbeIds ||
        first.orderedProbeDigest != second.orderedProbeDigest ||
        first.probes.size() != second.probes.size()) return false;
    for (std::size_t i = 0; i < first.probes.size(); ++i) {
        const auto& a = first.probes[i];
        const auto& b = second.probes[i];
        if (a.probeId != b.probeId ||
            a.parentSourceKey != b.parentSourceKey ||
            a.insetMemberKey != b.insetMemberKey ||
            a.companionInsetMemberKey != b.companionInsetMemberKey ||
            a.insetOwnershipToken != b.insetOwnershipToken ||
            a.strokeXPhysicalPx != b.strokeXPhysicalPx ||
            a.strokeYPhysicalPx != b.strokeYPhysicalPx ||
            a.companionXPhysicalPx != b.companionXPhysicalPx ||
            a.companionYPhysicalPx != b.companionYPhysicalPx ||
            a.backgroundXPhysicalPx != b.backgroundXPhysicalPx ||
            a.backgroundYPhysicalPx != b.backgroundYPhysicalPx)
            return false;
    }
    return true;
}

bool routeWideLineageValid(const TrajectoryInsetDrawPackage& package)
{
    constexpr double kEpsilon = 1e-7;
    if (package.sourceGroupDenominator != package.mappingRows.size() ||
        package.predictedDashEvents.empty()) return false;
    std::map<std::string, const TrajectoryInsetSourceMappingRow*> rows;
    std::map<std::string, const TrajectoryInsetDashEvent*> events;
    std::map<std::string, const TrajectoryInsetSemanticBatch*> batches;
    for (const auto& row : package.mappingRows)
        if (!rows.emplace(row.parentSourceKey, &row).second) return false;
    for (const auto& batch : package.semanticBatches)
        if (!batches.emplace(batch.insetMemberKey, &batch).second) return false;
    for (std::size_t index = 0; index < package.predictedDashEvents.size(); ++index) {
        const auto& event = package.predictedDashEvents[index];
        if (event.routeWideDashEventOrdinal != index || event.eventKey.empty() ||
            !events.emplace(event.eventKey, &event).second ||
            event.routeDistanceEndPx <= event.routeDistanceStartPx ||
            event.orderedContributors.empty()) return false;
        std::set<std::uint32_t> roleTokens;
        const std::array<TrajectoryInsetRole, 3> roles{{
            TrajectoryInsetRole::PredictedOuterRim,
            TrajectoryInsetRole::PredictedInnerHalo,
            TrajectoryInsetRole::PredictedDashCore}};
        for (std::size_t role = 0; role < roles.size(); ++role) {
            const auto member = batches.find(event.orderedRoleMemberKeys[role]);
            if (member == batches.end() || member->second->role != roles[role] ||
                member->second->parentSourceKey != event.eventKey ||
                member->second->insetOwnershipToken !=
                    event.orderedRoleTokens[role] ||
                event.orderedRoleTokens[role] == 0 ||
                !roleTokens.insert(event.orderedRoleTokens[role]).second)
                return false;
        }
        double cursor = event.routeDistanceStartPx;
        for (const auto& contributor : event.orderedContributors) {
            if (rows.count(contributor.parentSourceKey) == 0 ||
                contributor.sourceLocalTStart < 0.0 ||
                contributor.sourceLocalTEnd > 1.0 ||
                contributor.sourceLocalTEnd <= contributor.sourceLocalTStart ||
                std::abs(contributor.eventRouteDistanceStartPx - cursor) >
                    kEpsilon || contributor.eventRouteDistanceEndPx <= cursor)
                return false;
            cursor = contributor.eventRouteDistanceEndPx;
        }
        if (std::abs(cursor - event.routeDistanceEndPx) > kEpsilon) return false;
        std::size_t eventProbeCount = 0;
        for (const auto& probe : package.probes)
            if (probe.parentSourceKey == event.eventKey) {
                ++eventProbeCount;
                if (probe.insetMemberKey != event.orderedRoleMemberKeys[2] ||
                    probe.companionInsetMemberKey !=
                        event.orderedRoleMemberKeys[1] ||
                    probe.insetOwnershipToken != event.orderedRoleTokens[2])
                    return false;
            }
        if (eventProbeCount != 1) return false;
    }
    std::size_t predictedRows = 0;
    std::size_t maximumSpan = 0;
    for (const auto& row : package.mappingRows) {
        if (row.orderedPredictedRouteIntervals.empty()) continue;
        ++predictedRows;
        double localCursor = 0.0;
        double routeCursor = row.orderedPredictedRouteIntervals.front().
            routeDistanceStartPx;
        for (const auto& interval : row.orderedPredictedRouteIntervals) {
            const bool event = !interval.dashEventKey.empty();
            const bool gap = !interval.nonRenderedReason.empty();
            if (event == gap || std::abs(interval.sourceLocalTStart -
                    localCursor) > kEpsilon ||
                std::abs(interval.routeDistanceStartPx - routeCursor) >
                    kEpsilon || interval.sourceLocalTEnd <= localCursor ||
                interval.routeDistanceEndPx <= routeCursor ||
                interval.sourceLocalTEnd > 1.0 + kEpsilon)
                return false;
            if (event) {
                const auto found = events.find(interval.dashEventKey);
                if (found == events.end()) return false;
                bool exactContributor = false;
                for (const auto& contributor : found->second->orderedContributors)
                    exactContributor = exactContributor ||
                        (contributor.parentSourceKey == row.parentSourceKey &&
                        contributor.arcSegmentId == interval.arcSegmentId &&
                        contributor.routeOrderOrdinal ==
                            interval.routeOrderOrdinal &&
                        std::abs(contributor.sourceLocalTStart -
                            interval.sourceLocalTStart) <= kEpsilon &&
                        std::abs(contributor.sourceLocalTEnd -
                            interval.sourceLocalTEnd) <= kEpsilon &&
                        std::abs(contributor.eventRouteDistanceStartPx -
                            interval.routeDistanceStartPx) <= kEpsilon &&
                        std::abs(contributor.eventRouteDistanceEndPx -
                            interval.routeDistanceEndPx) <= kEpsilon);
                if (!exactContributor) return false;
            } else if (interval.nonRenderedReason !=
                    "predicted_dash_gap_by_route_wide_cadence") {
                return false;
            }
            maximumSpan = std::max(maximumSpan, interval.routeSpanOrdinal);
            localCursor = interval.sourceLocalTEnd;
            routeCursor = interval.routeDistanceEndPx;
        }
        if (std::abs(localCursor - 1.0) > kEpsilon) return false;
    }
    for (const auto& probe : package.probes) {
        const auto batch = batches.find(probe.insetMemberKey);
        if (batch != batches.end() &&
            batch->second->role == TrajectoryInsetRole::PredictedDashCore &&
            events.count(probe.parentSourceKey) == 0)
            return false;
    }
    return predictedRows != 0 && package.predictedRouteSpanCount ==
        maximumSpan + 1;
}

void auditRouteWideLineage(const TrajectoryInsetDrawPackage& package)
{
    expect(routeWideLineageValid(package),
        "route-wide dash event/source lineage is exact and complete");
    expect(exactDigestContractValid(package),
        "all five ordered digests equal independent canonical recomputation");
    expect(!package.orderedDashEventDigest.empty() &&
        !package.orderedDashContributorDigest.empty() &&
        !package.orderedSourceRouteIntervalDigest.empty(),
        "event, contributor, and source interval digests are frozen package outputs");
}

std::uint32_t tokenFor(const TrajectoryInsetDrawPackage& package,
    const std::string& parent, TrajectoryInsetRole role)
{
    for (const auto& batch : package.semanticBatches)
        if (batch.parentSourceKey == parent && batch.role == role)
            return batch.insetOwnershipToken;
    return 0;
}

bool tokenNear(const TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetSemanticRaster& raster, float physicalX,
    float physicalY, std::uint32_t token, int radius = 2)
{
    const int centerX = static_cast<int>(std::floor(
        physicalX - package.layout.plotRect.x));
    const int centerY = static_cast<int>(std::floor(
        physicalY - package.layout.plotRect.y));
    for (int y = centerY - radius; y <= centerY + radius; ++y)
        for (int x = centerX - radius; x <= centerX + radius; ++x) {
            if (x < 0 || y < 0 || x >= raster.width || y >= raster.height)
                continue;
            for (std::size_t sample = 0; sample < raster.sampleCount; ++sample) {
                const std::size_t offset = (static_cast<std::size_t>(y) *
                    raster.width + x) * raster.sampleCount + sample;
                if (raster.perSampleTokens[offset] == token) return true;
            }
        }
    return false;
}

void auditExactMemberAndMapping(const TrajectoryInsetDrawPackage& package)
{
    std::set<std::string> members;
    std::set<std::uint32_t> tokens;
    for (const auto& row : package.mappingRows) {
        expect(row.orderedWorldMemberKeys.empty() &&
            row.worldNotEmittedReason == "world_overlay_default_off",
            "default world mapping is explicit and exact");
        for (const auto& member : row.orderedInsetMemberKeys)
            expect(!member.empty(), "source-owned inset member is nonempty");
    }
    for (const auto& batch : package.semanticBatches) {
        expect(members.insert(batch.insetMemberKey).second,
            "semantic inset member unique");
        expect(batch.insetOwnershipToken != 0 &&
            batch.insetOwnershipToken != 0x80000001u &&
            tokens.insert(batch.insetOwnershipToken).second,
            "dedicated nonzero token is unique and avoids world token");
    }
    for (const auto& probe : package.probes) {
        expect(members.count(probe.insetMemberKey) == 1,
            "probe resolves one exact mapping member");
        expect(tokens.count(probe.insetOwnershipToken) == 1,
            "probe resolves one exact token");
        expect(std::abs(probe.backgroundXPhysicalPx -
                (package.layout.plotRect.x + 0.5f)) <= 1.0f &&
            std::abs(probe.backgroundYPhysicalPx -
                (package.layout.plotRect.y + 0.5f)) <= 1.0f,
            "probe background is the deterministic plot-gutter neutral sample");
    }
    expect(package.sourceGroupDenominator == package.mappingRows.size() &&
        package.semanticMemberDenominator == package.semanticBatches.size() &&
        package.probeDenominator == package.probes.size(),
        "all three denominators are exact inventories");
    expect(package.orientationLabels == std::array<std::string, 2>{{
        "HEAD / 开球端", "FOOT / 底端"}} &&
        package.orientationLabelsExcludedFromDenominator,
        "ordered bilingual labels are outside denominator");
    for (const auto& probe : package.probes)
        expect(probe.insetMemberKey.find("HEAD / 开球端") == std::string::npos &&
            probe.insetMemberKey.find("FOOT / 底端") == std::string::npos,
            "orientation labels never enter probes");
}

bool exactPixelHasToken(const TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetSemanticRaster& raster, int physicalX, int physicalY,
    std::uint32_t token)
{
    const int x = static_cast<int>(std::floor(
        physicalX - package.layout.plotRect.x));
    const int y = static_cast<int>(std::floor(
        physicalY - package.layout.plotRect.y));
    if (x < 0 || y < 0 || x >= raster.width || y >= raster.height) return false;
    for (std::size_t sample = 0; sample < raster.sampleCount; ++sample)
        if (raster.perSampleTokens[(static_cast<std::size_t>(y) * raster.width +
                x) * raster.sampleCount + sample] == token)
            return true;
    return false;
}

void auditProbePixels(const TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetSemanticRaster& raster)
{
    std::map<std::string, const TrajectoryInsetSemanticBatch*> byMember;
    for (const auto& batch : package.semanticBatches)
        byMember.emplace(batch.insetMemberKey, &batch);
    for (const auto& probe : package.probes) {
        const auto stroke = byMember.find(probe.insetMemberKey);
        const auto companion = byMember.find(probe.companionInsetMemberKey);
        expect(stroke != byMember.end() && companion != byMember.end(),
            "probe stroke and companion resolve exact semantic members");
        expect(stroke->second->insetOwnershipToken ==
                probe.insetOwnershipToken &&
            exactPixelHasToken(package, raster, probe.strokeXPhysicalPx,
                probe.strokeYPhysicalPx, probe.insetOwnershipToken),
            "probe stroke pixel carries exact token:" +
                stroke->second->primitiveId + ":samples=" +
                std::to_string(raster.sampleCount) + ":xy=" +
                std::to_string(probe.strokeXPhysicalPx) + "," +
                std::to_string(probe.strokeYPhysicalPx));
        const int companionLocalX = static_cast<int>(std::floor(
            probe.companionXPhysicalPx - package.layout.plotRect.x));
        const int companionLocalY = static_cast<int>(std::floor(
            probe.companionYPhysicalPx - package.layout.plotRect.y));
        const std::uint32_t observedCompanion = companionLocalX >= 0 &&
            companionLocalY >= 0 && companionLocalX < raster.width &&
            companionLocalY < raster.height ? raster.perSampleTokens[
                (static_cast<std::size_t>(companionLocalY) * raster.width +
                    companionLocalX) * raster.sampleCount] : 0xffffffffu;
        expect(exactPixelHasToken(package, raster,
                probe.companionXPhysicalPx, probe.companionYPhysicalPx,
                companion->second->insetOwnershipToken),
            "probe companion pixel carries declared token:stroke=" +
                stroke->second->primitiveId + ":companion=" +
                companion->second->primitiveId + ":samples=" +
                std::to_string(raster.sampleCount) + ":expected=" +
                std::to_string(companion->second->insetOwnershipToken) +
                ":observed0=" + std::to_string(observedCompanion) +
                ":xy=" + std::to_string(probe.companionXPhysicalPx) + "," +
                std::to_string(probe.companionYPhysicalPx) + ":vcount=" +
                std::to_string(companion->second->vertexCount) + ":x0=" +
                std::to_string(package.vertices[companion->second->firstVertex].xPhysicalPx) +
                ":x2=" + std::to_string(package.vertices[
                    companion->second->firstVertex + 2].xPhysicalPx));
        const int backgroundX = static_cast<int>(std::floor(
            probe.backgroundXPhysicalPx - package.layout.plotRect.x));
        const int backgroundY = static_cast<int>(std::floor(
            probe.backgroundYPhysicalPx - package.layout.plotRect.y));
        expect(backgroundX >= 0 && backgroundY >= 0 &&
            backgroundX < raster.width && backgroundY < raster.height,
            "fixed background sample is in plot gutter");
        for (std::size_t sample = 0; sample < raster.sampleCount; ++sample)
            expect(raster.perSampleTokens[(static_cast<std::size_t>(backgroundY) *
                    raster.width + backgroundX) * raster.sampleCount + sample] == 0,
                "fixed background sample remains neutral at every MSAA sample");
    }
}

int componentsForRole(const TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetSemanticRaster& raster, TrajectoryInsetRole role,
    std::size_t sample)
{
    std::set<std::uint32_t> tokens;
    for (const auto& batch : package.semanticBatches)
        if (batch.role == role) tokens.insert(batch.insetOwnershipToken);
    std::vector<unsigned char> mask(static_cast<std::size_t>(raster.width) *
        raster.height, 0);
    for (int y = 0; y < raster.height; ++y) for (int x = 0; x < raster.width; ++x) {
        const std::uint32_t token = raster.perSampleTokens[
            (static_cast<std::size_t>(y) * raster.width + x) *
                raster.sampleCount + sample];
        mask[static_cast<std::size_t>(y) * raster.width + x] =
            tokens.count(token) != 0;
    }
    int components = 0;
    const std::array<std::array<int, 2>, 8> neighbors{{
        {{-1,-1}},{{0,-1}},{{1,-1}},{{-1,0}},{{1,0}},
        {{-1,1}},{{0,1}},{{1,1}}}};
    for (int sy = 0; sy < raster.height; ++sy) for (int sx = 0; sx < raster.width; ++sx) {
        const std::size_t start = static_cast<std::size_t>(sy) * raster.width + sx;
        if (mask[start] != 1) continue;
        ++components;
        mask[start] = 2;
        std::queue<std::array<int, 2>> pending;
        pending.push({sx, sy});
        while (!pending.empty()) {
            const auto point = pending.front(); pending.pop();
            for (const auto& delta : neighbors) {
                const int x = point[0] + delta[0], y = point[1] + delta[1];
                if (x < 0 || y < 0 || x >= raster.width || y >= raster.height)
                    continue;
                const std::size_t offset = static_cast<std::size_t>(y) *
                    raster.width + x;
                if (mask[offset] != 1) continue;
                mask[offset] = 2;
                pending.push({x, y});
            }
        }
    }
    return components;
}

std::string componentDiagnostic(const TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetSemanticRaster& raster, TrajectoryInsetRole role,
    std::size_t sample)
{
    std::set<std::uint32_t> tokens;
    for (const auto& batch : package.semanticBatches)
        if (batch.role == role) tokens.insert(batch.insetOwnershipToken);
    std::vector<unsigned char> mask(static_cast<std::size_t>(raster.width) *
        raster.height, 0);
    for (int y = 0; y < raster.height; ++y) for (int x = 0; x < raster.width; ++x) {
        const std::uint32_t token = raster.perSampleTokens[
            (static_cast<std::size_t>(y) * raster.width + x) *
                raster.sampleCount + sample];
        mask[static_cast<std::size_t>(y) * raster.width + x] =
            tokens.count(token) != 0;
    }
    struct Component { int size = 0, minX = 0, minY = 0, maxX = 0, maxY = 0; };
    std::vector<Component> components;
    const std::array<std::array<int, 2>, 8> neighbors{{
        {{-1,-1}},{{0,-1}},{{1,-1}},{{-1,0}},{{1,0}},
        {{-1,1}},{{0,1}},{{1,1}}}};
    for (int sy = 0; sy < raster.height; ++sy) for (int sx = 0; sx < raster.width; ++sx) {
        const std::size_t start = static_cast<std::size_t>(sy) * raster.width + sx;
        if (mask[start] != 1) continue;
        Component component{0, sx, sy, sx, sy};
        mask[start] = 2;
        std::queue<std::array<int, 2>> pending;
        pending.push({sx, sy});
        while (!pending.empty()) {
            const auto point = pending.front(); pending.pop();
            ++component.size;
            component.minX = std::min(component.minX, point[0]);
            component.minY = std::min(component.minY, point[1]);
            component.maxX = std::max(component.maxX, point[0]);
            component.maxY = std::max(component.maxY, point[1]);
            for (const auto& delta : neighbors) {
                const int x = point[0] + delta[0], y = point[1] + delta[1];
                if (x < 0 || y < 0 || x >= raster.width || y >= raster.height)
                    continue;
                const std::size_t offset = static_cast<std::size_t>(y) *
                    raster.width + x;
                if (mask[offset] != 1) continue;
                mask[offset] = 2;
                pending.push({x, y});
            }
        }
        components.push_back(component);
    }
    std::vector<Component> componentsByX = components;
    std::sort(componentsByX.begin(), componentsByX.end(), [](const Component& a,
        const Component& b) { return a.minX < b.minX; });
    std::sort(components.begin(), components.end(), [](const Component& a,
        const Component& b) { return a.size > b.size; });
    std::string result = std::string(trajectoryInsetRoleName(role)) +
        ":components=" + std::to_string(components.size());
    for (std::size_t i = 0; i < std::min<std::size_t>(components.size(), 6); ++i)
        result += ":c" + std::to_string(i) + "=" +
            std::to_string(components[i].size) + "@[" +
            std::to_string(components[i].minX) + "," +
            std::to_string(components[i].minY) + ".." +
            std::to_string(components[i].maxX) + "," +
            std::to_string(components[i].maxY) + "]";
    std::map<std::uint32_t, std::string> tokenOwner;
    for (const auto& batch : package.semanticBatches)
        tokenOwner.emplace(batch.insetOwnershipToken,
            batch.primitiveId + "/" + trajectoryInsetRoleName(batch.role));
    for (std::size_t i = 1; i < componentsByX.size(); ++i) {
        const Component& before = componentsByX[i - 1];
        const Component& after = componentsByX[i];
        if (before.maxX + 1 >= after.minX) continue;
        std::set<std::uint32_t> owners;
        const int minY = std::min(before.minY, after.minY) - 1;
        const int maxY = std::max(before.maxY, after.maxY) + 1;
        for (int y = std::max(0, minY); y <= std::min(raster.height - 1, maxY); ++y)
            for (int x = before.maxX + 1; x < after.minX; ++x) {
                const std::uint32_t token = raster.perSampleTokens[
                    (static_cast<std::size_t>(y) * raster.width + x) *
                        raster.sampleCount + sample];
                if (token != 0) owners.insert(token);
            }
        result += ":gap[" + std::to_string(before.maxX + 1) + ".." +
            std::to_string(after.minX - 1) + "]owners=";
        if (owners.empty()) result += "zero";
        for (std::uint32_t token : owners)
            result += std::to_string(token) + "(" + tokenOwner[token] + "),";
    }
    return result;
}

std::pair<int, int> tokenAreaAndLargest8(
    const TrajectoryInsetSemanticRaster& raster, std::uint32_t token,
    std::size_t sample)
{
    std::vector<unsigned char> mask(static_cast<std::size_t>(raster.width) *
        raster.height, 0);
    int area = 0, largest = 0;
    for (int y = 0; y < raster.height; ++y) for (int x = 0; x < raster.width; ++x) {
        const std::size_t pixel = static_cast<std::size_t>(y) * raster.width + x;
        if (raster.perSampleTokens[pixel * raster.sampleCount + sample] == token) {
            mask[pixel] = 1;
            ++area;
        }
    }
    const std::array<std::array<int, 2>, 8> neighbors{{
        {{-1,-1}},{{0,-1}},{{1,-1}},{{-1,0}},{{1,0}},
        {{-1,1}},{{0,1}},{{1,1}}}};
    for (int sy = 0; sy < raster.height; ++sy) for (int sx = 0; sx < raster.width; ++sx) {
        const std::size_t start = static_cast<std::size_t>(sy) * raster.width + sx;
        if (mask[start] != 1) continue;
        int size = 0;
        mask[start] = 2;
        std::queue<std::array<int, 2>> pending;
        pending.push({sx, sy});
        while (!pending.empty()) {
            const auto point = pending.front(); pending.pop();
            ++size;
            for (const auto& delta : neighbors) {
                const int x = point[0] + delta[0], y = point[1] + delta[1];
                if (x < 0 || y < 0 || x >= raster.width || y >= raster.height)
                    continue;
                const std::size_t pixel = static_cast<std::size_t>(y) *
                    raster.width + x;
                if (mask[pixel] != 1) continue;
                mask[pixel] = 2;
                pending.push({x, y});
            }
        }
        largest = std::max(largest, size);
    }
    return {area, largest};
}

TrajectoryInsetSemanticRaster isolatedTokenRaster(
    const TrajectoryInsetDrawPackage& source,
    const std::set<std::uint32_t>& included,
    const TrajectoryInsetSampleProfile& profile)
{
    TrajectoryInsetDrawPackage isolated;
    isolated.ok = true;
    isolated.layout = source.layout;
    for (std::size_t i = 0; i + 2 < source.vertices.size(); i += 3)
        if (included.count(source.vertices[i].insetOwnershipToken) != 0) {
            isolated.vertices.push_back(source.vertices[i]);
            isolated.vertices.push_back(source.vertices[i + 1]);
            isolated.vertices.push_back(source.vertices[i + 2]);
        }
    return rasterTrajectoryInsetSemantics(isolated, profile);
}

void auditOrientationAndGutter(const TrajectoryInsetDrawPackage& package,
    bool reference)
{
    const auto& table = package.canonicalTableRect;
    const auto& plot = package.layout.plotRect;
    expect(std::abs(table.width / 254.0f - table.height / 127.0f) < 1e-6f,
        "canonical mapping uses uniform scale");
    expect(table.x - plot.x >= 12.0f * package.layout.uiScale - 1e-4f &&
        table.y - plot.y >= 12.0f * package.layout.uiScale - 1e-4f &&
        plot.x + plot.width - table.x - table.width >=
            12.0f * package.layout.uiScale - 1e-4f &&
        plot.y + plot.height - table.y - table.height >=
            12.0f * package.layout.uiScale - 1e-4f,
        "canonical table has symmetric complete-silhouette clearance");
    if (reference) {
        expect(std::abs(table.x - (plot.x + 24.0f)) < 1e-5f &&
            std::abs(table.y - (plot.y + 12.0f)) < 1e-5f &&
            std::abs(table.width - 368.0f) < 1e-5f &&
            std::abs(table.height - 184.0f) < 1e-5f,
            "reference canonical table rect is exact [24,12,368,184]");
    }
    // Full semantic silhouette, including endpoint markers, remains in plot.
    for (const auto& vertex : package.vertices)
        expect(vertex.xPhysicalPx >= plot.x - 1e-4f &&
            vertex.yPhysicalPx >= plot.y - 1e-4f &&
            vertex.xPhysicalPx <= plot.x + plot.width + 1e-4f &&
            vertex.yPhysicalPx <= plot.y + plot.height + 1e-4f,
            "complete silhouette remains inside plot:profile=" +
                package.layout.profileId + ":vertex=" +
                std::to_string(vertex.xPhysicalPx) + "," +
                std::to_string(vertex.yPhysicalPx) + ":plot=" +
                std::to_string(plot.x) + "," + std::to_string(plot.y) + "," +
                std::to_string(plot.width) + "," +
                std::to_string(plot.height));
}

void auditRailsAndDash(const TrajectoryInsetDrawPackage& package,
    const TrajectoryInsetSemanticRaster& raster,
    bool requireHorizontalCadenceScan = true)
{
    auditProbePixels(package, raster);
    for (std::size_t sample = 0; sample < raster.sampleCount; ++sample) {
        const int left = componentsForRole(package, raster,
            TrajectoryInsetRole::ActualLeftCore, sample);
        const int right = componentsForRole(package, raster,
            TrajectoryInsetRole::ActualRightCore, sample);
        expect(left == 1 && right == 1,
            "each actual rail is one 8-connected component through every join:" +
            package.layout.profileId + ":sampleProfile=" +
            std::to_string(raster.sampleCount) + ":sampleLayer=" +
            std::to_string(sample) + ":" +
            componentDiagnostic(package, raster,
                TrajectoryInsetRole::ActualLeftCore, sample) + ":" +
            componentDiagnostic(package, raster,
                TrajectoryInsetRole::ActualRightCore, sample));
    }
    expect(!package.actualMidpointWitnesses.empty(), "actual midpoint witnesses exist");
    for (const auto& witness : package.actualMidpointWitnesses) {
        const std::uint32_t left = tokenFor(package, witness.parentSourceKey,
            TrajectoryInsetRole::ActualLeftCore);
        const std::uint32_t right = tokenFor(package, witness.parentSourceKey,
            TrajectoryInsetRole::ActualRightCore);
        expect(left != 0 && right != 0 && left != right,
            "both actual rails retain distinct semantic identities");
        for (float t : {0.1f, 0.35f, 0.65f, 0.9f}) {
            const auto interpolate = [t](const std::array<float, 2>& a,
                const std::array<float, 2>& b) {
                return std::array<float, 2>{{a[0] + (b[0] - a[0]) * t,
                    a[1] + (b[1] - a[1]) * t}};
            };
            const auto canonical = interpolate(witness.canonicalFirst,
                witness.canonicalSecond);
            const auto leftPoint = interpolate(witness.leftFirst,
                witness.leftSecond);
            const auto rightPoint = interpolate(witness.rightFirst,
                witness.rightSecond);
            expect(std::abs((leftPoint[0] + rightPoint[0]) * 0.5f -
                    canonical[0]) < 1e-5f &&
                std::abs((leftPoint[1] + rightPoint[1]) * 0.5f -
                    canonical[1]) < 1e-5f,
                "actual rail midpoint equals canonical route exactly");
            expect(tokenNear(package, raster, leftPoint[0], leftPoint[1], left) &&
                tokenNear(package, raster, rightPoint[0], rightPoint[1], right),
                "both actual rails are sample-resolved continuous");
        }
    }
    std::set<std::uint32_t> predictedCoreTokens;
    std::size_t predictedEndpointCount = 0;
    for (const auto& batch : package.semanticBatches) {
        if (batch.role == TrajectoryInsetRole::PredictedDashCore) {
            predictedCoreTokens.insert(batch.insetOwnershipToken);
            const auto& a = package.vertices.at(batch.firstVertex);
            const auto& c = package.vertices.at(batch.firstVertex + 2);
            expect(tokenNear(package, raster, (a.xPhysicalPx + c.xPhysicalPx) * 0.5f,
                    (a.yPhysicalPx + c.yPhysicalPx) * 0.5f,
                    batch.insetOwnershipToken),
                "predicted center dash core is dominant at its center");
        } else if (batch.role == TrajectoryInsetRole::PredictedEndpointDiamond) {
            ++predictedEndpointCount;
        }
    }
    expect(predictedCoreTokens.size() >= 8 && predictedEndpointCount == 1,
        "predicted center dash rhythm and endpoint remain present");

    const int y = static_cast<int>(std::floor(
        package.canonicalTableRect.y + package.canonicalTableRect.height * 0.5f -
        package.layout.plotRect.y));
    int dashPixels = 0, gapPixels = 0, transitions = 0;
    bool previous = false;
    for (int x = 0; x < raster.width; ++x) {
        bool core = false;
        for (std::size_t sample = 0; sample < raster.sampleCount; ++sample) {
            const std::uint32_t token = raster.perSampleTokens[
                (static_cast<std::size_t>(y) * raster.width + x) *
                    raster.sampleCount + sample];
            core = core || predictedCoreTokens.count(token) != 0;
        }
        if (core) ++dashPixels; else ++gapPixels;
        if (x > 0 && core != previous) ++transitions;
        previous = core;
    }
    expect(!requireHorizontalCadenceScan ||
        (dashPixels > 0 && gapPixels > 0 && transitions >= 8),
        "predicted dash-on and dash-gap rhythm crosses source joins:profile=" +
        package.layout.profileId + ":dash=" + std::to_string(dashPixels) +
        ":gap=" + std::to_string(gapPixels) + ":transitions=" +
        std::to_string(transitions));
}

void testExact96Cases()
{
    const std::array<std::string, 4> cameras{{
        "player_low", "elevated", "overhead", "broadcast"}};
    const std::array<TrajectoryInsetPalette, 2> palettes{{
        TrajectoryInsetPalette::Standard,
        TrajectoryInsetPalette::ColorVisionSafe}};
    const std::array<TrajectoryInsetSampleProfile, 3> samples{{
        trajectoryInsetSampleProfile1x(), trajectoryInsetSampleProfile2x(),
        trajectoryInsetSampleProfile4x()}};
    std::size_t caseCount = 0;
    for (std::size_t layoutIndex = 0; layoutIndex < 4; ++layoutIndex) {
        std::string cameraIndependentDigest;
        std::string standardTopology;
        for (const std::string& camera : cameras) {
            (void)camera; // Deliberately output-only: no camera enters builder.
            TrajectoryInsetDrawPackage standardPackage;
            for (TrajectoryInsetPalette palette : palettes) {
                const TrajectoryInsetDrawPackage package =
                    buildTrajectoryInsetDrawPackage(requestFor(layoutIndex, palette));
                const auto& failedPlot = package.layout.plotRect;
                const auto& failedTable = package.canonicalTableRect;
                const float failedXScale = failedTable.width / 254.0f;
                const float failedYScale = failedTable.height / 127.0f;
                expect(package.ok, "96-case package builds:layout=" +
                    layout(layoutIndex).profileId + ":camera=" + camera +
                    ":palette=" + std::to_string(static_cast<int>(palette)) +
                    ":error=" + package.errorCode + ":member=" +
                    package.probeResolutionFailureMember + ":xScale=" +
                    std::to_string(failedXScale) + ":yScale=" +
                    std::to_string(failedYScale) + ":diff=" +
                    std::to_string(std::abs(failedXScale - failedYScale)) +
                    ":margins=" +
                    std::to_string(failedTable.x - failedPlot.x) + "," +
                    std::to_string(failedTable.y - failedPlot.y) + "," +
                    std::to_string(failedPlot.x + failedPlot.width -
                        failedTable.x - failedTable.width) + "," +
                    std::to_string(failedPlot.y + failedPlot.height -
                        failedTable.y - failedTable.height) + ":minimum=" +
                    std::to_string(12.0f * package.layout.uiScale));
                auditExactMemberAndMapping(package);
                auditRouteWideLineage(package);
                auditOrientationAndGutter(package, layoutIndex == 0);
                const std::string digest = topologyDigest(package);
                if (cameraIndependentDigest.empty()) cameraIndependentDigest = digest;
                expect(digest == cameraIndependentDigest,
                    "camera and palette do not alter canonical topology/mapping");
                if (palette == TrajectoryInsetPalette::Standard) {
                    standardTopology = digest;
                    standardPackage = package;
                } else {
                    expect(digest == standardTopology,
                        "standard/CVD semantic topology is byte-identical");
                    expect(probeInventoryEqual(standardPackage, package),
                        "standard/CVD probe IDs, members, tokens, and coordinates are exact");
                }
                std::set<std::string> sampleDigests;
                for (const auto& sample : samples) {
                    const TrajectoryInsetSemanticRaster raster =
                        rasterTrajectoryInsetSemantics(package, sample);
                    expect(raster.ok && raster.sampleCount == sample.positions.size(),
                        "real sample profile raster builds");
                    expect(sampleDigests.insert(raster.tokenDigest).second,
                        "1x/2x/4x are independently rasterized, not relabeled");
                    auditRailsAndDash(package, raster);
                    ++caseCount;
                }
            }
        }
    }
    expect(caseCount == 96, "exact layout-major camera/palette/sample matrix is 96");
}

bool endpointLockedGeometryValid(const TrajectoryInsetDrawPackage& package)
{
    std::vector<const TrajectoryInsetSemanticBatch*> capsules;
    const TrajectoryInsetSemanticBatch* marker = nullptr;
    const TrajectoryInsetSemanticBatch* halo = nullptr;
    for (const auto& batch : package.semanticBatches) {
        if (batch.role == TrajectoryInsetRole::ActualEndpointCapsule)
            capsules.push_back(&batch);
        else if (batch.role == TrajectoryInsetRole::PredictedEndpointDiamond)
            marker = &batch;
        else if (batch.role == TrajectoryInsetRole::PredictedInnerHalo &&
            batch.primitiveId.find("endpoint_inner_halo") != std::string::npos)
            halo = &batch;
    }
    if (capsules.size() != 2 || marker == nullptr || halo == nullptr)
        return false;
    const auto boundsFor = [&](const TrajectoryInsetSemanticBatch& batch) {
        std::array<float, 4> result{{
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity()}};
        for (std::size_t i = batch.firstVertex;
             i < batch.firstVertex + batch.vertexCount; ++i) {
            if (i >= package.vertices.size() ||
                package.vertices[i].insetOwnershipToken !=
                    batch.insetOwnershipToken)
                return std::array<float, 4>{{0, 0, 0, 0}};
            result[0] = std::min(result[0], package.vertices[i].xPhysicalPx);
            result[1] = std::min(result[1], package.vertices[i].yPhysicalPx);
            result[2] = std::max(result[2], package.vertices[i].xPhysicalPx);
            result[3] = std::max(result[3], package.vertices[i].yPhysicalPx);
        }
        return result;
    };
    const auto haloBounds = boundsFor(*halo);
    const auto markerBounds = boundsFor(*marker);
    const std::array<float, 2> center{{
        (haloBounds[0] + haloBounds[2]) * 0.5f,
        (haloBounds[1] + haloBounds[3]) * 0.5f}};
    if (std::abs(haloBounds[2] - haloBounds[0] - 9.0f) > 1e-4f ||
        std::abs(haloBounds[3] - haloBounds[1] - 9.0f) > 1e-4f ||
        std::abs(markerBounds[2] - markerBounds[0] - 7.0f) > 1e-4f ||
        std::abs(markerBounds[3] - markerBounds[1] - 7.0f) > 1e-4f)
        return false;
    const auto bandValid = [&](const TrajectoryInsetSemanticBatch& batch,
        float outer, float inner) {
        bool sawOuter = false, sawInner = false;
        for (std::size_t i = batch.firstVertex;
             i < batch.firstVertex + batch.vertexCount; ++i) {
            const float l1 = std::abs(package.vertices[i].xPhysicalPx - center[0]) +
                std::abs(package.vertices[i].yPhysicalPx - center[1]);
            const bool atOuter = std::abs(l1 - outer) <= 1e-4f;
            const bool atInner = std::abs(l1 - inner) <= 1e-4f;
            if (!atOuter && !atInner) return false;
            sawOuter = sawOuter || atOuter;
            sawInner = sawInner || atInner;
        }
        return sawOuter && sawInner;
    };
    if (!bandValid(*halo, 4.5f, 3.5f) ||
        !bandValid(*marker, 3.5f, 2.5f)) return false;
    std::array<std::array<float, 2>, 2> capsuleCenters;
    for (std::size_t i = 0; i < capsules.size(); ++i) {
        const auto bounds = boundsFor(*capsules[i]);
        const float width = bounds[2] - bounds[0];
        const float height = bounds[3] - bounds[1];
        if (std::abs(std::max(width, height) - 7.0f) > 1e-4f ||
            std::abs(std::min(width, height) - 2.5f) > 1e-4f)
            return false;
        capsuleCenters[i] = {{(bounds[0] + bounds[2]) * 0.5f,
            (bounds[1] + bounds[3]) * 0.5f}};
        const float dx = capsuleCenters[i][0] - center[0];
        const float dy = capsuleCenters[i][1] - center[1];
        if (std::abs(std::sqrt(dx * dx + dy * dy) - 7.75f) > 1e-4f)
            return false;
    }
    if (std::abs((capsuleCenters[0][0] + capsuleCenters[1][0]) * 0.5f -
            center[0]) > 1e-6f ||
        std::abs((capsuleCenters[0][1] + capsuleCenters[1][1]) * 0.5f -
            center[1]) > 1e-6f) return false;
    const auto& tail = package.predictedDashEvents.back();
    const auto findBatch = [&](const std::string& member) {
        return std::find_if(package.semanticBatches.begin(),
            package.semanticBatches.end(), [&](const auto& batch) {
                return batch.insetMemberKey == member;
            });
    };
    const auto outer = findBatch(tail.orderedRoleMemberKeys[0]);
    const auto inner = findBatch(tail.orderedRoleMemberKeys[1]);
    const auto core = findBatch(tail.orderedRoleMemberKeys[2]);
    return outer != package.semanticBatches.end() &&
        inner != package.semanticBatches.end() &&
        core != package.semanticBatches.end() &&
        outer->firstVertex < inner->firstVertex &&
        inner->firstVertex < core->firstVertex &&
        core->firstVertex < halo->firstVertex &&
        halo->firstVertex < marker->firstVertex;
}

void auditEndpointOwnershipGrammar(const TrajectoryInsetDrawPackage& package)
{
    expect(endpointLockedGeometryValid(package),
        "endpoint geometry and draw order match the frozen addendum");
    std::vector<std::uint32_t> capsules;
    std::uint32_t marker = 0, markerHalo = 0;
    std::map<std::uint32_t, const TrajectoryInsetSemanticBatch*> batchByToken;
    for (const auto& batch : package.semanticBatches) {
        batchByToken.emplace(batch.insetOwnershipToken, &batch);
        if (batch.role == TrajectoryInsetRole::ActualEndpointCapsule)
            capsules.push_back(batch.insetOwnershipToken);
        else if (batch.role == TrajectoryInsetRole::PredictedEndpointDiamond)
            marker = batch.insetOwnershipToken;
        else if (batch.role == TrajectoryInsetRole::PredictedInnerHalo &&
            batch.primitiveId.find("endpoint_inner_halo") != std::string::npos)
            markerHalo = batch.insetOwnershipToken;
    }
    expect(capsules.size() == 2 && marker != 0 && markerHalo != 0 &&
        !package.predictedDashEvents.empty(),
        "endpoint grammar has paired capsules, hollow marker and tail event");
    const std::uint32_t tailEventCore =
        package.predictedDashEvents.back().orderedRoleTokens[2];
    const auto& tailEvent = package.predictedDashEvents.back();
    expect(std::abs(tailEvent.routeDistanceStartPx - 550.0) < 1e-6 &&
        std::abs(tailEvent.routeDistanceEndPx - 552.0) < 1e-6,
        "edge/corner retains the complete [550,552) cadence tail interval");
    const auto probeCountForMember = [&](const std::string& member) {
        return std::count_if(package.probes.begin(), package.probes.end(),
            [&](const auto& probe) { return probe.insetMemberKey == member; });
    };
    expect(probeCountForMember(tailEvent.orderedRoleMemberKeys[2]) == 1 &&
        probeCountForMember(batchByToken.at(marker)->insetMemberKey) == 1 &&
        probeCountForMember(batchByToken.at(capsules[0])->insetMemberKey) == 1 &&
        probeCountForMember(batchByToken.at(capsules[1])->insetMemberKey) == 1,
        "tail event, marker, and both capsules each retain one exact probe");
    const std::set<std::uint32_t> actualEndpointTokens(capsules.begin(),
        capsules.end());
    const std::set<std::uint32_t> predictedOutlineTokens{marker, markerHalo};
    int totalBoundedAnalyticOverlap = 0;
    const auto bounds = [&](std::uint32_t token) {
        const auto* batch = batchByToken.at(token);
        std::array<float, 4> value{{
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity()}};
        for (std::size_t i = batch->firstVertex;
             i < batch->firstVertex + batch->vertexCount; ++i) {
            value[0] = std::min(value[0], package.vertices[i].xPhysicalPx);
            value[1] = std::min(value[1], package.vertices[i].yPhysicalPx);
            value[2] = std::max(value[2], package.vertices[i].xPhysicalPx);
            value[3] = std::max(value[3], package.vertices[i].yPhysicalPx);
        }
        return value;
    };
    const auto haloBounds = bounds(markerHalo);
    const auto markerBounds = bounds(marker);
    const std::array<float, 2> endpointCenter{{
        (haloBounds[0] + haloBounds[2]) * 0.5f,
        (haloBounds[1] + haloBounds[3]) * 0.5f}};
    expect(std::abs(haloBounds[2] - haloBounds[0] - 9.0f) < 1e-5f &&
        std::abs(haloBounds[3] - haloBounds[1] - 9.0f) < 1e-5f &&
        std::abs(markerBounds[2] - markerBounds[0] - 7.0f) < 1e-5f &&
        std::abs(markerBounds[3] - markerBounds[1] - 7.0f) < 1e-5f,
        "hollow endpoint outer bands lock supports 4.5 and 3.5");
    const auto requireBandRadii = [&](std::uint32_t token, float outer,
        float inner) {
        const auto* batch = batchByToken.at(token);
        bool sawOuter = false, sawInner = false;
        for (std::size_t i = batch->firstVertex;
             i < batch->firstVertex + batch->vertexCount; ++i) {
            const float l1 = std::abs(package.vertices[i].xPhysicalPx -
                    endpointCenter[0]) +
                std::abs(package.vertices[i].yPhysicalPx - endpointCenter[1]);
            sawOuter = sawOuter || std::abs(l1 - outer) < 1e-4f;
            sawInner = sawInner || std::abs(l1 - inner) < 1e-4f;
            expect(std::abs(l1 - outer) < 1e-4f ||
                std::abs(l1 - inner) < 1e-4f,
                "diamond band vertices use only frozen L1 radii");
        }
        expect(sawOuter && sawInner, "diamond band contains both closed radii");
    };
    requireBandRadii(markerHalo, 4.5f, 3.5f);
    requireBandRadii(marker, 3.5f, 2.5f);
    std::array<std::array<float, 2>, 2> capsuleCenters;
    for (std::size_t i = 0; i < capsules.size(); ++i) {
        const auto capsuleBounds = bounds(capsules[i]);
        const float width = capsuleBounds[2] - capsuleBounds[0];
        const float height = capsuleBounds[3] - capsuleBounds[1];
        expect(std::abs(std::max(width, height) - 7.0f) < 1e-4f &&
            std::abs(std::min(width, height) - 2.5f) < 1e-4f,
            "actual capsule locks 7px tangent by 2.5px normal extent");
        capsuleCenters[i] = {{(capsuleBounds[0] + capsuleBounds[2]) * 0.5f,
            (capsuleBounds[1] + capsuleBounds[3]) * 0.5f}};
        const float dx = capsuleCenters[i][0] - endpointCenter[0];
        const float dy = capsuleCenters[i][1] - endpointCenter[1];
        expect(std::abs(std::sqrt(dx * dx + dy * dy) - 7.75f) < 1e-4f,
            "actual capsule center uses symmetric 7.75px offset");
    }
    expect(std::abs((capsuleCenters[0][0] + capsuleCenters[1][0]) * 0.5f -
            endpointCenter[0]) < 1e-6f &&
        std::abs((capsuleCenters[0][1] + capsuleCenters[1][1]) * 0.5f -
            endpointCenter[1]) < 1e-6f,
        "paired capsule midpoint remains canonical endpoint");
    for (const auto& profile : {trajectoryInsetSampleProfile1x(),
            trajectoryInsetSampleProfile2x(),
            trajectoryInsetSampleProfile4x()}) {
        const auto finalRaster = rasterTrajectoryInsetSemantics(package, profile);
        const auto repeatedRaster = rasterTrajectoryInsetSemantics(package, profile);
        const auto actualOnly = isolatedTokenRaster(package,
            actualEndpointTokens, profile);
        const auto outlineOnly = isolatedTokenRaster(package,
            predictedOutlineTokens, profile);
        const auto tailOnly = isolatedTokenRaster(package,
            {tailEventCore}, profile);
        expect(finalRaster.ok && actualOnly.ok && outlineOnly.ok && tailOnly.ok &&
            repeatedRaster.ok &&
            repeatedRaster.perSampleTokens == finalRaster.perSampleTokens &&
            repeatedRaster.tokenDigest == finalRaster.tokenDigest,
            "endpoint final and isolated masks rasterize");
        for (std::size_t sample = 0; sample < finalRaster.sampleCount; ++sample) {
            int isolatedTailOutlineOverlap = 0;
            int overlapOwnedByLaterEndpoint = 0;
            int firstOverlapX = -1, firstOverlapY = -1;
            std::uint32_t firstTailToken = 0, firstOutlineToken = 0;
            for (std::uint32_t token : capsules) {
                const auto stats = tokenAreaAndLargest8(finalRaster, token, sample);
                expect(stats.first >= 4 && stats.second >= 2,
                    "each actual endpoint capsule retains exact-token area");
            }
            const auto markerStats = tokenAreaAndLargest8(finalRaster, marker,
                sample);
            const auto haloStats = tokenAreaAndLargest8(finalRaster, markerHalo,
                sample);
            const auto tailStats = tokenAreaAndLargest8(finalRaster,
                tailEventCore, sample);
            expect(markerStats.first >= 4 && markerStats.second >= 2 &&
                haloStats.first >= 4 && haloStats.second >= 2 &&
                tailStats.first >= 2 && tailStats.second >= 1,
                "hollow marker bands and full-cadence tail event remain exact");
            for (std::size_t pixel = 0;
                 pixel < static_cast<std::size_t>(finalRaster.width) *
                    finalRaster.height; ++pixel) {
                const std::uint32_t actualToken = actualOnly.perSampleTokens[
                    pixel * actualOnly.sampleCount + sample];
                const std::uint32_t outlineToken = outlineOnly.perSampleTokens[
                    pixel * outlineOnly.sampleCount + sample];
                expect(actualToken == 0 || outlineToken == 0,
                    "actual capsules and predicted complete outline have zero geometry overlap");
                const std::uint32_t tailToken = tailOnly.perSampleTokens[
                    pixel * tailOnly.sampleCount + sample];
                if (tailToken != 0 && outlineToken != 0) {
                    if (firstOverlapX < 0) {
                        firstOverlapX = static_cast<int>(pixel % finalRaster.width);
                        firstOverlapY = static_cast<int>(pixel / finalRaster.width);
                        firstTailToken = tailToken;
                        firstOutlineToken = outlineToken;
                    }
                    ++isolatedTailOutlineOverlap;
                    const std::uint32_t finalOwner =
                        finalRaster.perSampleTokens[
                            pixel * finalRaster.sampleCount + sample];
                    overlapOwnedByLaterEndpoint += finalOwner == marker ||
                        finalOwner == markerHalo;
                }
            }
            totalBoundedAnalyticOverlap += isolatedTailOutlineOverlap;
            expect(overlapOwnedByLaterEndpoint == isolatedTailOutlineOverlap &&
                (isolatedTailOutlineOverlap == 0 ||
                    (firstOverlapX >= 0 && firstTailToken == tailEventCore &&
                    (firstOutlineToken == marker ||
                        firstOutlineToken == markerHalo))),
                "bounded analytic crossing is deterministically owned by later endpoint bands");
        }
    }
    expect(totalBoundedAnalyticOverlap > 0,
        "closed endpoint has the expected bounded analytic centerline crossing");
}

void testEdgeCornerAndWorldMapping()
{
    TrajectoryInsetBuildRequest edgeRequest = requestFor(0,
        TrajectoryInsetPalette::Standard, edgeCornerFixture());
    const TrajectoryInsetDrawPackage edge =
        buildTrajectoryInsetDrawPackage(edgeRequest);
    expect(edge.ok, "edge/corner package builds:error=" + edge.errorCode +
        ":member=" + edge.probeResolutionFailureMember);
    auditRouteWideLineage(edge);
    auditEndpointOwnershipGrammar(edge);
    auditOrientationAndGutter(edge, true);
    for (const auto& sample : {trajectoryInsetSampleProfile1x(),
            trajectoryInsetSampleProfile2x(),
            trajectoryInsetSampleProfile4x()}) {
        const auto raster = rasterTrajectoryInsetSemantics(edge, sample);
        expect(raster.ok, "edge/corner silhouettes raster at every sample count");
        auditRailsAndDash(edge, raster, false);
    }
    const auto edgeCvd = buildTrajectoryInsetDrawPackage(requestFor(0,
        TrajectoryInsetPalette::ColorVisionSafe, edgeCornerFixture()));
    expect(edgeCvd.ok && topologyDigest(edgeCvd) == topologyDigest(edge),
        "edge endpoint standard/CVD topology and lineage are identical");
    expect(probeInventoryEqual(edge, edgeCvd),
        "edge endpoint standard/CVD probe inventories are exact");
    auditEndpointOwnershipGrammar(edgeCvd);
    for (const auto& profile : {trajectoryInsetSampleProfile1x(),
            trajectoryInsetSampleProfile2x(),
            trajectoryInsetSampleProfile4x()}) {
        const auto standardRaster = rasterTrajectoryInsetSemantics(edge, profile);
        const auto cvdRaster = rasterTrajectoryInsetSemantics(edgeCvd, profile);
        expect(standardRaster.ok && cvdRaster.ok &&
            standardRaster.perSampleTokens == cvdRaster.perSampleTokens &&
            standardRaster.tokenDigest == cvdRaster.tokenDigest,
            "endpoint ownership is palette-independent for every real sample profile");
    }

    TrajectoryInsetBuildRequest worldOn = requestFor(0,
        TrajectoryInsetPalette::Standard);
    const auto baseline = buildTrajectoryInsetDrawPackage(worldOn);
    CameraPresentation overhead;
    overhead.eye = {0.0f, 520.0f, 0.0f};
    overhead.target = {0.0f, 86.5f, 0.0f};
    overhead.fieldOfViewDegrees = 48.0f;
    const TrajectoryOverlayDrawPackage worldPackage =
        buildTrajectoryOverlayDrawPackage(worldOn.snapshot, overhead,
            {1920, 1080, 1.0, 1.0},
            TrajectoryOverlayPalette::Standard);
    expect(worldPackage.ok, "real main-world package builds for mapping import");
    worldOn.worldNotEmittedReason.clear();
    for (const auto& parent : baseline.orderedParentSourceKeys) {
        TrajectoryInsetBuildRequest::WorldMappingInput input;
        input.parentSourceKey = parent;
        for (const auto& batch : worldPackage.semanticBatches) {
            const std::string track = batch.track ==
                TrajectoryContrastTrack::Actual ? "actual" : "predicted";
            if (parent.find("|" + track + "|" + batch.arcSegmentId + "|") ==
                std::string::npos) continue;
            input.orderedWorldMemberKeys.push_back(parent + "|world_v11|" +
                batch.primitiveId + "|" + batch.arcSegmentId + "|" +
                std::to_string(static_cast<int>(batch.componentRole)) + "|" +
                std::to_string(static_cast<int>(batch.visualSublayer)) + "|" +
                std::to_string(batch.finalOwnershipToken));
            worldOn.forbiddenWorldOwnershipTokens.push_back(
                batch.finalOwnershipToken);
        }
        if (input.orderedWorldMemberKeys.empty())
            input.worldNotEmittedReason = "world_not_emitted_by_projection";
        worldOn.worldMappings.push_back(input);
    }
    const auto mapped = buildTrajectoryInsetDrawPackage(worldOn);
    expect(mapped.ok && mapped.mappingRows.size() == baseline.mappingRows.size(),
        "optional diagnostic world-on mapping is exact and complete");
    for (const auto& row : mapped.mappingRows)
        expect((!row.orderedWorldMemberKeys.empty()) !=
                (!row.worldNotEmittedReason.empty()),
            "every real world row has exact members xor explicit projection reason");

    auto partial = worldOn;
    partial.worldMappings.pop_back();
    expect(!buildTrajectoryInsetDrawPackage(partial).ok,
        "partial world mapping fails closed");
    auto foreign = worldOn;
    foreign.worldMappings.push_back({"foreign_generation|actual|actual_arc_0",
        {"foreign_world_member"}, {}});
    expect(!buildTrajectoryInsetDrawPackage(foreign).ok,
        "foreign/mixed generation world mapping fails closed");
}

void testRouteWideResidualGapAndSpanContracts()
{
    const auto ui135 = buildTrajectoryInsetDrawPackage(requestFor(1,
        TrajectoryInsetPalette::Standard));
    expect(ui135.ok && routeWideLineageValid(ui135),
        "ui135 route-wide package and lineage build");
    bool foundResidual = false;
    for (const auto& event : ui135.predictedDashEvents) {
        for (std::size_t contributorIndex = 1;
             contributorIndex < event.orderedContributors.size();
             ++contributorIndex) {
            const auto& previous =
                event.orderedContributors[contributorIndex - 1];
            const auto& contributor =
                event.orderedContributors[contributorIndex];
            const double length = contributor.eventRouteDistanceEndPx -
                contributor.eventRouteDistanceStartPx;
            if (previous.arcSegmentId == "predicted_arc_0" &&
                contributor.arcSegmentId == "predicted_arc_1") {
                expect(std::abs(length - 0.7779527559055168) < 1e-4 &&
                    std::abs(contributor.sourceLocalTStart) < 1e-7,
                    "ui135 exact arc0-to-arc1 residual is 0.7779527559px");
                foundResidual = true;
                expect(event.orderedRoleMemberKeys.size() == 3,
                    "cross-parent residual event has exact three roles");
                for (const auto& batch : ui135.semanticBatches)
                    expect(batch.primitiveId.find(
                        "predicted_arc_1_dash_0") == std::string::npos,
                        "0.778px residual never becomes standalone member");
                break;
            }
        }
    }
    expect(foundResidual,
        "ui135 0.778px residual is a contributor of one cross-parent event");

    const auto gapOnly = buildTrajectoryInsetDrawPackage(requestFor(0,
        TrajectoryInsetPalette::Standard, gapOnlySourceFixture()));
    expect(gapOnly.ok && routeWideLineageValid(gapOnly),
        "gap-only source fixture builds exact lineage");
    const TrajectoryInsetSourceMappingRow* gapOnlyRow = nullptr;
    for (const auto& row : gapOnly.mappingRows) {
        if (row.orderedPredictedRouteIntervals.empty()) continue;
        bool allGap = true;
        for (const auto& interval : row.orderedPredictedRouteIntervals)
            allGap = allGap && interval.dashEventKey.empty() &&
                interval.nonRenderedReason ==
                    "predicted_dash_gap_by_route_wide_cadence";
        if (allGap) gapOnlyRow = &row;
    }
    expect(gapOnlyRow != nullptr && gapOnlyRow->orderedInsetMemberKeys.empty(),
        "gap-only source remains denominator row with no token/member");
    for (const auto& probe : gapOnly.probes)
        expect(probe.parentSourceKey != gapOnlyRow->parentSourceKey,
            "gap-only source interval creates no probe");

    const auto discontinuous = buildTrajectoryInsetDrawPackage(requestFor(0,
        TrajectoryInsetPalette::Standard, discontinuousPredictedFixture()));
    expect(discontinuous.ok && routeWideLineageValid(discontinuous) &&
        discontinuous.predictedRouteSpanCount == 2,
        "visibility discontinuity creates exactly two cadence spans");
    std::map<std::string, std::size_t> parentSpan;
    for (const auto& row : discontinuous.mappingRows)
        for (const auto& interval : row.orderedPredictedRouteIntervals)
            parentSpan[row.parentSourceKey] = interval.routeSpanOrdinal;
    for (const auto& event : discontinuous.predictedDashEvents)
        for (const auto& contributor : event.orderedContributors)
            expect(parentSpan.at(contributor.parentSourceKey) ==
                    event.routeSpanOrdinal,
                "no maximal event crosses an invisible/invalid span break");
}

void testRouteWideLineageMutationsFailClosed()
{
    const auto canonical = buildTrajectoryInsetDrawPackage(requestFor(1,
        TrajectoryInsetPalette::Standard));
    expect(canonical.ok && routeWideLineageValid(canonical),
        "mutation canonical lineage passes");
    std::size_t crossing = canonical.predictedDashEvents.size();
    for (std::size_t i = 0; i < canonical.predictedDashEvents.size(); ++i)
        if (canonical.predictedDashEvents[i].orderedContributors.size() >= 2) {
            crossing = i; break;
        }
    expect(crossing < canonical.predictedDashEvents.size(),
        "mutation fixture has cross-parent event");
    auto mutated = canonical;
    std::swap(mutated.predictedDashEvents[crossing].orderedContributors[0],
        mutated.predictedDashEvents[crossing].orderedContributors[1]);
    expect(!routeWideLineageValid(mutated),
        "reordered contributors fail closed");
    mutated = canonical;
    mutated.predictedDashEvents[crossing].orderedContributors[1].
        eventRouteDistanceStartPx -= 0.25;
    expect(!routeWideLineageValid(mutated),
        "overlapping contributors fail closed");
    mutated = canonical;
    mutated.predictedDashEvents[crossing].orderedContributors[1].
        eventRouteDistanceStartPx += 0.25;
    expect(!routeWideLineageValid(mutated),
        "forward-gap contributor fails closed");
    mutated = canonical;
    mutated.predictedDashEvents[crossing].orderedContributors.pop_back();
    expect(!routeWideLineageValid(mutated),
        "missing contributor fails closed");
    mutated = canonical;
    mutated.predictedDashEvents[crossing].orderedContributors[0].
        parentSourceKey = "foreign_generation|predicted|foreign_arc";
    expect(!routeWideLineageValid(mutated),
        "foreign contributor fails closed");
    mutated = canonical;
    bool changedGap = false;
    for (auto& row : mutated.mappingRows)
        for (auto& interval : row.orderedPredictedRouteIntervals)
            if (!changedGap && !interval.nonRenderedReason.empty()) {
                interval.dashEventKey =
                    mutated.predictedDashEvents.front().eventKey;
                changedGap = true;
            }
    expect(changedGap && !routeWideLineageValid(mutated),
        "gap interval cannot gain event token/reference");

    const auto gapCanonical = buildTrajectoryInsetDrawPackage(requestFor(0,
        TrajectoryInsetPalette::Standard, gapOnlySourceFixture()));
    mutated = gapCanonical;
    std::string gapParent;
    for (const auto& row : mutated.mappingRows) {
        if (row.orderedPredictedRouteIntervals.empty()) continue;
        bool allGap = true;
        for (const auto& interval : row.orderedPredictedRouteIntervals)
            allGap = allGap && interval.dashEventKey.empty();
        if (allGap) { gapParent = row.parentSourceKey; break; }
    }
    expect(!gapParent.empty(), "gap mutation source exists");
    InsetTrajectoryProbe illegal = mutated.probes.front();
    illegal.parentSourceKey = gapParent;
    illegal.insetMemberKey =
        mutated.predictedDashEvents.front().orderedRoleMemberKeys[2];
    mutated.probes.push_back(illegal);
    expect(!routeWideLineageValid(mutated),
        "gap-only source cannot gain predicted token/probe");
    mutated = gapCanonical;
    mutated.mappingRows.erase(std::remove_if(mutated.mappingRows.begin(),
        mutated.mappingRows.end(), [&](const auto& row) {
            return row.parentSourceKey == gapParent;
        }), mutated.mappingRows.end());
    expect(!routeWideLineageValid(mutated),
        "gap-only source row deletion fails denominator closed");
}

void testEndpointMutationsFailClosed()
{
    const auto canonical = buildTrajectoryInsetDrawPackage(requestFor(0,
        TrajectoryInsetPalette::Standard, edgeCornerFixture()));
    expect(canonical.ok && endpointLockedGeometryValid(canonical),
        "endpoint mutation canonical is valid");
    const auto findRole = [&](TrajectoryInsetDrawPackage& package,
        TrajectoryInsetRole role, const std::string& fragment = {}) {
        return std::find_if(package.semanticBatches.begin(),
            package.semanticBatches.end(), [&](const auto& batch) {
                return batch.role == role && (fragment.empty() ||
                    batch.primitiveId.find(fragment) != std::string::npos);
            });
    };
    auto mutated = canonical;
    auto marker = findRole(mutated,
        TrajectoryInsetRole::PredictedEndpointDiamond);
    mutated.vertices[marker->firstVertex].insetOwnershipToken = 0;
    expect(!endpointLockedGeometryValid(mutated),
        "open/notched endpoint band fails closed");
    mutated = canonical;
    marker = findRole(mutated, TrajectoryInsetRole::PredictedEndpointDiamond);
    mutated.vertices[marker->firstVertex].xPhysicalPx += 0.25f;
    expect(!endpointLockedGeometryValid(mutated),
        "endpoint band radius drift fails closed");
    mutated = canonical;
    auto capsule = findRole(mutated, TrajectoryInsetRole::ActualEndpointCapsule);
    float capsuleMaxX = -std::numeric_limits<float>::infinity();
    for (std::size_t i = capsule->firstVertex;
         i < capsule->firstVertex + capsule->vertexCount; ++i)
        capsuleMaxX = std::max(capsuleMaxX,
            mutated.vertices[i].xPhysicalPx);
    for (std::size_t i = capsule->firstVertex;
         i < capsule->firstVertex + capsule->vertexCount; ++i)
        if (std::abs(mutated.vertices[i].xPhysicalPx - capsuleMaxX) < 1e-5f)
            mutated.vertices[i].xPhysicalPx += 0.25f;
    float mutatedCapsuleMaxX = -std::numeric_limits<float>::infinity();
    for (std::size_t i = capsule->firstVertex;
         i < capsule->firstVertex + capsule->vertexCount; ++i)
        mutatedCapsuleMaxX = std::max(mutatedCapsuleMaxX,
            mutated.vertices[i].xPhysicalPx);
    expect(std::abs(mutatedCapsuleMaxX - capsuleMaxX - 0.25f) < 1e-5f,
        "capsule dimension mutation changes the real tangent bbox extreme");
    expect(!endpointLockedGeometryValid(mutated),
        "capsule dimension drift fails closed");
    mutated = canonical;
    capsule = findRole(mutated, TrajectoryInsetRole::ActualEndpointCapsule);
    for (std::size_t i = capsule->firstVertex;
         i < capsule->firstVertex + capsule->vertexCount; ++i)
        mutated.vertices[i].yPhysicalPx += 0.25f;
    expect(!endpointLockedGeometryValid(mutated),
        "capsule offset/asymmetry drift fails closed");
    mutated = canonical;
    marker = findRole(mutated, TrajectoryInsetRole::PredictedEndpointDiamond);
    auto halo = findRole(mutated, TrajectoryInsetRole::PredictedInnerHalo,
        "endpoint_inner_halo");
    std::swap(marker->firstVertex, halo->firstVertex);
    expect(!endpointLockedGeometryValid(mutated),
        "endpoint draw-order drift fails closed");
    mutated = canonical;
    mutated.predictedDashEvents.back().routeDistanceEndPx -= 0.5;
    expect(!routeWideLineageValid(mutated),
        "tail-event truncation fails closed");
    mutated = canonical;
    mutated.predictedDashEvents.back().routeDistanceStartPx += 0.25;
    expect(!routeWideLineageValid(mutated),
        "tail-event rephase fails closed");

    const auto endpointProbeContract = [&](const TrajectoryInsetDrawPackage& p) {
        std::vector<std::string> required;
        required.push_back(p.predictedDashEvents.back().orderedRoleMemberKeys[2]);
        for (const auto& batch : p.semanticBatches)
            if (batch.role == TrajectoryInsetRole::PredictedEndpointDiamond ||
                batch.role == TrajectoryInsetRole::ActualEndpointCapsule)
                required.push_back(batch.insetMemberKey);
        for (const auto& member : required)
            if (std::count_if(p.probes.begin(), p.probes.end(),
                    [&](const auto& probe) {
                        return probe.insetMemberKey == member;
                    }) != 1) return false;
        return true;
    };
    expect(endpointProbeContract(canonical),
        "canonical endpoint probe contract is exact");
    mutated = canonical;
    const std::string markerMember = findRole(mutated,
        TrajectoryInsetRole::PredictedEndpointDiamond)->insetMemberKey;
    mutated.probes.erase(std::remove_if(mutated.probes.begin(),
        mutated.probes.end(), [&](const auto& probe) {
            return probe.insetMemberKey == markerMember;
        }), mutated.probes.end());
    expect(!endpointProbeContract(mutated),
        "removed marker probe fails closed");
    mutated = canonical;
    const std::string capsuleMember = findRole(mutated,
        TrajectoryInsetRole::ActualEndpointCapsule)->insetMemberKey;
    const auto capsuleProbe = std::find_if(mutated.probes.begin(),
        mutated.probes.end(), [&](const auto& probe) {
            return probe.insetMemberKey == capsuleMember;
        });
    mutated.probes.push_back(*capsuleProbe);
    expect(!endpointProbeContract(mutated),
        "duplicated capsule probe fails closed");

    mutated = canonical;
    marker = findRole(mutated, TrajectoryInsetRole::PredictedEndpointDiamond);
    const auto markerBounds = [&]() {
        std::array<float, 4> value{{
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity()}};
        for (std::size_t i = marker->firstVertex;
             i < marker->firstVertex + marker->vertexCount; ++i) {
            value[0] = std::min(value[0], mutated.vertices[i].xPhysicalPx);
            value[1] = std::min(value[1], mutated.vertices[i].yPhysicalPx);
            value[2] = std::max(value[2], mutated.vertices[i].xPhysicalPx);
            value[3] = std::max(value[3], mutated.vertices[i].yPhysicalPx);
        }
        return value;
    }();
    const float centerX = (markerBounds[0] + markerBounds[2]) * 0.5f;
    const float centerY = (markerBounds[1] + markerBounds[3]) * 0.5f;
    for (std::size_t i = marker->firstVertex;
         i < marker->firstVertex + marker->vertexCount; ++i) {
        const float l1 = std::abs(mutated.vertices[i].xPhysicalPx - centerX) +
            std::abs(mutated.vertices[i].yPhysicalPx - centerY);
        if (std::abs(l1 - 2.5f) < 1e-4f) {
            mutated.vertices[i].xPhysicalPx = centerX;
            mutated.vertices[i].yPhysicalPx = centerY;
        }
    }
    const auto consumedRaster = rasterTrajectoryInsetSemantics(mutated,
        trajectoryInsetSampleProfile1x());
    const auto consumedStats = tokenAreaAndLargest8(consumedRaster,
        mutated.predictedDashEvents.back().orderedRoleTokens[2], 0);
    expect(consumedRaster.ok && consumedStats.first == 0,
        "mutation proves a filled endpoint that consumes the tail is rejected by visibility contract");
    expect(!endpointLockedGeometryValid(mutated),
        "tail-consuming filled endpoint fails locked hollow geometry");
}

void testExactDigestMutationsFailClosed()
{
    const auto canonical = buildTrajectoryInsetDrawPackage(requestFor(1,
        TrajectoryInsetPalette::Standard));
    expect(canonical.ok && exactDigestContractValid(canonical),
        "canonical five-digest contract passes independent recomputation");
    auto mutated = canonical;
    mutated.orderedParentSourceKeys.front() += "_mutated";
    expect(recomputeInsetDigests(mutated).parents !=
            canonical.orderedParentSourceDigest &&
        !exactDigestContractValid(mutated),
        "parent-key field mutation changes and invalidates parent digest");
    mutated = canonical;
    ++mutated.probes.front().strokeXPhysicalPx;
    expect(recomputeInsetDigests(mutated).probes !=
            canonical.orderedProbeDigest &&
        !exactDigestContractValid(mutated),
        "probe coordinate mutation changes and invalidates complete probe digest");
    mutated = canonical;
    ++mutated.predictedDashEvents.front().orderedRoleTokens[0];
    expect(recomputeInsetDigests(mutated).events !=
            canonical.orderedDashEventDigest &&
        !exactDigestContractValid(mutated),
        "event role-token mutation changes and invalidates event digest");
    mutated = canonical;
    mutated.predictedDashEvents.front().orderedContributors.front().
        sourceLocalTEnd = std::nextafter(
            mutated.predictedDashEvents.front().orderedContributors.front().
                sourceLocalTEnd,
            std::numeric_limits<double>::infinity());
    expect(recomputeInsetDigests(mutated).contributors !=
            canonical.orderedDashContributorDigest &&
        !exactDigestContractValid(mutated),
        "one-ULP contributor mutation changes exact IEEE contributor digest");
    mutated = canonical;
    auto& interval = mutated.mappingRows.back().
        orderedPredictedRouteIntervals.front();
    interval.routeDistanceEndPx = std::nextafter(interval.routeDistanceEndPx,
        std::numeric_limits<double>::infinity());
    expect(recomputeInsetDigests(mutated).sourceIntervals !=
            canonical.orderedSourceRouteIntervalDigest &&
        !exactDigestContractValid(mutated),
        "one-ULP source interval mutation changes exact partition digest");
}

void testFailClosedInputs()
{
    auto request = requestFor(0, TrajectoryInsetPalette::Standard);
    request.generationId.clear();
    expect(!buildTrajectoryInsetDrawPackage(request).ok,
        "missing generation fails closed");
    request = requestFor(0, TrajectoryInsetPalette::Standard);
    request.layout.plotRect.width = 392.0f;
    expect(!buildTrajectoryInsetDrawPackage(request).ok,
        "stretched/non-2:1 plot fails closed");
    request = requestFor(0, TrajectoryInsetPalette::Standard);
    request.layout.uiScale = 1.36f;
    expect(!buildTrajectoryInsetDrawPackage(request).ok,
        "unsupported UI scale fails closed");
    TrajectoryInsetDrawPackage invalid;
    const auto raster = rasterTrajectoryInsetSemantics(invalid,
        trajectoryInsetSampleProfile1x());
    expect(!raster.ok, "invalid package never rasterizes");
    auto badSamples = trajectoryInsetSampleProfile2x();
    badSamples.positions[1][1] = 1.0f;
    const auto valid = buildTrajectoryInsetDrawPackage(
        requestFor(0, TrajectoryInsetPalette::Standard));
    expect(!rasterTrajectoryInsetSemantics(valid, badSamples).ok,
        "invalid asymmetric sample fails closed");
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        testExact96Cases();
        testEdgeCornerAndWorldMapping();
        testRouteWideResidualGapAndSpanContracts();
        testRouteWideLineageMutationsFailClosed();
        testEndpointMutationsFailClosed();
        testExactDigestMutationsFailClosed();
        testFailClosedInputs();
        if (argc == 2 && std::string(argv[1]) == "--emit-inventory-json") {
            std::cout << "{\"schema\":\"billiardgl.phase4.v19.cp1_layout_inventory.v1\",\"layouts\":[";
            for (std::size_t index = 0; index < 4; ++index) {
                const auto package = buildTrajectoryInsetDrawPackage(requestFor(
                    index, TrajectoryInsetPalette::Standard));
                expect(package.ok && exactDigestContractValid(package),
                    "inventory package passes exact digest contract");
                if (index != 0) std::cout << ",";
                std::cout << "{\"profileId\":\"" << package.layout.profileId
                    << "\",\"sourceGroupDenominator\":"
                    << package.sourceGroupDenominator
                    << ",\"semanticMemberDenominator\":"
                    << package.semanticMemberDenominator
                    << ",\"probeDenominator\":" << package.probeDenominator
                    << ",\"dashEventCount\":"
                    << package.predictedDashEvents.size()
                    << ",\"routeSpanCount\":"
                    << package.predictedRouteSpanCount
                    << ",\"orderedParentSourceDigest\":\""
                    << package.orderedParentSourceDigest
                    << "\",\"orderedProbeDigest\":\""
                    << package.orderedProbeDigest
                    << "\",\"orderedDashEventDigest\":\""
                    << package.orderedDashEventDigest
                    << "\",\"orderedDashContributorDigest\":\""
                    << package.orderedDashContributorDigest
                    << "\",\"orderedSourceRouteIntervalDigest\":\""
                    << package.orderedSourceRouteIntervalDigest << "\"}";
            }
            std::cout << "]}\n";
            return 0;
        }
        std::cout << "phase4_v19_trajectory_inset_cpu_tests: PASS cases=96 "
                     "camera=4 palette=2 samples=1/2/4 layouts=4\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "phase4_v19_trajectory_inset_cpu_tests: FAIL "
                  << error.what() << "\n";
        return 1;
    }
}
