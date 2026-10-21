#include "rules/referee_authority_protocol.h"

#include "deterministic_hash.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

class TestAuthenticator final :
    public billiardgl::rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const billiardgl::rules::RefereeAuthenticationInput& input)
        const override
    {
        return input.credentialId == "credential.referee.alpha" &&
            input.keyVersion == "test-key-v1" &&
            input.authenticationProof == billiardgl::sha256Hex(
                "test-only-secret|" + input.authenticatedBytes);
    }
};

class TestCommitGuard final :
    public billiardgl::rules::RefereeAuthorityCommitGuard {
public:
    bool allowCommit() const override { return allow; }
    bool allow = true;
};

billiardgl::rules::RefereeAuthorityConsumerPolicy refereePolicy()
{
    using namespace billiardgl::rules;
    RefereeAuthorityConsumerPolicy value;
    value.declarationKind = "TEST_REFEREE_ONLY_DECLARATION";
    value.allowedActorRoles = {RefereeAuthorityRole::Referee};
    value.exactPayloadKeyTypes = {
        {"note", RefereeAuthorityPayloadType::String}};
    value.maxPayloadBytes = 128;
    value.maxLifetimeTicks = 20;
    value.proposalRequired = true;
    value.sameActorCancelRequired = true;
    return value;
}

billiardgl::rules::RefereeAuthorityConsumerPolicy playerPolicy()
{
    auto value = refereePolicy();
    value.declarationKind = "TEST_PLAYER_ALLOWED_DECLARATION";
    value.allowedActorRoles = {
        billiardgl::rules::RefereeAuthorityRole::Player0,
        billiardgl::rules::RefereeAuthorityRole::Player1};
    return value;
}

billiardgl::rules::RefereeAuthorityRequest request(
    const billiardgl::rules::RefereeAuthorityProtocol& protocol,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& idempotencyKey,
    const std::string& nonce)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest value;
    value.protocolId = kRefereeAuthorityProtocolId;
    value.schemaVersion = 1;
    value.operation = operation;
    value.declarationKind = "TEST_REFEREE_ONLY_DECLARATION";
    value.decisionId = "test.decision.1";
    value.rackIdentity = protocol.state().rackIdentity;
    value.expectedAuthorityRevision = protocol.state().authorityRevision;
    value.expectedAuthorityStateHash = protocol.state().authorityStateHash;
    value.authorityRole = RefereeAuthorityRole::Referee;
    value.authoritySubjectId = "referee.alpha";
    value.credentialId = "credential.referee.alpha";
    value.keyVersion = "test-key-v1";
    value.authenticationNonce = nonce;
    value.expiresAtAuthorityTick = 10;
    value.idempotencyKey = idempotencyKey;
    value.payloadCanonical = "{\"note\":\"frozen\"}";
    value.authenticationProof = billiardgl::sha256Hex(
        "test-only-secret|" + canonicalRefereeAuthenticatedBytes(value));
    return value;
}

void resign(billiardgl::rules::RefereeAuthorityRequest& value)
{
    value.authenticationProof = billiardgl::sha256Hex(
        "test-only-secret|" +
        billiardgl::rules::canonicalRefereeAuthenticatedBytes(value));
}

