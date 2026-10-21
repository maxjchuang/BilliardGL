#include "rules/stalemate_authority.h"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const char* message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

class Auth final :
    public billiardgl::rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const billiardgl::rules::RefereeAuthenticationInput& input)
        const override
    {
        return input.credentialId == "credential" &&
            input.keyVersion == "v1" &&
            input.authenticationProof ==
                billiardgl::rules::refereeAuthoritySha256Hex(
                "secret|" + input.authenticatedBytes);
    }
};

class Guard final : public billiardgl::rules::RefereeAuthorityCommitGuard {
public:
    bool allowCommit() const override { return allow; }
    bool allow = true;
};

void sign(billiardgl::rules::RefereeAuthorityRequest& request)
{
    request.authenticationProof =
        billiardgl::rules::refereeAuthoritySha256Hex("secret|" +
        billiardgl::rules::canonicalRefereeAuthenticatedBytes(request));
}

billiardgl::rules::RefereeAuthorityRequest envelope(
    const billiardgl::rules::StalemateAuthority& authority,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& kind, const std::string& subject,
    const std::string& nonce, const std::string& key,
    const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId = kRefereeAuthorityProtocolId;
    request.schemaVersion = 1;
    request.operation = operation;
    request.declarationKind = kind;
    request.decisionId = "stalemate.1";
    request.rackIdentity = authority.state().rackIdentity;
    request.expectedAuthorityRevision =
        authority.protocolState().authorityRevision;
    request.expectedAuthorityStateHash =
        authority.protocolState().authorityStateHash;
    request.authorityRole = subject == "player.0" ?
        RefereeAuthorityRole::Player0 : subject == "player.1" ?
        RefereeAuthorityRole::Player1 : RefereeAuthorityRole::Referee;
    request.authoritySubjectId = subject;
    request.credentialId = "credential";
    request.keyVersion = "v1";
    request.authenticationNonce = nonce;
    request.expiresAtAuthorityTick = 100;
    request.idempotencyKey = key;
    request.payloadCanonical = payload;
    sign(request);
    return request;
}

std::string proposalPayload(const char* directive, int allowance)
{
    return std::string("{\"directive_kind\":\"") + directive +
        "\",\"per_player_shot_allowance\":" +
        std::to_string(allowance) + ",\"stalemate_attested\":true}";
}

std::string refusalPayload(int player)
{
    return std::string("{\"proposal_decision_id\":\"stalemate.1\",") +
        "\"refusing_player_id\":" + std::to_string(player) + "}";
}

std::string finalPayload(const billiardgl::rules::StalemateState& state,
                         bool attested = true)
{
    return std::string("{\"counter_snapshot_sha256\":\"") +
        state.counterSnapshotSha256 + "\",\"directive_kind\":\"" +
        state.directiveKind + "\",\"per_player_completed_shots\":[" +
        std::to_string(state.completedShots[0]) + "," +
        std::to_string(state.completedShots[1]) +
        "],\"per_player_shot_allowance\":" +
        std::to_string(state.perPlayerShotAllowance) +
        ",\"proposal_decision_id\":\"stalemate.1\"," +
        "\"stalemate_still_present_attested\":" +
        (attested ? "true}" : "false}");
}

