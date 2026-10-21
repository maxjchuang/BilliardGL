#include "rules/unintentional_break_miscue_declaration.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace billiardgl::rules;

class Authenticator final : public RefereeAuthorityAuthenticator {
public:
    bool verify(const RefereeAuthenticationInput& input) const override
    {
        return input.authenticationProof ==
            refereeAuthoritySha256Hex(input.authenticatedBytes);
    }
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

RefereeAuthorityRequest request(
    const UnintentionalBreakMiscueDeclarationAuthority& authority,
    RefereeAuthorityOperation operation, const std::string& payload,
    const std::string& key)
{
    RefereeAuthorityRequest result;
    result.protocolId = kRefereeAuthorityProtocolId;
    result.schemaVersion = 1;
    result.operation = operation;
    result.declarationKind = kUnintentionalBreakMiscueDeclarationKind;
    result.decisionId = "rules.unintentional_break_miscue.1";
    result.rackIdentity = authority.state().rackIdentity;
    result.expectedAuthorityRevision =
        authority.protocolState().authorityRevision;
    result.expectedAuthorityStateHash =
        authority.protocolState().authorityStateHash;
    result.authorityRole = RefereeAuthorityRole::Referee;
    result.authoritySubjectId = "referee.06g";
    result.credentialId = "credential.06g";
    result.keyVersion = "v1";
    result.authenticationNonce = "nonce." + key;
    result.expiresAtAuthorityTick = 1000;
    result.idempotencyKey = key;
    result.payloadCanonical = payload;
    sign(result);
    return result;
}

void testCommittedDeclarationIsShotBoundAndSingleUse()
{
    Authenticator authenticator;
    UnintentionalBreakMiscueDeclarationAuthority authority(&authenticator);
    authority.reset("rack.06g.1");
    UnintentionalBreakMiscueDeclarationContext context;
    context.actorPlayerId = 0;
    context.parentRevision = 17;
    context.parentStateHash = std::string(64, 'a');

    const std::string proposal =
        "{\"actor_player_id\":0,\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
    auto proposed = request(authority, RefereeAuthorityOperation::Propose,
        proposal, "proposal");
    expect(authority.propose(proposed, context, 1).ok,
        "signed 06G proposal is accepted");
    expect(authority.state().phase ==
            UnintentionalBreakMiscueDeclarationPhase::ProposalOpen &&
        !authority.state().committed,
        "proposal alone cannot classify the break");

    const std::string final =
        "{\"actor_player_id\":0,\"proposal_decision_id\":\"rules.unintentional_break_miscue.1\",\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
    auto committed = request(authority,
        RefereeAuthorityOperation::FinalDeclare, final, "final");
    const auto revisionBefore = authority.protocolState().authorityRevision;
    const auto hashBefore = authority.protocolState().authorityStateHash;
    const auto rejectedWithoutMutation = [&](const auto& candidate,
                                             const auto& candidateContext,
                                             std::uint64_t tick) {
        expect(!authority.finalDeclare(candidate, candidateContext, tick).ok,
            "invalid final declaration is rejected");
        expect(authority.state().phase ==
                UnintentionalBreakMiscueDeclarationPhase::ProposalOpen &&
            !authority.state().committed &&
            authority.protocolState().authorityRevision == revisionBefore &&
            authority.protocolState().authorityStateHash == hashBefore,
            "rejected final preserves consumer and protocol authority");
    };
    auto changedContext = context;
    ++changedContext.parentRevision;
    rejectedWithoutMutation(committed, changedContext, 2);
    changedContext = context;
    changedContext.parentStateHash = std::string(64, 'b');
    rejectedWithoutMutation(committed, changedContext, 2);
    changedContext = context;
    changedContext.actorPlayerId = 1;
    rejectedWithoutMutation(committed, changedContext, 2);
    auto invalidFinal = committed;
    invalidFinal.authenticationProof.clear();
    rejectedWithoutMutation(invalidFinal, context, 2);
    invalidFinal = committed;
    invalidFinal.authoritySubjectId = "referee.other";
    sign(invalidFinal);
    rejectedWithoutMutation(invalidFinal, context, 2);
    invalidFinal = committed;
    invalidFinal.rackIdentity = "rack.other";
    sign(invalidFinal);
    rejectedWithoutMutation(invalidFinal, context, 2);
    invalidFinal = committed;
    ++invalidFinal.expectedAuthorityRevision;
    sign(invalidFinal);
    rejectedWithoutMutation(invalidFinal, context, 2);
    rejectedWithoutMutation(committed, context, 1001);
    expect(authority.finalDeclare(committed, context, 2).ok &&
        authority.state().phase ==
            UnintentionalBreakMiscueDeclarationPhase::Active &&
        authority.state().committed,
        "final signed declaration activates exact shot binding");
    const auto finalRevision = authority.protocolState().authorityRevision;
    const auto finalHash = authority.protocolState().authorityStateHash;
    expect(authority.finalDeclare(committed, context, 2).ok &&
        authority.protocolState().authorityRevision == finalRevision &&
        authority.protocolState().authorityStateHash == finalHash,
        "exact final retry is idempotent");
    expect(authority.consume("runtime.stroke.17", 3,
            std::string(64, 'a')) &&
        authority.state().phase ==
            UnintentionalBreakMiscueDeclarationPhase::Consumed &&
        authority.state().consumedStrokeBindingSha256 ==
            std::string(64, 'a') &&
        !authority.consume("runtime.stroke.18", 4,
            std::string(64, 'b')) &&
        authority.state().consumedStrokeBindingSha256 ==
            std::string(64, 'a'),
        "declaration is consumed by exactly one shot and retains that stroke binding");
    expect(authority.finalDeclare(committed, context, 4).ok &&
        authority.state().phase ==
            UnintentionalBreakMiscueDeclarationPhase::Consumed &&
        authority.state().consumedShotIdentity == "runtime.stroke.17" &&
        authority.protocolState().authorityRevision == finalRevision &&
        !authority.consume("runtime.stroke.19", 5),
        "exact final retry after consumption cannot rearm permission");
}

void testBadActorPayloadAndUnsignedRequestsFailClosed()
{
    Authenticator authenticator;
    UnintentionalBreakMiscueDeclarationAuthority authority(&authenticator);
    authority.reset("rack.06g.2");
    UnintentionalBreakMiscueDeclarationContext context;
    context.actorPlayerId = 1;
    context.parentRevision = 5;
    context.parentStateHash = std::string(64, 'b');

    auto wrongActor = request(authority,
        RefereeAuthorityOperation::Propose,
        "{\"actor_player_id\":0,\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}",
        "wrong-actor");
    expect(!authority.propose(wrongActor, context, 1).ok &&
        authority.state().phase ==
            UnintentionalBreakMiscueDeclarationPhase::Inactive,
        "actor-mismatched declaration has zero mutation");

    auto unsignedRequest = request(authority,
        RefereeAuthorityOperation::Propose,
        "{\"actor_player_id\":1,\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}",
        "unsigned");
    unsignedRequest.authenticationProof.clear();
    expect(!authority.propose(unsignedRequest, context, 1).ok &&
        authority.state().phase ==
            UnintentionalBreakMiscueDeclarationPhase::Inactive,
        "unsigned declaration has zero mutation");
}

}  // namespace

int main()
{
    testCommittedDeclarationIsShotBoundAndSingleUse();
    testBadActorPayloadAndUnsignedRequestsFailClosed();
    std::cout << "unintentional_break_miscue_declaration_tests: PASS\n";
    return 0;
}
