#pragma once

#include "rules/ball_in_hand_authority.h"
#include "rules/chinese_eight_ball_rules.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::rules {

inline constexpr const char* kBreakFoulRulesetId =
    "wpa_heyball_2025_08_16_v1";
inline constexpr const char* kBreakFoulCapabilityId =
    "break_foul_opponent_choices";
inline constexpr const char* kBreakFoulFlowId =
    "break_foul_opponent_choice";
inline constexpr const char* kBreakFoulAcceptTableActionId =
    "rules.break_foul.accept_table";
inline constexpr const char* kBreakFoulBallInHandActionId =
    "rules.break_foul.ball_in_hand_behind_head_string";
inline constexpr const char* kBreakFoulRerackOpponentActionId =
    "rules.break_foul.rerack_opponent_breaks";
inline constexpr const char* kBreakFoulRerackOriginalActionId =
    "rules.break_foul.rerack_original_breaker";
inline constexpr const char* kIllegalBreakSourceClause = "WPA-II-06-C";
inline constexpr const char* kBreakShotFoulSourceClause = "WPA-II-06-E";
inline constexpr const char* kBreakNoRackContactSourceClause = "WPA-II-06-G";

enum class BreakFoulBranch {
    IllegalBreak,
    BreakShotFoul,
    NoRackContactMiscue,
};

struct BreakFoulDecisionOffer {
    std::string decisionId;
    std::uint64_t authorityStateRevision = 0;
    int actorPlayerId = -1;
    int originalBreakerPlayerId = -1;
    int frozenNextPlayer = -1;
    BreakFoulBranch branch = BreakFoulBranch::IllegalBreak;
    std::string sourceClause;
    int sourcePdfPage = 0;
    std::vector<std::string> offeredActionIds;
};

struct BreakFoulDecisionRequest {
    std::string rulesetId;
    std::string capabilityId;
    std::string flowId;
    std::string decisionId;
    std::uint64_t expectedAuthorityStateRevision = 0;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::string actionId;
    std::string payloadCanonical;
};

struct BreakFoulDecisionEffects {
    BreakFoulBranch branch = BreakFoulBranch::IllegalBreak;
    bool tableAccepted = false;
    bool rackRebuilt = false;
    int originalBreakerPlayerId = -1;
    int breakerPlayerId = -1;
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool ballInHand = false;
    BallInHandPlacementScope ballInHandScope =
        BallInHandPlacementScope::None;
    bool childDecisionPending = false;
    std::array<BallGroup, 2> groups{{BallGroup::Open, BallGroup::Open}};
    MatchPhase phase = MatchPhase::Break;
    int winner = -1;
};

struct BreakFoulDecisionAck {
    bool accepted = false;
    std::string decisionId;
    std::string actionId;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::uint64_t previousStateRevision = 0;
    std::uint64_t authoritativeStateRevision = 0;
    BreakFoulDecisionEffects effects;
    std::string authoritativeStateHash;
};

struct BreakFoulDecisionResult {
    bool ok = false;
    std::string errorCode;
    BreakFoulDecisionAck ack;
};

}  // namespace billiardgl::rules
