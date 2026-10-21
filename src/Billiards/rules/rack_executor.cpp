#include "rules/rack_executor.h"

#include "rules/chinese_eight_ball_rules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace billiardgl { namespace rules {
namespace {

struct SlotGeometry {
    int row = 0;
    int column = 0;
    Point3 position;
};

std::array<SlotGeometry, 15> rackGeometry(const PhysicsProfile& profile)
{
    std::array<SlotGeometry, 15> slots;
    const double radius = profile.ball.radiusCm;
    const double footZ = profile.tableBoundary.playfieldLengthCm * 0.25;
    const double rowAdvance = std::sqrt(3.0) * radius;
    int index = 0;
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column <= row; ++column) {
            SlotGeometry& slot = slots[index++];
            slot.row = row;
            slot.column = column;
            slot.position = Point3{
                static_cast<float>((2 * column - row) * radius),
                kTableHeight + profile.ball.radiusCm,
                static_cast<float>(footZ + row * rowAdvance)};
        }
    }
    return slots;
}

bool adjacent(const SlotGeometry& first, const SlotGeometry& second,
              double diameter)
{
    const double dx = first.position.x - second.position.x;
    const double dz = first.position.z - second.position.z;
    return dx * dx + dz * dz <= diameter * diameter + 0.0001;
}

std::uint64_t mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

int populationCount(unsigned value)
{
    int count = 0;
    while (value != 0) {
        value &= value - 1;
        ++count;
    }
    return count;
}

template <std::size_t Size>
void seededShuffle(std::array<int, Size>& values, std::uint64_t seed)
{
    std::uint64_t state = seed;
    for (std::size_t count = Size; count > 1; --count) {
        state = mix(state);
        const std::size_t other = static_cast<std::size_t>(state % count);
        std::swap(values[count - 1], values[other]);
    }
}

std::vector<int> groupSlots()
{
    std::vector<int> result;
    for (int slot = 0; slot < 15; ++slot)
        if (slot != 4) result.push_back(slot);
    return result;
}

int sameGroupAdjacencies(const std::array<SlotGeometry, 15>& geometry,
                         const std::array<int, 15>& group, double diameter)
{
    int result = 0;
    for (int first = 0; first < 15; ++first) {
        if (group[first] < 0) continue;
        for (int second = first + 1; second < 15; ++second) {
            if (group[second] == group[first] &&
                adjacent(geometry[first], geometry[second], diameter))
                ++result;
        }
    }
    return result;
}

std::string transactionId(const char* operation, std::uint64_t revision,
                          const std::string& baseHash,
                          const std::string& parameters)
{
    std::uint64_t hash = 1469598103934665603ULL;
    const std::string canonical = std::string(operation) + '|' +
        std::to_string(revision) + '|' + baseHash + '|' + parameters;
    for (unsigned char byte : canonical) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream value;
    value << "rules.foundation." << operation << '.' << revision << '.'
          << std::hex << std::setfill('0') << std::setw(16) << hash;
    return value.str();
}

bool finitePoint(const Point3& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::isfinite(point.z);
}

bool legalRespotCandidate(const GameState& state,
                          const PhysicsProfile& profile, int target,
                          const Point3& candidate, double clearance)
{
    const double radius = profile.ball.radiusCm;
    const double xLimit =
        profile.tableBoundary.playfieldWidthCm * 0.5 - radius;
    const double zLimit =
        profile.tableBoundary.playfieldLengthCm * 0.5 - radius;
    if (!finitePoint(candidate) || std::fabs(candidate.x) > xLimit ||
        std::fabs(candidate.z) > zLimit)
        return false;
    const double minimum = 2.0 * radius + clearance;
    const double minimumSquared = minimum * minimum;
    for (int index = 0; index < kBallCount; ++index) {
        if (index == target || state.balls[index].pocketed) continue;
        const BallState& other = state.balls[index];
        if (!finitePoint(other.position)) return false;
        const double dx = candidate.x - other.position.x;
        const double dz = candidate.z - other.position.z;
        if (dx * dx + dz * dz < minimumSquared) return false;
    }
    return true;
}

}  // namespace

