#pragma once

#include "game_state.h"
#include "rules/ball_in_hand_authority.h"
#include "rules/chinese_eight_ball_rules.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::rules {

inline constexpr const char* kEightOnBreakCapabilityId =
    "eight_on_break_spot_or_rerack";
inline constexpr const char* kEightOnBreakFlowId =
    "eight_on_break_spot_or_rerack";
inline constexpr const char* kEightOnBreakRulesetId =
    "wpa_heyball_2025_08_16_v1";
inline constexpr const char* kEightOnBreakSpotContinueActionId =
    "rules.eight_on_break.spot_continue";
inline constexpr const char* kEightOnBreakSpotBallInHandActionId =
    "rules.eight_on_break.spot_ball_in_hand_head_string";
inline constexpr const char* kEightOnBreakRerackOriginalActionId =
    "rules.eight_on_break.rerack_original_breaker";
inline constexpr const char* kEightOnBreakRerackOpponentActionId =
    "rules.eight_on_break.rerack_opponent_breaks";

enum class EightOnBreakBranch {
    LegalBreak,
    FoulBreak,
};

struct EightOnBreakDecisionOffer {
    std::string decisionId;
    std::uint64_t authorityStateRevision = 0;
    int actorPlayerId = 0;
    int originalBreakerPlayerId = 0;
    int frozenNextPlayer = 0;
    bool frozenContinueTurn = false;
    EightOnBreakBranch branch = EightOnBreakBranch::LegalBreak;
    std::vector<std::string> offeredActionIds;
};

struct EightOnBreakDecisionRequest {
    std::string rulesetId;
    std::string capabilityId;
    std::string flowId;
    std::string decisionId;
    std::uint64_t expectedAuthorityStateRevision = 0;
    std::string idempotencyKey;
    int actorPlayerId = 0;
    std::string actionId;
    std::string payloadCanonical;
};

struct EightOnBreakDecisionEffects {
    EightOnBreakBranch branch = EightOnBreakBranch::LegalBreak;
    bool eightBallRespotted = false;
    bool rackRebuilt = false;
    int originalBreakerPlayerId = 0;
    int breakerPlayerId = -1;
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool turnContinues = false;
    bool ballInHand = false;
    BallInHandPlacementScope ballInHandScope =
        BallInHandPlacementScope::None;
    bool decisionPending = false;
    std::array<BallGroup, 2> groups{{BallGroup::Open, BallGroup::Open}};
    MatchPhase phase = MatchPhase::Break;
    int winner = -1;
    Point3 eightBallPosition;
};

struct EightOnBreakDecisionAck {
    bool accepted = false;
    std::string decisionId;
    std::string actionId;
    std::string idempotencyKey;
    int actorPlayerId = 0;
    std::uint64_t previousStateRevision = 0;
    std::uint64_t authoritativeStateRevision = 0;
    EightOnBreakDecisionEffects effects;
    std::string authoritativeStateHash;
};

struct EightOnBreakDecisionResult {
    bool ok = false;
    std::string errorCode;
    EightOnBreakDecisionAck ack;
};

}  // namespace billiardgl::rules