void testPositiveProposalFinalCancelAndPlayerRole()
{
    using namespace billiardgl::rules;
    TestAuthenticator authenticator;
    TestCommitGuard guard;
    RefereeAuthorityProtocol protocol(
        {refereePolicy(), playerPolicy()}, &authenticator, &guard);
    protocol.reset("rack.test.1");

    const auto proposalRequest = request(protocol,
        RefereeAuthorityOperation::Propose, "proposal-key", "nonce-1");
    const auto proposal = protocol.submit(proposalRequest, 1);
    expect(proposal.ok && proposal.ack.accepted &&
        proposal.ack.proposalPending &&
        !proposal.ack.finalDeclarationCommitted &&
        protocol.state().liveProposal.has_value() &&
        protocol.state().journal.size() == 1,
        "AUTHENTICATED_REFEREE_PROPOSAL_STAGES_OPAQUE_RECORD");

    const auto retry = protocol.submit(proposalRequest, 2);
    expect(retry.ok && retry.ack.authoritativeRevision ==
            proposal.ack.authoritativeRevision &&
        protocol.state().journal.size() == 1,
        "EXACT_RETRY_RETURNS_ORIGINAL_ACK_ZERO_DELTA");

    auto finalRequest = request(protocol,
        RefereeAuthorityOperation::FinalDeclare, "final-key", "nonce-2");
    const auto final = protocol.submit(finalRequest, 3);
    expect(final.ok && final.ack.finalDeclarationCommitted &&
        !final.ack.proposalPending &&
        protocol.state().latestFinalDeclaration.has_value() &&
        protocol.state().journal.size() == 2,
        "AUTHENTICATED_FINAL_REVALIDATES_AND_COMMITS");

    protocol.reset("rack.test.2");
    auto player = request(protocol, RefereeAuthorityOperation::Propose,
        "player-key", "nonce-player");
    player.declarationKind = "TEST_PLAYER_ALLOWED_DECLARATION";
    player.authorityRole = RefereeAuthorityRole::Player0;
    player.authoritySubjectId = "player.0";
    resign(player);
    expect(protocol.submit(player, 1).ok,
        "EXPLICITLY_ALLOWED_PLAYER_ROLE_PROPOSAL_ONLY");

    auto cancel = request(protocol, RefereeAuthorityOperation::CancelProposal,
        "cancel-key", "nonce-cancel");
    cancel.declarationKind = player.declarationKind;
    cancel.decisionId = player.decisionId;
    cancel.authorityRole = player.authorityRole;
    cancel.authoritySubjectId = player.authoritySubjectId;
    resign(cancel);
    const auto cancelled = protocol.submit(cancel, 2);
    expect(cancelled.ok && !cancelled.ack.proposalPending &&
        !cancelled.ack.finalDeclarationCommitted &&
        !protocol.state().latestFinalDeclaration.has_value(),
        "CANCEL_BY_SAME_AUTHORITY_CONSUMES_WITHOUT_FINAL");
}

void testNegativeAuthenticationSchemaContextExpiryAndConflicts()
{
    using namespace billiardgl::rules;
    TestAuthenticator authenticator;
    TestCommitGuard guard;
    RefereeAuthorityProtocol protocol(
        {refereePolicy(), playerPolicy()}, &authenticator, &guard);
    protocol.reset("rack.test.negative");
    const std::string initialHash = protocol.state().authorityStateHash;

    auto rejectZeroDelta = [&](RefereeAuthorityRequest bad,
                               const std::string& label) {
        const auto result = protocol.submit(bad, 1);
        expect(!result.ok && protocol.state().authorityRevision == 0 &&
            protocol.state().authorityStateHash == initialHash &&
            protocol.state().journal.empty(), label);
    };

    auto bad = request(protocol, RefereeAuthorityOperation::Propose,
        "bad-role", "bad-role-nonce");
    bad.authorityRole = RefereeAuthorityRole::Player0;
    resign(bad);
    rejectZeroDelta(bad, "UNKNOWN_ROLE_OR_PLAYER_WHEN_REFEREE_REQUIRED");

    bad = request(protocol, RefereeAuthorityOperation::Propose,
        "bad-proof", "bad-proof-nonce");
    bad.authenticationProof = "bad";
    rejectZeroDelta(bad, "MISSING_OR_BAD_AUTHENTICATION_PROOF");

    bad = request(protocol, RefereeAuthorityOperation::Propose,
        "bad-subject", "bad-subject-nonce");
    bad.credentialId = "other";
    resign(bad);
    rejectZeroDelta(bad, "WRONG_CREDENTIAL_KEY_VERSION_OR_SUBJECT");

    bad = request(protocol, RefereeAuthorityOperation::Propose,
        "bad-context", "bad-context-nonce");
    bad.rackIdentity = "wrong-rack";
    resign(bad);
    rejectZeroDelta(bad, "WRONG_DECISION_RACK_REVISION_OR_STATE_HASH");

    bad = request(protocol, RefereeAuthorityOperation::Propose,
        "expired", "expired-nonce");
    bad.expiresAtAuthorityTick = 0;
    resign(bad);
    rejectZeroDelta(bad, "EXPIRED_OR_EXCESS_LIFETIME");

    bad = request(protocol, RefereeAuthorityOperation::Propose,
        "payload", "payload-nonce");
    bad.payloadCanonical = "{ \"note\": \"frozen\" }";
    resign(bad);
    rejectZeroDelta(bad,
        "MALFORMED_NONCANONICAL_UNKNOWN_OR_OVERSIZE_PAYLOAD");

    bad = request(protocol, RefereeAuthorityOperation::Propose,
        "duplicate-payload", "duplicate-payload-nonce");
    bad.payloadCanonical =
        "{\"note\":\"first\",\"note\":\"second\"}";
    resign(bad);
    rejectZeroDelta(bad, "DUPLICATE_PAYLOAD_KEYS_FAIL_CLOSED");

    bad = request(protocol, RefereeAuthorityOperation::FinalDeclare,
        "orphan-final", "orphan-final-nonce");
    rejectZeroDelta(bad,
        "FINAL_WITHOUT_PROPOSAL_OR_PROPOSAL_FINAL_FIELD_DRIFT");

    bad = request(protocol, RefereeAuthorityOperation::Propose,
        "unknown-operation", "unknown-operation-nonce");
    bad.operation = static_cast<RefereeAuthorityOperation>(99);
    resign(bad);
    rejectZeroDelta(bad, "UNKNOWN_OPERATION_FAILS_CLOSED");
}