OfficialRackContract buildOfficialRackContract(
    const PhysicsProfile& profile, std::uint64_t deterministicSeed)
{
    OfficialRackContract contract;
    contract.deterministicSeed = deterministicSeed;
    contract.footSpot = Point3{0.0f,
        kTableHeight + profile.ball.radiusCm,
        profile.tableBoundary.playfieldLengthCm * 0.25f};
    contract.cueBallBreakPosition = Point3{0.0f,
        kTableHeight + profile.ball.radiusCm,
        -profile.tableBoundary.playfieldLengthCm * 0.25f};
    const std::array<SlotGeometry, 15> geometry = rackGeometry(profile);
    const std::vector<int> candidates = groupSlots();

    int bestScore = std::numeric_limits<int>::max();
    std::uint64_t bestTie = std::numeric_limits<std::uint64_t>::max();
    std::array<int, 15> bestGroups;
    bestGroups.fill(-1);
    const unsigned limit = 1u << candidates.size();
    for (unsigned mask = 0; mask < limit; ++mask) {
        if (populationCount(mask) != 7) continue;
        std::array<int, 15> groups;
        groups.fill(-1);
        for (std::size_t index = 0; index < candidates.size(); ++index)
            groups[candidates[index]] = (mask & (1u << index)) ? 0 : 1;
        if (groups[10] == groups[14]) continue;
        const int score = sameGroupAdjacencies(
            geometry, groups, 2.0 * profile.ball.radiusCm);
        const std::uint64_t tie = mix(deterministicSeed ^ mask);
        if (score < bestScore || (score == bestScore && tie < bestTie)) {
            bestScore = score;
            bestTie = tie;
            bestGroups = groups;
        }
    }

    std::array<int, 7> solids{{1, 2, 3, 4, 5, 6, 7}};
    std::array<int, 7> stripes{{9, 10, 11, 12, 13, 14, 15}};
    seededShuffle(solids, deterministicSeed ^ 0x534f4c494453ULL);
    seededShuffle(stripes, deterministicSeed ^ 0x53545249504553ULL);
    std::size_t solid = 0;
    std::size_t stripe = 0;
    for (int slot = 0; slot < 15; ++slot) {
        RackSlot& output = contract.slots[slot];
        output.slotIndex = slot;
        output.row = geometry[slot].row;
        output.column = geometry[slot].column;
        output.position = geometry[slot].position;
        if (slot == contract.eightBallSlot) output.ball = 8;
        else if (bestGroups[slot] == 0) output.ball = solids[solid++];
        else output.ball = stripes[stripe++];
    }
    contract.sameGroupAdjacencyCount = bestScore;
    contract.minimumPossibleSameGroupAdjacencyCount = bestScore;
    return contract;
}

RackFoundationPlan buildRerackFoundationPlan(
    const GameState&, const PhysicsProfile& profile,
    const RerackFoundationParameters& parameters,
    std::uint64_t authorityRevision,
    const std::string& baseAuthorityStateHash)
{
    RackFoundationPlan plan;
    plan.operation = RackFoundationOperation::Rerack;
    plan.expectedAuthorityStateRevision = authorityRevision;
    plan.baseAuthorityStateHash = baseAuthorityStateHash;
    plan.rerack = parameters;
    if (parameters.breakerPlayerId < 0 || parameters.breakerPlayerId > 1) {
        plan.errorCode = "rules_rerack_breaker_invalid";
        return plan;
    }
    plan.rackContract = buildOfficialRackContract(
        profile, parameters.deterministicSeed);
    plan.placements.emplace_back(0,
        plan.rackContract.cueBallBreakPosition);
    for (const RackSlot& slot : plan.rackContract.slots)
        plan.placements.emplace_back(slot.ball, slot.position);
    std::sort(plan.placements.begin(), plan.placements.end(),
        [](const auto& first, const auto& second) {
            return first.first < second.first;
        });

    const double radius = profile.ball.radiusCm;
    const double xLimit =
        profile.tableBoundary.playfieldWidthCm * 0.5 - radius;
    const double zLimit =
        profile.tableBoundary.playfieldLengthCm * 0.5 - radius;
    for (const auto& placement : plan.placements) {
        if (!finitePoint(placement.second) ||
            std::fabs(placement.second.x) > xLimit ||
            std::fabs(placement.second.z) > zLimit) {
            plan.errorCode = "rules_rerack_geometry_invalid";
            plan.placements.clear();
            return plan;
        }
    }
    const double diameter = radius * 2.0;
    for (std::size_t first = 0; first < plan.placements.size(); ++first) {
        for (std::size_t second = first + 1;
             second < plan.placements.size(); ++second) {
            const double dx = plan.placements[first].second.x -
                plan.placements[second].second.x;
            const double dz = plan.placements[first].second.z -
                plan.placements[second].second.z;
            if (dx * dx + dz * dz <
                diameter * diameter - 0.0001) {
                plan.errorCode = "rules_rerack_geometry_invalid";
                plan.placements.clear();
                return plan;
            }
        }
    }
    std::ostringstream parametersText;
    parametersText << parameters.deterministicSeed << '|'
                   << parameters.breakerPlayerId;
    plan.transactionId = transactionId("rerack", authorityRevision,
        baseAuthorityStateHash, parametersText.str());
    plan.ok = true;
    return plan;
}

