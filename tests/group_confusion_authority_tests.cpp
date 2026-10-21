#include "rules/group_confusion_authority.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

class Auth final : public billiardgl::rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const billiardgl::rules::RefereeAuthenticationInput& input)
        const override
    {
        return input.credentialId == "credential" &&
            input.keyVersion == "v1" &&
            input.authenticationProof ==
                billiardgl::rules::refereeAuthoritySha256Hex(
                    input.authenticatedBytes);
    }
};

class Guard final : public billiardgl::rules::RefereeAuthorityCommitGuard {
public:
    bool allow = true;
    bool allowCommit() const override { return allow; }
};

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void sign(billiardgl::rules::RefereeAuthorityRequest& request)
{
    request.authenticationProof =
        billiardgl::rules::refereeAuthoritySha256Hex(
            billiardgl::rules::canonicalRefereeAuthenticatedBytes(request));
}

billiardgl::rules::GroupConfusionWindow window()
{
    billiardgl::rules::GroupConfusionWindow value;
    value.rackIdentity = "rack.1";
    value.originalBreakerPlayerId = 1;
    value.originalShootingOrder = {1, 0};
    value.assignmentSnapshotSha256 = "assignment.sha";
    value.completedShotIdentity = "shot.7";
    value.completedShotOrdinal = 7;
    value.completedShooter = 0;
    value.completedRevision = 41;
    value.completedStateHash = "rules.sha";
    return value;
}

std::string proposalPayload(
    const billiardgl::rules::GroupConfusionState& state)
{
    return std::string("{\"assignment_snapshot_sha256\":\"") +
        state.window.assignmentSnapshotSha256 +
        "\",\"claimed_confusion_kind\":\"ASSIGNMENT_OR_SEQUENCE_CONFUSION\"," +
        "\"completed_shot_identity\":\"" +
        state.window.completedShotIdentity +
        "\",\"realization_attested\":true,\"window_generation\":" +
        std::to_string(state.windowGeneration) + "}";
}

std::string finalPayload(
    const billiardgl::rules::GroupConfusionState& state)
{
    return std::string("{\"assignment_snapshot_sha256\":\"") +
        state.window.assignmentSnapshotSha256 +
        "\",\"claimed_confusion_kind\":\"ASSIGNMENT_OR_SEQUENCE_CONFUSION\"," +
        "\"completed_shot_identity\":\"" +
        state.window.completedShotIdentity +
        "\",\"proposal_decision_id\":\"" + state.proposalDecisionId +
        "\",\"rack_order_snapshot_sha256\":\"" +
        state.rackOrderSnapshotSha256 +
        "\",\"realization_still_attested\":true,\"window_generation\":" +
        std::to_string(state.windowGeneration) + "}";
}

billiardgl::rules::RefereeAuthorityRequest request(
    const billiardgl::rules::GroupConfusionAuthority& authority,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& payload, const std::string& key,
    billiardgl::rules::RefereeAuthorityRole role =
        billiardgl::rules::RefereeAuthorityRole::Referee,
    const std::string& subject = "referee")
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest value;
    value.protocolId = kRefereeAuthorityProtocolId;
    value.schemaVersion = 1;
    value.operation = operation;
    value.declarationKind = kGroupConfusionDeclarationKind;
    value.decisionId = "confusion.1";
    value.rackIdentity = authority.state().window.rackIdentity;
    value.expectedAuthorityRevision =
        authority.protocolState().authorityRevision;
    value.expectedAuthorityStateHash =
        authority.protocolState().authorityStateHash;
    value.authorityRole = role;
    value.authoritySubjectId = subject;
    value.credentialId = "credential";
    value.keyVersion = "v1";
    value.authenticationNonce = "nonce." + key;
    value.expiresAtAuthorityTick = 100;
    value.idempotencyKey = key;
    value.payloadCanonical = payload;
    sign(value);
    return value;
}

void testWindowProposalFinalAndRetry()
{
    using namespace billiardgl::rules;
    Auth auth;
    Guard guard;
    GroupConfusionAuthority authority(&auth, &guard);
    authority.reset("rack.1", 1, {1, 0});
    expect(authority.openWindow(window()), "closed-table window opens");
    const auto before = authority.state();
    auto propose = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(before), "p1", RefereeAuthorityRole::Player0,
        "player.0");
    const auto accepted = authority.propose(propose, 1);
    expect(accepted.ok && authority.rackStopped() &&
        authority.state().phase == GroupConfusionPhase::RealizationProposed,
        "signed player proposal stops rack without rerack");
    const auto retry = authority.propose(propose, 1);
    expect(retry.ok && retry.ack.authoritativeRevision ==
        accepted.ack.authoritativeRevision, "proposal retry is idempotent");
    auto conflict = propose;
    conflict.payloadCanonical += " ";
    sign(conflict);
    expect(!authority.propose(conflict, 1).ok,
        "conflicting proposal fails closed");

    auto final = request(authority,
        RefereeAuthorityOperation::FinalDeclare,
        finalPayload(authority.state()), "f1",
        RefereeAuthorityRole::Player0, "player.0");
    const auto finalized = authority.finalDeclare(final, 2);
    expect(finalized.ok && authority.state().finalAuthorized &&
        !authority.state().rerackCommitted,
        "same actor final authorizes but does not claim rack commit");
    authority.markRerackCommitted();
    expect(authority.state().phase == GroupConfusionPhase::RerackCommitted &&
        authority.state().rerackCommitted, "parent marks verified rerack");
    expect(authority.finalDeclare(final, 2).ok,
        "final exact retry returns original ack");
}

void testFailClosedActorsPayloadDeadlineAndGuard()
{
    using namespace billiardgl::rules;
    Auth auth;
    Guard guard;
    GroupConfusionAuthority authority(&auth, &guard);
    authority.reset("rack.1", 1, {1, 0});
    expect(authority.openWindow(window()), "window opens");
    auto bad = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(authority.state()), "bad");
    bad.payloadCanonical =
        "{\"ordinary_wrong_first_contact_only\":true}";
    sign(bad);
    expect(!authority.propose(bad, 1).ok && !authority.rackStopped(),
        "ordinary wrong-first payload cannot become section 19");
    auto cancelled = request(authority,
        RefereeAuthorityOperation::CancelProposal,
        "{}", "cancel");
    expect(!authority.cancel(cancelled, 1).ok,
        "cancel is unsupported");
    authority.expireWindowOnAcceptedShoot();
    auto late = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(authority.state()), "late");
    expect(!authority.propose(late, 2).ok,
        "accepted next shoot expires prior window");

    authority.reset("rack.1", 1, {1, 0});
    expect(authority.openWindow(window()), "window reopens after reset");
    guard.allow = false;
    auto guarded = request(authority, RefereeAuthorityOperation::Propose,
        proposalPayload(authority.state()), "guarded");
    expect(!authority.propose(guarded, 1).ok && !authority.rackStopped(),
        "commit guard failure leaves authority unchanged");
}

}  // namespace

int main()
{
    testWindowProposalFinalAndRetry();
    testFailClosedActorsPayloadDeadlineAndGuard();
    std::cout << "group_confusion_authority_tests: PASS\n";
    return 0;
}