void testConflictAtomicRollbackResetAndNoPhysicsClaims()
{
    using namespace billiardgl::rules;
    TestAuthenticator authenticator;
    TestCommitGuard guard;
    RefereeAuthorityProtocol protocol(
        {refereePolicy(), playerPolicy()}, &authenticator, &guard);
    protocol.reset("rack.test.atomic");
    const auto accepted = request(protocol, RefereeAuthorityOperation::Propose,
        "accepted-key", "accepted-nonce");
    expect(protocol.submit(accepted, 1).ok, "proposal prerequisite");
    const auto before = protocol.state();

    auto conflict = accepted;
    conflict.payloadCanonical = "{\"note\":\"changed\"}";
    resign(conflict);
    expect(!protocol.submit(conflict, 2).ok &&
        protocol.state().authorityRevision == before.authorityRevision &&
        protocol.state().journal.size() == before.journal.size(),
        "NONCE_REPLAY_OR_IDEMPOTENCY_CONFLICT");

    auto wrongCancel = request(protocol,
        RefereeAuthorityOperation::CancelProposal,
        "wrong-cancel", "wrong-cancel-nonce");
    wrongCancel.authoritySubjectId = "referee.beta";
    resign(wrongCancel);
    expect(!protocol.submit(wrongCancel, 2).ok &&
        protocol.state().liveProposal.has_value(),
        "WRONG_CANCEL_AUTHORITY");

    auto finalRequest = request(protocol,
        RefereeAuthorityOperation::FinalDeclare,
        "atomic-final", "atomic-final-nonce");
    guard.allow = false;
    const auto failed = protocol.submit(finalRequest, 2);
    expect(!failed.ok &&
        protocol.state().authorityRevision == before.authorityRevision &&
        protocol.state().journal.size() == before.journal.size() &&
        protocol.state().liveProposal.has_value() &&
        !protocol.state().latestFinalDeclaration.has_value(),
        "ATOMIC_STORAGE_OR_JOURNAL_FAILURE_ROLLS_BACK");

    expect(protocol.state().latestFinalDeclaration == std::nullopt &&
        protocol.state().journal.front().declarationKind ==
            "TEST_REFEREE_ONLY_DECLARATION",
        "REFEREE_DECLARATION_NEVER_APPEARS_AS_PHYSICS_SHOTFACT_OR_FOUL");

    guard.allow = true;
    const std::string oldStateHash = protocol.state().authorityStateHash;
    protocol.reset("rack.test.reset");
    expect(!protocol.state().liveProposal.has_value() &&
        protocol.state().journal.empty() &&
        protocol.state().authorityStateHash != oldStateHash &&
        !protocol.submit(accepted, 2).ok,
        "RESET_INVALIDATES_PROPOSAL_NONCE_AND_RECEIPT");
}

}  // namespace

int main()
{
    testPositiveProposalFinalCancelAndPlayerRole();
    testNegativeAuthenticationSchemaContextExpiryAndConflicts();
    testConflictAtomicRollbackResetAndNoPhysicsClaims();
    if (failures != 0) {
        std::cerr << failures << " referee authority protocol checks failed\n";
        return 1;
    }
    std::cout << "Referee authority protocol checks passed\n";
    return 0;
}
