#pragma once

#include "game_state.h"
#include "physics_profile.h"
#include "rules/chinese_eight_ball_rules.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl { namespace rules {

inline constexpr std::uint64_t kOfficialRackDefaultSeed =
    0x48455942414c4c25ULL;
inline constexpr const char* kOfficialRackAlgorithmId =
    "wpa_heyball_2025_08_16_group_separation_seeded_v1";
inline constexpr const char* kFoundationRerackActionId =
    "rules.foundation.rerack";
inline constexpr const char* kFoundationRespotActionId =
    "rules.foundation.respot";

enum class RackFoundationOperation {
    Rerack,
    Respot,
};

struct RackSlot {
    int slotIndex = -1;
    int row = -1;
    int column = -1;
    int ball = -1;
    Point3 position;
};

struct OfficialRackContract {
    std::string rulesetId = "wpa_heyball_2025_08_16_v1";
    std::string sourceClause = "WPA-II-04";
    std::string algorithmId = kOfficialRackAlgorithmId;
    std::uint64_t deterministicSeed = kOfficialRackDefaultSeed;
    Point3 footSpot;
    Point3 cueBallBreakPosition;
    int apexSlot = 0;
    int eightBallSlot = 4;
    std::array<int, 2> baseCornerSlots{{10, 14}};
    int sameGroupAdjacencyCount = 0;
    int minimumPossibleSameGroupAdjacencyCount = 0;
    // WPA II §4 fixes the geometry/group constraints above, but does not fix
    // the complete numbered-ball order. The seed and algorithm ID define only
    // this implementation's deterministic completion of that partial order.
    bool numberedBallOrderIsOfficial = false;
    std::array<RackSlot, 15> slots;
};

struct RerackFoundationParameters {
    std::uint64_t deterministicSeed = kOfficialRackDefaultSeed;
    int breakerPlayerId = 0;
};

struct RespotSearchPolicy {
    // WPA II §7: nominal/original position, then the longitudinal axis toward
    // the top cushion; if that whole segment is obstructed, continue on the
    // longitudinal axis at increasing distance from the foot spot.
    double stepCm = 0.25;
    int maximumCandidates = 1024;
    double clearanceCm = 0.001;
};

struct RespotFoundationParameters {
    int ball = 8;
    Point3 nominalPosition;
    RespotSearchPolicy search;
};

struct RackFoundationPlan {
    bool ok = false;
    std::string errorCode;
    RackFoundationOperation operation = RackFoundationOperation::Rerack;
    std::string transactionId;
    std::uint64_t expectedAuthorityStateRevision = 0;
    std::string baseAuthorityStateHash;
    RerackFoundationParameters rerack;
    RespotFoundationParameters respot;
    OfficialRackContract rackContract;
    std::vector<std::pair<int, Point3>> placements;
    int candidatesExamined = 0;
};

struct RackFoundationEffects {
    RackFoundationOperation operation = RackFoundationOperation::Rerack;
    std::vector<std::pair<int, Point3>> placements;
    int breakerPlayerId = -1;
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool ballInHand = false;
    std::array<BallGroup, 2> groups{{BallGroup::Open, BallGroup::Open}};
    MatchPhase phase = MatchPhase::Break;
    int winner = -1;
};

struct RackFoundationAck {
    bool accepted = false;
    std::string transactionId;
    std::string actionId;
    std::string idempotencyKey;
    std::uint64_t previousStateRevision = 0;
    std::uint64_t authoritativeStateRevision = 0;
    RackFoundationEffects effects;
    std::string authoritativeStateHash;
};

struct RackFoundationResult {
    bool ok = false;
    std::string errorCode;
    RackFoundationAck ack;
};

OfficialRackContract buildOfficialRackContract(
    const PhysicsProfile& profile,
    std::uint64_t deterministicSeed = kOfficialRackDefaultSeed);

RackFoundationPlan buildRerackFoundationPlan(
    const GameState& state, const PhysicsProfile& profile,
    const RerackFoundationParameters& parameters,
    std::uint64_t authorityRevision,
    const std::string& baseAuthorityStateHash);

RackFoundationPlan buildRespotFoundationPlan(
    const GameState& state, const PhysicsProfile& profile,
    const RespotFoundationParameters& parameters,
    std::uint64_t authorityRevision,
    const std::string& baseAuthorityStateHash);

} }  // namespace billiardgl::rules
