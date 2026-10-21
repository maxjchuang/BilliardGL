#pragma once

#include "chinese_eight_ball_rules.h"

#include <array>
#include <cstdint>
#include <string>

namespace billiardgl { namespace rules {

inline constexpr const char* kGroupChoiceRulesetId =
    "wpa_heyball_2025_08_16_v1";
inline constexpr const char* kGroupChoiceAuthorityCapabilityId =
    "simultaneous_contact_and_group_choice";
inline constexpr const char* kGroupChoiceAuthorityFlowId =
    "simultaneous_group_choice";
inline constexpr const char* kGroupChoiceSolidsActionId =
    "rules.group_choice.choose_solids";
inline constexpr const char* kGroupChoiceStripesActionId =
    "rules.group_choice.choose_stripes";

struct GroupChoiceDecisionOffer {
    std::string decisionId;
    std::uint64_t authorityStateRevision = 0;
    int actorPlayerId = -1;
    std::array<std::string, 2> offeredActionIds{{
        kGroupChoiceSolidsActionId, kGroupChoiceStripesActionId}};
    bool continueTurn = false;
    int nextPlayer = 0;
};

struct GroupChoiceDecisionRequest {
    std::string rulesetId;
    std::string capabilityId;
    std::string flowId;
    std::string decisionId;
    std::uint64_t expectedAuthorityStateRevision = 0;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::string actionId;
    // The current transaction has no action-specific arguments. Keeping the
    // canonical payload in the request makes idempotency comparisons exact.
    std::string payloadCanonical = "{}";
};

struct GroupChoiceDecisionEffects {
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool turnContinues = false;
    bool ballInHand = false;
    std::array<BallGroup, 2> groups{{BallGroup::Open, BallGroup::Open}};
    MatchPhase phase = MatchPhase::Break;
    int winner = -1;
    bool groupChoicePending = false;
};

struct GroupChoiceDecisionAck {
    bool accepted = false;
    std::string decisionId;
    std::string actionId;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::uint64_t previousStateRevision = 0;
    std::uint64_t authoritativeStateRevision = 0;
    GroupChoiceDecisionEffects effects;
    std::string authoritativeStateHash;
};

struct GroupChoiceDecisionResult {
    bool ok = false;
    std::string errorCode;
    GroupChoiceDecisionAck ack;
};

struct RulesAuthorityAction {
    std::uint64_t sequence = 0;
    std::uint64_t tick = 0;
    std::string decisionId;
    std::string actionId;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::uint64_t previousStateRevision = 0;
    std::uint64_t authoritativeStateRevision = 0;
    std::string authoritativeStateHash;
    std::string payloadCanonical;
};

} }  // namespace billiardgl::rules