RackFoundationPlan buildRespotFoundationPlan(
    const GameState& state, const PhysicsProfile& profile,
    const RespotFoundationParameters& parameters,
    std::uint64_t authorityRevision,
    const std::string& baseAuthorityStateHash)
{
    RackFoundationPlan plan;
    plan.operation = RackFoundationOperation::Respot;
    plan.expectedAuthorityStateRevision = authorityRevision;
    plan.baseAuthorityStateHash = baseAuthorityStateHash;
    plan.respot = parameters;
    if (parameters.ball <= 0 || parameters.ball >= kBallCount) {
        plan.errorCode = "rules_respot_ball_invalid";
        return plan;
    }
    if (!finitePoint(parameters.nominalPosition) ||
        !std::isfinite(parameters.search.stepCm) ||
        parameters.search.stepCm <= 0.0 ||
        parameters.search.stepCm > profile.ball.radiusCm ||
        parameters.search.maximumCandidates <= 0 ||
        parameters.search.maximumCandidates > 4096 ||
        !std::isfinite(parameters.search.clearanceCm) ||
        parameters.search.clearanceCm < 0.0 ||
        parameters.search.clearanceCm > 0.1) {
        plan.errorCode = "rules_respot_policy_invalid";
        return plan;
    }

    auto consider = [&](const Point3& candidate) {
        if (plan.candidatesExamined >=
            parameters.search.maximumCandidates)
            return false;
        ++plan.candidatesExamined;
        if (!legalRespotCandidate(state, profile, parameters.ball,
                candidate, parameters.search.clearanceCm))
            return false;
        plan.placements.emplace_back(parameters.ball, candidate);
        return true;
    };
    if (!consider(parameters.nominalPosition)) {
        const double zLimit =
            profile.tableBoundary.playfieldLengthCm * 0.5 -
            profile.ball.radiusCm;
        const double footZ =
            profile.tableBoundary.playfieldLengthCm * 0.25;
        const float y = kTableHeight + profile.ball.radiusCm;
        for (int index = 1;
             plan.placements.empty() &&
             plan.candidatesExamined < parameters.search.maximumCandidates;
             ++index) {
            const double z = footZ + index * parameters.search.stepCm;
            if (z > zLimit + 1e-9) break;
            consider(Point3{0.0f, y, static_cast<float>(z)});
        }
        for (int index = 1;
             plan.placements.empty() &&
             plan.candidatesExamined < parameters.search.maximumCandidates;
             ++index) {
            const double z = footZ - index * parameters.search.stepCm;
            if (z < -zLimit - 1e-9) break;
            consider(Point3{0.0f, y, static_cast<float>(z)});
        }
    }
    if (plan.placements.empty()) {
        plan.errorCode = "rules_respot_no_legal_position";
        return plan;
    }
    std::ostringstream parametersText;
    parametersText.imbue(std::locale::classic());
    parametersText << std::setprecision(
        std::numeric_limits<double>::max_digits10)
        << parameters.ball << '|' << parameters.nominalPosition.x << '|'
        << parameters.nominalPosition.y << '|'
        << parameters.nominalPosition.z << '|'
        << parameters.search.stepCm << '|'
        << parameters.search.maximumCandidates << '|'
        << parameters.search.clearanceCm;
    plan.transactionId = transactionId("respot", authorityRevision,
        baseAuthorityStateHash, parametersText.str());
    plan.ok = true;
    return plan;
}

} }  // namespace billiardgl::rules
