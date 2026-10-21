#pragma once

#include "chinese_eight_ball_rules.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl { namespace rules {

inline constexpr const char* kConfirmFullTablePlacementActionId =
    "rules.ball_in_hand.confirm_full_table_placement";
inline constexpr const char* kConfirmBehindHeadStringPlacementActionId =
    "rules.ball_in_hand.confirm_behind_head_string";
inline constexpr const char* kFreeBallPlacementCapabilityId =
    "free_ball_placement";
inline constexpr const char* kBallInHandRulesetId =
    "wpa_heyball_2025_08_16_v1";
inline constexpr const char* kFullTableBallInHandFlowId =
    "ball_in_hand_full_table";
inline constexpr const char* kBehindHeadStringBallInHandFlowId =
    "ball_in_hand_behind_head_string";
inline constexpr const char* kBehindHeadStringSourceClause = "WPA-II-08";
inline constexpr const char* kHeadStringDefinitionSourceClause =
    "WPA-II-03-E";
inline constexpr const char* kRequestNearestLegalRespotActionId =
    "rules.ball_in_hand.request_nearest_legal_respot";
inline constexpr const char* kChooseEquidistantRespotBallActionId =
    "rules.ball_in_hand.choose_equidistant_respot_ball";
inline constexpr const char* kBehindHeadStringRespotCapabilityId =
    "behind_head_string_all_behind_respot";
inline constexpr const char* kBehindHeadStringRespotFlowId =
    "behind_head_string_all_behind_respot";
inline constexpr const char* kConsumeBallInHandPositiveImpulseActionId =
    "rules.ball_in_hand.consume_positive_cue_ball_impulse";

enum class BallInHandPlacementScope {
    None,
    FullTable,
    BehindHeadString,
};

struct BallInHandPlacementPosition {
    double worldX = 0.0;
    double worldZ = 0.0;
};

struct BallInHandPlacementOffer {
    std::string decisionId;
    std::uint64_t authorityStateRevision = 0;
    int actorPlayerId = -1;
    BallInHandPlacementScope scope = BallInHandPlacementScope::None;
    double headStringWorldZCm = 0.0;
    std::string sourceClause;
    int sourcePdfPage = 0;
    std::string headStringDefinitionSourceClause;
    int headStringDefinitionPdfPage = 0;
    std::array<std::string, 1> offeredActionIds{{
        kConfirmFullTablePlacementActionId}};
};

struct BallInHandPlacementValidation {
    bool ok = false;
    std::string errorCode;
    BallInHandPlacementPosition authoritativePosition;
};

struct BallInHandPlacementRequest {
    std::string rulesetId = kBallInHandRulesetId;
    std::string capabilityId = kFreeBallPlacementCapabilityId;
    std::string flowId = kFullTableBallInHandFlowId;
    std::string decisionId;
    std::uint64_t expectedAuthorityStateRevision = 0;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::string actionId;
    BallInHandPlacementPosition position;
};

struct BallInHandPlacementEffects {
    int currentPlayer = 0;
    int nextPlayer = 0;
    bool turnContinues = false;
    bool ballInHand = false;
    BallInHandPlacementScope ballInHandScope =
        BallInHandPlacementScope::None;
    bool placementPending = false;
    std::array<BallGroup, 2> groups{{BallGroup::Open, BallGroup::Open}};
    MatchPhase phase = MatchPhase::Break;
    int winner = -1;
    BallInHandPlacementPosition cueBallPosition;
};

struct BallInHandPlacementAck {
    bool accepted = false;
    std::string decisionId;
    std::string actionId;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::uint64_t previousStateRevision = 0;
    std::uint64_t authoritativeStateRevision = 0;
    BallInHandPlacementEffects effects;
    std::string replacementPlacementDecisionId;
    std::string authoritativeStateHash;
};

struct BallInHandPlacementResult {
    bool ok = false;
    std::string errorCode;
    BallInHandPlacementAck ack;
};

struct BehindHeadStringRespotOffer {
    std::string decisionId;
    std::uint64_t authorityStateRevision = 0;
    int actorPlayerId = -1;
    std::string sourcePlacementDecisionId;
    double headStringWorldZCm = 0.0;
    std::vector<int> tiedCandidateBallIds;
    std::array<std::string, 1> offeredActionIds{{
        kChooseEquidistantRespotBallActionId}};
};

struct BehindHeadStringRespotRequest {
    std::string rulesetId = kBallInHandRulesetId;
    std::string capabilityId = kBehindHeadStringRespotCapabilityId;
    std::string flowId = kBehindHeadStringRespotFlowId;
    std::string decisionId;
    std::uint64_t expectedAuthorityStateRevision = 0;
    std::string expectedAuthorityStateHash;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    std::string actionId;
    int selectedBallId = -1;
    std::string payloadCanonical = "{}";
};

struct BehindHeadStringRespotAck {
    bool accepted = false;
    bool respotCommitted = false;
    bool tieChoiceStaged = false;
    std::string decisionId;
    std::string actionId;
    std::string idempotencyKey;
    int actorPlayerId = -1;
    int selectedBallId = -1;
    std::vector<int> tiedCandidateBallIds;
    std::uint64_t previousStateRevision = 0;
    std::uint64_t authoritativeStateRevision = 0;
    std::string replacementPlacementDecisionId;
    std::string authoritativeStateHash;
};

struct BehindHeadStringRespotResult {
    bool ok = false;
    std::string errorCode;
    BehindHeadStringRespotAck ack;
};

} }  // namespace billiardgl::rules
