#include "frozen_cue_topology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>

namespace billiardgl {
namespace {

struct BallEdge {
    int first = -1;
    int second = -1;
    ContinuousContactCandidate contact;
};

FrozenCueTopology contradictory()
{
    FrozenCueTopology result;
    result.status = FrozenCueTopologyStatus::ContradictoryTopology;
    result.error = "contradictory_frozen_topology";
    return result;
}

Point3 normalBetween(const BallState& first, const BallState& second)
{
    const double x = second.position.x - first.position.x;
    const double y = second.position.y - first.position.y;
    const double z = second.position.z - first.position.z;
    const double length = std::sqrt(x * x + y * y + z * z);
    if (!(length > 1e-12) || !std::isfinite(length)) {
        return Point3{1.0f, 0.0f, 0.0f};
    }
    return Point3{static_cast<float>(x / length),
        static_cast<float>(y / length), static_cast<float>(z / length)};
}

std::vector<BallEdge> touchingBallEdges(const GameState& state,
    const PhysicsProfile& profile, bool& hasDeepOverlap)
{
    std::vector<BallEdge> edges;
    const double diameter = 2.0 * profile.ball.radiusCm;
    const double tolerance = std::max(
        0.000001, static_cast<double>(profile.solver.penetrationSlopCm));
    for (int first = 0; first < kBallCount; ++first) {
        if (state.balls[first].pocketed) continue;
        for (int second = first + 1; second < kBallCount; ++second) {
            if (state.balls[second].pocketed) continue;
            const double x = state.balls[second].position.x -
                state.balls[first].position.x;
            const double y = state.balls[second].position.y -
                state.balls[first].position.y;
            const double z = state.balls[second].position.z -
                state.balls[first].position.z;
            const double distance = std::sqrt(x * x + y * y + z * z);
            if (!std::isfinite(distance)) {
                hasDeepOverlap = true;
                continue;
            }
            const double penetration = std::max(0.0, diameter - distance);
            if (penetration > profile.solver.maximumPenetrationCm) {
                hasDeepOverlap = true;
            }
            if (distance > diameter + tolerance) continue;
            ContinuousContactCandidate contact;
            contact.valid = true;
            contact.kind = ContinuousContactKind::BallBall;
            contact.firstBall = first;
            contact.secondBall = second;
            contact.timeOfImpactSeconds = 0.0;
            contact.penetrationCm = penetration;
            contact.normal = normalBetween(
                state.balls[first], state.balls[second]);
            edges.push_back(BallEdge{first, second, contact});
        }
    }
    return edges;
}

void appendRailContacts(FrozenCueTopology& topology, const GameState& state,
    const PhysicsProfile& profile, const std::array<bool, kBallCount>& included)
{
    const double tolerance = std::max(
        0.000001, static_cast<double>(profile.solver.penetrationSlopCm));
    const double xLimit = profile.tableBoundary.playfieldWidthCm * 0.5 -
        profile.ball.radiusCm;
    const double zLimit = profile.tableBoundary.playfieldLengthCm * 0.5 -
        profile.ball.radiusCm;
    for (int index = 0; index < kBallCount; ++index) {
        if (!included[index]) continue;
        const BallState& ball = state.balls[index];
        const struct Rail {
            double coordinate;
            double limit;
            int negativeFeature;
            int positiveFeature;
            bool xAxis;
        } rails[] = {
            {ball.position.x, xLimit, 0, 1, true},
            {ball.position.z, zLimit, 2, 3, false},
        };
        for (const Rail& rail : rails) {
            if (std::fabs(std::fabs(rail.coordinate) - rail.limit) > tolerance) {
                continue;
            }
            const bool positive = rail.coordinate >= 0.0;
            Point3 normal;
            if (rail.xAxis) normal.x = positive ? -1.0f : 1.0f;
            else normal.z = positive ? -1.0f : 1.0f;
            ContinuousContactCandidate contact = boundaryContactCandidate(
                index, positive ? rail.positiveFeature : rail.negativeFeature,
                0.0, normal, PocketBoundaryEventKind::StraightRail);
            contact.penetrationCm = std::max(
                0.0, std::fabs(rail.coordinate) - rail.limit);
            topology.island.contacts.push_back(contact);
        }
    }
}

}  // namespace

FrozenCueTopology detectFrozenCueTopology(
    const GameState& state, int cueBallIndex, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode)
{
    if (cueBallIndex < 0 || cueBallIndex >= kBallCount ||
        state.balls[cueBallIndex].pocketed) {
        return contradictory();
    }

    bool hasDeepOverlap = false;
    const std::vector<BallEdge> edges =
        touchingBallEdges(state, profile, hasDeepOverlap);
    std::array<bool, kBallCount> included{};
    included[cueBallIndex] = true;
    std::queue<int> pending;
    pending.push(cueBallIndex);
    while (!pending.empty()) {
        const int current = pending.front();
        pending.pop();
        for (const BallEdge& edge : edges) {
            int neighbour = -1;
            if (edge.first == current) neighbour = edge.second;
            else if (edge.second == current) neighbour = edge.first;
            if (neighbour >= 0 && !included[neighbour]) {
                included[neighbour] = true;
                pending.push(neighbour);
            }
        }
    }

    FrozenCueTopology result;
    result.island.islandId = 0;
    result.island.timeOfImpactSeconds = 0.0;
    for (int index = 0; index < kBallCount; ++index) {
        if (included[index]) result.island.ballIndices.push_back(index);
    }
    if (hasDeepOverlap) {
        for (const BallEdge& edge : edges) {
            if (included[edge.first] && included[edge.second] &&
                edge.contact.penetrationCm >
                    profile.solver.maximumPenetrationCm) {
                return contradictory();
            }
        }
    }
    if (static_cast<int>(result.island.ballIndices.size()) >
        profile.solver.maximumIslandSize) {
        result.status = FrozenCueTopologyStatus::IslandLimit;
        result.error = "contact_island_limit";
        result.island.limitExceeded = true;
        return result;
    }
    for (const BallEdge& edge : edges) {
        if (included[edge.first] && included[edge.second]) {
            result.island.contacts.push_back(edge.contact);
        }
    }
    if (boundaryMode == PhysicsBoundaryMode::ProductionTable) {
        appendRailContacts(result, state, profile, included);
    }
    std::sort(result.island.contacts.begin(), result.island.contacts.end(),
        continuousContactLess);
    result.frozen = !result.island.contacts.empty();
    return result;
}

}  // namespace billiardgl