void testDefaultFlowAndNoAutoFinal()
{
    using namespace billiardgl::rules;
    Auth auth;
    Guard guard;
    StalemateAuthority authority(&auth, &guard);
    authority.reset("rack.1", 0, {0, 1});
    const std::string frozenOrder = authority.state().orderSnapshotSha256;

    auto proposal = envelope(authority, RefereeAuthorityOperation::Propose,
        kStalemateDirectiveKind, "referee", "n1", "k1",
        proposalPayload("DEFAULT_ADVISORY_THREE_EACH", 3));
    const auto proposalAck = authority.propose(proposal, 1);
    expect(proposalAck.ok &&
        authority.state().phase == StalematePhase::ProposalOpen &&
        authority.state().orderSnapshotSha256 == frozenOrder,
        "REFEREE_DEFAULT_THREE_PROPOSAL_FREEZES_RACK_ORDER");
    expect(!authority.recordCompletedShot(0, "before", "rack.1"),
        "PRE_WINDOW_SHOT_NOT_COUNTED");

    auto refusal = envelope(authority, RefereeAuthorityOperation::Propose,
        kStalemateRefusalKind, "player.0", "n2", "k2",
        refusalPayload(0));
    const auto refusalAck = authority.refuse(refusal, 2);
    expect(refusalAck.ok &&
        authority.state().phase == StalematePhase::ContinuationActive &&
        authority.state().refusalHistory == std::vector<int>{0} &&
        !authority.state().proposalDecisionId.empty(),
        "PLAYER_ZERO_SIGNED_REFUSAL_STARTS_WINDOW_AND_RETAINS_HISTORY");
    expect(!authority.recordCompletedShot(0, "shot.1", "rack.other"),
        "REPLAY_OR_OTHER_RACK_COUNT_REJECTED");
    expect(authority.recordCompletedShot(0, "shot.1", "rack.1") &&
        !authority.recordCompletedShot(0, "shot.1", "rack.1") &&
        authority.recordCompletedShot(0, "shot.2", "rack.1") &&
        authority.recordCompletedShot(0, "shot.3", "rack.1") &&
        authority.state().phase == StalematePhase::AwaitingRefereeFinal &&
        !authority.state().finalAuthorized &&
        !authority.state().rerackCommitted,
        "THRESHOLD_REACHES_AWAITING_FINAL_WITHOUT_AUTO_RERACK");

    auto final = envelope(authority,
        RefereeAuthorityOperation::FinalDeclare, kStalemateDirectiveKind,
        "referee", "n3", "k3", finalPayload(authority.state()));
    const auto finalAck = authority.finalDeclare(final, 3);
    expect(finalAck.ok && authority.state().finalAuthorized &&
        !authority.state().rerackCommitted,
        "REFEREE_FINAL_BINDS_EXACT_COUNTER_SNAPSHOT");
    authority.markRerackCommitted();
    expect(authority.state().rerackCommitted &&
        authority.state().originalBreakerPlayerId == 0 &&
        authority.state().originalShootingOrder == std::array<int, 2>{0, 1},
        "ORIGINAL_BREAKER_AND_SHOOTING_ORDER_PRESERVED");
    const auto retry = authority.finalDeclare(final, 4);
    expect(retry.ok && retry.ack.authoritativeRevision ==
            finalAck.ack.authoritativeRevision,
        "FINAL_EXACT_RETRY_ZERO_DELTA_AFTER_RERACK");
}

