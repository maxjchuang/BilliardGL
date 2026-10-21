#include "rules/frozen_ball_declaration.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace billiardgl::rules;

class Auth final : public RefereeAuthorityAuthenticator {
public:
    bool verify(const RefereeAuthenticationInput& input) const override
    {
        return input.credentialId == "credential" &&
            input.keyVersion == "v1" &&
            input.authenticationProof == refereeAuthoritySha256Hex(
                input.authenticatedBytes);
    }
};

class Guard final : public RefereeAuthorityCommitGuard {
public:
    bool allowCommit() const override { return allow; }
    bool allow = true;
};

void expect(bool value, const char* message)
{
    if (value) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

void sign(RefereeAuthorityRequest& request)
{
    request.authenticationProof = refereeAuthoritySha256Hex(
        canonicalRefereeAuthenticatedBytes(request));
}

std::string proposalPayload(int object = 1)
{
    return std::string(
        "{\"cue_ball_id\":0,\"frozen_attested\":true,") +
        "\"object_ball_id\":" + std::to_string(object) +
        ",\"relation\":\"CUE_BALL_FROZEN_TO_LEGAL_OBJECT_BALL\"}";
}

std::string finalPayload(int object = 1)
{
    return std::string(
        "{\"cue_ball_id\":0,\"frozen_attested\":true,") +
        "\"object_ball_id\":" + std::to_string(object) +
        ",\"proposal_decision_id\":\"frozen.1\"," +
        "\"relation\":\"CUE_BALL_FROZEN_TO_LEGAL_OBJECT_BALL\"}";
}

RefereeAuthorityRequest request(const FrozenBallDeclarationAuthority& authority,
    RefereeAuthorityOperation operation, const std::string& payload,
    const std::string& key)
{
    RefereeAuthorityRequest value;
    value.protocolId = kRefereeAuthorityProtocolId;
    value.schemaVersion = 1;
    value.operation = operation;
    value.declarationKind = kFrozenCueObjectDeclarationKind;
    value.decisionId = "frozen.1";
    value.rackIdentity = authority.state().rackIdentity;
    value.expectedAuthorityRevision =
        authority.protocolState().authorityRevision;
    value.expectedAuthorityStateHash =
        authority.protocolState().authorityStateHash;
    value.authorityRole = RefereeAuthorityRole::Referee;
    value.authoritySubjectId = "referee";
    value.credentialId = "credential";
    value.keyVersion = "v1";
    value.authenticationNonce = "nonce." + key;
    value.expiresAtAuthorityTick = 100;
    value.idempotencyKey = key;
    value.payloadCanonical = payload;
    sign(value);
    return value;
}

FrozenBallDeclarationContext context()
{
    return FrozenBallDeclarationContext{0, 1, 4, "parent.rules.state"};
}

void testProposalFinalConsumeAndExactRetries()
{
    Auth auth;
    FrozenBallDeclarationAuthority authority(&auth);
    authority.reset("rack.1");
    const auto frozenContext = context();
    auto proposal = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(), "proposal");
    const auto proposed = authority.propose(proposal, frozenContext, 1);
    expect(proposed.ok && !authority.state().committed &&
        authority.state().phase == FrozenBallDeclarationPhase::ProposalOpen,
        "REFEREE_PROPOSE_EXACT_PAIR_DOES_NOT_DECLARE");
    const std::size_t proposalJournal =
        authority.protocolState().journal.size();
    expect(authority.propose(proposal, frozenContext, 1).ok &&
        authority.protocolState().journal.size() == proposalJournal,
        "EXACT_PROPOSAL_RETRY_ZERO_DELTA");

    auto final = request(authority, RefereeAuthorityOperation::FinalDeclare,
        finalPayload(), "final");
    const auto finalized = authority.finalDeclare(final, frozenContext, 2);
    expect(finalized.ok && authority.state().committed &&
        authority.state().phase == FrozenBallDeclarationPhase::Active,
        "SAME_REFEREE_FINAL_COMMITS_PRE_SHOT_DECLARATION");
    const std::size_t finalJournal = authority.protocolState().journal.size();
    expect(authority.finalDeclare(final, frozenContext, 2).ok &&
        authority.protocolState().journal.size() == finalJournal &&
        authority.state().phase == FrozenBallDeclarationPhase::Active,
        "EXACT_FINAL_RETRY_ZERO_DELTA");

    expect(authority.consume("shot.1", 3) &&
        authority.state().phase == FrozenBallDeclarationPhase::Consumed &&
        authority.state().consumedShotIdentity == "shot.1",
        "POSITIVE_ACCEPTED_SHOT_CONSUMES_EXACTLY_ONCE");
    expect(!authority.consume("shot.2", 4) &&
        authority.state().consumedShotIdentity == "shot.1",
        "SECOND_SHOT_CANNOT_REUSE");
    expect(authority.finalDeclare(final, frozenContext, 4).ok &&
        authority.state().phase == FrozenBallDeclarationPhase::Consumed,
        "FINAL_RETRY_AFTER_CONSUME_CANNOT_REACTIVATE");
}

