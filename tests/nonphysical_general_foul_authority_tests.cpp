#include "rules/nonphysical_general_foul_authority.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace billiardgl::rules;

namespace {

class Auth final : public RefereeAuthorityAuthenticator {
    bool verify(const RefereeAuthenticationInput& input) const override
    {
        return input.authenticationProof ==
            refereeAuthoritySha256Hex(input.authenticatedBytes);
    }
};

class Guard final : public RefereeAuthorityCommitGuard {
public:
    bool allow = true;
    bool allowCommit() const override { return allow; }
};

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void sign(RefereeAuthorityRequest& request)
{
    request.authenticationProof = refereeAuthoritySha256Hex(
        canonicalRefereeAuthenticatedBytes(request));
}

RefereeAuthorityRequest request(
    const NonphysicalGeneralFoulAuthority& authority,
    RefereeAuthorityOperation operation, const std::string& decision,
    const std::string& payload, const std::string& key)
{
    RefereeAuthorityRequest value;
    value.protocolId = kRefereeAuthorityProtocolId;
    value.schemaVersion = 1;
    value.operation = operation;
    value.declarationKind = kNonphysicalGeneralFoulKind;
    value.decisionId = decision;
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
    value.expiresAtAuthorityTick = 1000;
    value.idempotencyKey = key;
    value.payloadCanonical = payload;
    sign(value);
    return value;
}

NonphysicalGeneralFoulContext context(int player, const std::string& shot,
                                      std::uint64_t revision)
{
    return {player, 1 - player, shot, revision,
        "parent." + std::to_string(revision)};
}

struct Transaction {
    NonphysicalGeneralFoulContext context;
    RefereeAuthorityRequest proposal;
    RefereeAuthorityRequest final;
};

Transaction transact(NonphysicalGeneralFoulAuthority& authority,
                     NonphysicalGeneralFoulKind kind, int player,
                     const std::string& identity, std::uint64_t tick)
{
    Transaction value;
    const std::string shot = "shot." + identity;
    const std::string decision = "decision." + identity;
    value.context = context(player, shot, tick);
    value.proposal = request(authority, RefereeAuthorityOperation::Propose,
        decision, canonicalNonphysicalGeneralFoulProposal(
            kind, player, shot), "proposal." + identity);
    const auto proposed = authority.propose(value.proposal, value.context, tick);
    expect(proposed.ok && !proposed.newlyCommitted &&
            proposed.outcome == NonphysicalGeneralFoulOutcome::None,
        "proposal must be authenticated but effect-free");
    value.final = request(authority,
        RefereeAuthorityOperation::FinalDeclare, decision,
        canonicalNonphysicalGeneralFoulFinal(
            kind, player, decision, shot), "final." + identity);
    return value;
}

void lifecycleAndSequentialTransactions()
{
    Auth auth;
    NonphysicalGeneralFoulAuthority authority(&auth, nullptr, {true});
    authority.reset("rack.1");
    auto first = transact(authority,
        NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot,
        0, "ordinary.1", 1);
    const auto committed = authority.finalDeclare(
        first.final, first.context, 2);
    expect(committed.ok && committed.newlyCommitted &&
            !committed.idempotentReplay &&
            committed.outcome ==
                NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand &&
            authority.state().committed,
        "signed ordinary final produces one general-foul outcome");

    auto second = transact(authority,
        NonphysicalGeneralFoulKind::OneHandShot, 1, "ordinary.2", 3);
    expect(authority.finalDeclare(second.final, second.context, 4).ok,
        "a completed transaction does not block a later rack event");
    const std::uint64_t revision =
        authority.protocolState().authorityRevision;
    const auto oldProposalRetry = authority.propose(
        first.proposal, first.context, 5);
    const auto oldFinalRetry = authority.finalDeclare(
        first.final, first.context, 5);
    expect(oldProposalRetry.ok && oldProposalRetry.idempotentReplay &&
            oldProposalRetry.outcome == NonphysicalGeneralFoulOutcome::None &&
            oldFinalRetry.ok && oldFinalRetry.idempotentReplay &&
            !oldFinalRetry.newlyCommitted &&
            oldFinalRetry.outcome ==
                NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand &&
            authority.protocolState().authorityRevision == revision,
        "old exact receipts remain zero-delta after later transactions");

    RefereeAuthorityRequest conflict = first.final;
    conflict.payloadCanonical += " ";
    sign(conflict);
    expect(!authority.finalDeclare(conflict, first.context, 5).ok &&
            authority.protocolState().authorityRevision == revision,
        "same idempotency key with changed authenticated bytes conflicts");
}

void mobilePerPlayerRackHistory()
{
    Auth auth;
    NonphysicalGeneralFoulAuthority authority(&auth, nullptr, {true});
    authority.reset("rack.mobile.1");

    auto player0First = transact(authority,
        NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
        0, "p0.first", 10);
    const auto firstResult = authority.finalDeclare(
        player0First.final, player0First.context, 11);
    expect(firstResult.ok && firstResult.newlyCommitted &&
            firstResult.outcome ==
                NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand &&
            authority.state().mobileDeviceViolations[0] ==
                MobileDeviceViolationState::GeneralFoulCommitted &&
            authority.state().mobileDeviceViolations[1] ==
                MobileDeviceViolationState::None,
        "first mobile violation is player-scoped ordinary general foul");

    const auto player1PrematureContext = context(1, "shot.p1.early", 12);
    const auto player1Premature = request(authority,
        RefereeAuthorityOperation::Propose, "decision.p1.early",
        canonicalNonphysicalGeneralFoulProposal(
            NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            1, player1PrematureContext.shotIdentity), "proposal.p1.early");
    expect(!authority.propose(player1Premature,
                player1PrematureContext, 12).ok &&
            authority.state().mobileDeviceViolations[1] ==
                MobileDeviceViolationState::None,
        "one player's first violation cannot authorize the other's repeat");

    auto player1First = transact(authority,
        NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
        1, "p1.first", 13);
    expect(authority.finalDeclare(
            player1First.final, player1First.context, 14).ok &&
            authority.state().mobileDeviceViolations[1] ==
                MobileDeviceViolationState::GeneralFoulCommitted,
        "other player retains an independent first-offense lane");

    const std::uint64_t beforeOldRetry =
        authority.protocolState().authorityRevision;
    const auto oldRetry = authority.finalDeclare(
        player0First.final, player0First.context, 15);
    expect(oldRetry.ok && oldRetry.idempotentReplay &&
            oldRetry.outcome ==
                NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand &&
            authority.protocolState().authorityRevision == beforeOldRetry,
        "first-offense receipt survives another player's transaction");

    auto player0Repeat = transact(authority,
        NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
        0, "p0.repeat", 16);
    const auto repeatResult = authority.finalDeclare(
        player0Repeat.final, player0Repeat.context, 17);
    expect(repeatResult.ok && repeatResult.newlyCommitted &&
            repeatResult.outcome ==
                NonphysicalGeneralFoulOutcome::LossOfRack &&
            authority.state().mobileDeviceViolations[0] ==
                MobileDeviceViolationState::RackLossCommitted,
        "same-player explicit signed repeat saturates at loss of rack");
    const std::uint64_t afterRepeat =
        authority.protocolState().authorityRevision;
    const auto repeatRetry = authority.finalDeclare(
        player0Repeat.final, player0Repeat.context, 18);
    expect(repeatRetry.ok && repeatRetry.idempotentReplay &&
            !repeatRetry.newlyCommitted &&
            repeatRetry.outcome == NonphysicalGeneralFoulOutcome::LossOfRack &&
            authority.protocolState().authorityRevision == afterRepeat,
        "repeat-loss exact retry cannot produce a second rack loss");

    const auto thirdContext = context(0, "shot.p0.third", 19);
    const auto third = request(authority, RefereeAuthorityOperation::Propose,
        "decision.p0.third", canonicalNonphysicalGeneralFoulProposal(
            NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            0, thirdContext.shotIdentity), "proposal.p0.third");
    expect(!authority.propose(third, thirdContext, 19).ok &&
            authority.protocolState().authorityRevision == afterRepeat,
        "saturated history rejects a third mobile penalty without overflow");

    authority.reset("rack.mobile.2");
    const auto staleProposal = authority.propose(
        player0Repeat.proposal, player0Repeat.context, 20);
    const auto staleFinal = authority.finalDeclare(
        player0Repeat.final, player0Repeat.context, 20);
    expect(authority.state().mobileDeviceViolations[0] ==
                MobileDeviceViolationState::None &&
            authority.state().mobileDeviceViolations[1] ==
                MobileDeviceViolationState::None &&
            !staleProposal.ok &&
            staleProposal.errorCode == "referee_authority_state_stale" &&
            !staleFinal.ok &&
            staleFinal.errorCode == "nonphysical_general_foul_final_state",
        "new rack clears both player histories and invalidates old receipts");
}

void commitFailureIsAtomic()
{
    Auth auth;
    Guard guard;
    NonphysicalGeneralFoulAuthority authority(&auth, &guard);
    authority.reset("rack.guard");
    auto first = transact(authority,
        NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
        0, "guarded", 1);
    guard.allow = false;
    const std::uint64_t revision =
        authority.protocolState().authorityRevision;
    const auto rejected = authority.finalDeclare(first.final, first.context, 2);
    expect(!rejected.ok && !authority.state().committed &&
            authority.state().phase ==
                NonphysicalGeneralFoulPhase::ProposalOpen &&
            authority.state().mobileDeviceViolations[0] ==
                MobileDeviceViolationState::None &&
            authority.protocolState().authorityRevision == revision,
        "failed signed final leaves protocol and rack history unchanged");
}

void legacyConsumerFailsClosed()
{
    Auth auth;
    NonphysicalGeneralFoulAuthority authority(&auth);
    authority.reset("rack.legacy-consumer");
    auto first = transact(authority,
        NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
        0, "legacy.first", 1);
    expect(authority.finalDeclare(first.final, first.context, 2).ok,
        "legacy consumer retains the existing first-offense transaction");
    const auto repeatContext = context(0, "shot.legacy.repeat", 3);
    const auto repeat = request(authority, RefereeAuthorityOperation::Propose,
        "decision.legacy.repeat", canonicalNonphysicalGeneralFoulProposal(
            NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
            0, repeatContext.shotIdentity), "proposal.legacy.repeat");
    const std::uint64_t revision = authority.protocolState().authorityRevision;
    expect(!authority.propose(repeat, repeatContext, 3).ok &&
            authority.protocolState().authorityRevision == revision &&
            authority.state().mobileDeviceViolations[0] ==
                MobileDeviceViolationState::GeneralFoulCommitted,
        "consumer that cannot apply structured outcomes cannot mis-map repeat loss");
}

void exclusions()
{
    Auth auth;
    NonphysicalGeneralFoulAuthority authority(&auth);
    authority.reset("rack.exclusions");
    const auto current = context(0, "shot.exclusions", 1);
    for (const std::string payload : {
        "{\"event_kind\":\"DOUBLE_HIT\",\"offending_player\":0,\"shot_identity\":\"shot.exclusions\"}",
        "{\"event_kind\":\"OBJECT_BALL_OFF_TABLE\",\"offending_player\":0,\"shot_identity\":\"shot.exclusions\"}"}) {
        auto value = request(authority, RefereeAuthorityOperation::Propose,
            "decision.exclusions", payload, payload);
        expect(!authority.propose(value, current, 1).ok,
            "unsupported class remains excluded");
    }
}

}  // namespace

int main()
{
    lifecycleAndSequentialTransactions();
    mobilePerPlayerRackHistory();
    commitFailureIsAtomic();
    legacyConsumerFailsClosed();
    exclusions();
    std::cout << "nonphysical_general_foul_authority_tests: PASS\n";
}