void testAlternativePlayerOneCancelAndConflicts()
{
    using namespace billiardgl::rules;
    Auth auth;
    Guard guard;
    StalemateAuthority authority(&auth, &guard);
    authority.reset("rack.2", 1, {1, 0});
    auto proposal = envelope(authority, RefereeAuthorityOperation::Propose,
        kStalemateDirectiveKind, "referee", "p1", "pk",
        proposalPayload("EXPLICIT_FINITE_ALTERNATIVE", 5));
    const auto first = authority.propose(proposal, 1);
    const auto retry = authority.propose(proposal, 2);
    expect(first.ok && retry.ok &&
        authority.protocolState().journal.size() == 1 &&
        authority.state().perPlayerShotAllowance == 5,
        "REFEREE_EXPLICIT_ALTERNATIVE_AND_PROPOSAL_EXACT_RETRY");

    auto refusal = envelope(authority, RefereeAuthorityOperation::Propose,
        kStalemateRefusalKind, "player.1", "r1", "rk",
        refusalPayload(1));
    expect(authority.refuse(refusal, 2).ok,
        "PLAYER_ONE_SIGNED_REFUSAL_STARTS_WINDOW");
    const std::size_t refusalCount = authority.state().refusalHistory.size();
    expect(authority.refuse(refusal, 2).ok &&
        authority.state().refusalHistory.size() == refusalCount,
        "REFUSAL_EXACT_RETRY_ZERO_DELTA");
    auto conflict = refusal;
    conflict.payloadCanonical = refusalPayload(0);
    sign(conflict);
    expect(!authority.refuse(conflict, 2).ok &&
        authority.state().refusalHistory.size() == refusalCount,
        "REFUSAL_IDEMPOTENCY_CONFLICT_ZERO_DELTA");

    auto staleFinal = envelope(authority,
        RefereeAuthorityOperation::FinalDeclare, kStalemateDirectiveKind,
        "referee", "f1", "fk", finalPayload(authority.state()));
    authority.recordCompletedShot(1, "later", "rack.2");
    expect(!authority.finalDeclare(staleFinal, 3).ok &&
        !authority.state().finalAuthorized,
        "STALE_COUNTER_SNAPSHOT_FINAL_REJECTED");

    auto cancel = envelope(authority,
        RefereeAuthorityOperation::CancelProposal, kStalemateDirectiveKind,
        "referee", "c1", "ck",
        "{\"proposal_decision_id\":\"stalemate.1\"}");
    expect(authority.cancel(cancel, 3).ok &&
        authority.state().phase == StalematePhase::Cancelled &&
        !authority.state().rerackCommitted,
        "SAME_REFEREE_CANCEL_WITHOUT_RERACK");
    expect(authority.cancel(cancel, 4).ok,
        "CANCEL_EXACT_RETRY_ZERO_DELTA");
}

void testValidationAndAtomicRollback()
{
    using namespace billiardgl::rules;
    Auth auth;
    Guard guard;
    StalemateAuthority authority(&auth, &guard);
    authority.reset("rack.3", 0, {0, 1});
    auto badDefault = envelope(authority, RefereeAuthorityOperation::Propose,
        kStalemateDirectiveKind, "referee", "d1", "dk",
        proposalPayload("DEFAULT_ADVISORY_THREE_EACH", 4));
    expect(!authority.propose(badDefault, 1).ok &&
        authority.state().phase == StalematePhase::Inactive,
        "DEFAULT_ALLOWANCE_NOT_THREE_REJECTED_ZERO_DELTA");

    guard.allow = false;
    auto proposal = envelope(authority, RefereeAuthorityOperation::Propose,
        kStalemateDirectiveKind, "referee", "p3", "pk3",
        proposalPayload("DEFAULT_ADVISORY_THREE_EACH", 3));
    expect(!authority.propose(proposal, 1).ok &&
        authority.state().phase == StalematePhase::Inactive &&
        authority.protocolState().journal.empty(),
        "ATOMIC_PROTOCOL_COMMIT_FAILURE_ROLLBACK");
    guard.allow = true;
    expect(authority.propose(proposal, 1).ok,
        "PROPOSAL_ACCEPTS_AFTER_TRANSIENT_STORAGE_FAILURE");

    auto wrongActor = envelope(authority, RefereeAuthorityOperation::Propose,
        kStalemateRefusalKind, "player.0", "wa", "wak",
        refusalPayload(1));
    expect(!authority.refuse(wrongActor, 2).ok &&
        authority.state().refusalHistory.empty(),
        "WRONG_PLAYER_REFUSAL_ACTOR_REJECTED");
    wrongActor.authenticationProof = "bad";
    expect(!authority.refuse(wrongActor, 2).ok,
        "UNAUTHENTICATED_REFUSAL_REJECTED");
}

}  // namespace

int main()
{
    testDefaultFlowAndNoAutoFinal();
    testAlternativePlayerOneCancelAndConflicts();
    testValidationAndAtomicRollback();
    if (failures != 0) return 1;
    std::cout << "Stalemate authority checks passed\n";
    return 0;
}