void testCancelAndRetry()
{
    Auth auth;
    FrozenBallDeclarationAuthority authority(&auth);
    authority.reset("rack.cancel");
    const auto frozenContext = context();
    auto proposal = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(), "proposal.cancel");
    expect(authority.propose(proposal, frozenContext, 1).ok,
        "CANCEL_FIXTURE_PROPOSAL_ACCEPTED");
    auto cancel = request(authority,
        RefereeAuthorityOperation::CancelProposal,
        "{\"proposal_decision_id\":\"frozen.1\"}", "cancel");
    expect(authority.cancel(cancel, 2).ok &&
        authority.state().phase == FrozenBallDeclarationPhase::Cancelled,
        "SAME_REFEREE_CANCEL_BEFORE_FINAL");
    const auto journal = authority.protocolState().journal.size();
    expect(authority.cancel(cancel, 2).ok &&
        authority.protocolState().journal.size() == journal,
        "EXACT_CANCEL_RETRY_ZERO_DELTA");
}

void testFailClosedInputsAndAtomicGuard()
{
    Auth auth;
    Guard guard;
    FrozenBallDeclarationAuthority authority(&auth, &guard);
    authority.reset("rack.negative");
    const auto frozenContext = context();

    auto bad = request(authority, RefereeAuthorityOperation::Propose,
        "{\"geometry_frozen\":true}", "geometry");
    expect(!authority.propose(bad, frozenContext, 1).ok,
        "GEOMETRY_ONLY_OR_FROZEN_TOPOLOGY_CANNOT_DECLARE");
    bad = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(), "player");
    bad.authorityRole = RefereeAuthorityRole::Player0;
    sign(bad);
    expect(!authority.propose(bad, frozenContext, 1).ok,
        "PLAYER_DECLARATION_REJECTED");
    bad = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(), "unauthenticated");
    bad.authenticationProof = "invalid";
    expect(!authority.propose(bad, frozenContext, 1).ok,
        "UNAUTHENTICATED_DECLARATION_REJECTED");
    bad = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(), "expired");
    bad.expiresAtAuthorityTick = 0;
    sign(bad);
    expect(!authority.propose(bad, frozenContext, 1).ok,
        "EXPIRED_DECLARATION_REJECTED");
    bad = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(2), "wrong-object");
    expect(!authority.propose(bad, frozenContext, 1).ok,
        "WRONG_CUE_OBJECT_OR_RELATION_REJECTED");
    bad = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload().substr(0, proposalPayload().size() - 1) +
            ",\"extra\":true}", "extra");
    expect(!authority.propose(bad, frozenContext, 1).ok,
        "UNKNOWN_EXTRA_FIELD_REJECTED");
    auto earlyFinal = request(authority,
        RefereeAuthorityOperation::FinalDeclare, finalPayload(),
        "early-final");
    expect(!authority.finalDeclare(earlyFinal, frozenContext, 1).ok,
        "FINAL_BEFORE_PROPOSAL_REJECTED");
    expect(authority.state().phase == FrozenBallDeclarationPhase::Inactive &&
        authority.protocolState().journal.empty(),
        "ALL_BAD_INPUTS_HAVE_ZERO_STATE_DELTA");

    guard.allow = false;
    auto guarded = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(), "guarded");
    expect(!authority.propose(guarded, frozenContext, 1).ok &&
        authority.state().phase == FrozenBallDeclarationPhase::Inactive &&
        authority.protocolState().journal.empty(),
        "COMMIT_GUARD_FAILURE_IS_ATOMIC");
}

}  // namespace

int main()
{
    testProposalFinalConsumeAndExactRetries();
    testCancelAndRetry();
    testFailClosedInputsAndAtomicGuard();
    std::cout << "frozen_ball_declaration_tests: PASS\n";
    return 0;
}
