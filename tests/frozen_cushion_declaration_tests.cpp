#include "rules/frozen_cushion_declaration.h"

#include <cstdlib>
#include <iostream>

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
std::string proposal(const std::string& cushion = "HEAD")
{
    return "{\"frozen_attested\":true,\"object_ball_id\":1,"
        "\"relation\":\"OBJECT_BALL_FROZEN_TO_STRAIGHT_CUSHION\","
        "\"straight_cushion_id\":\"" + cushion + "\"}";
}
std::string final()
{
    return "{\"frozen_attested\":true,\"object_ball_id\":1,"
        "\"proposal_decision_id\":\"cushion.1\","
        "\"relation\":\"OBJECT_BALL_FROZEN_TO_STRAIGHT_CUSHION\","
        "\"straight_cushion_id\":\"HEAD\"}";
}
RefereeAuthorityRequest request(const FrozenCushionDeclarationAuthority& a,
    RefereeAuthorityOperation operation, const std::string& payload,
    const std::string& key)
{
    RefereeAuthorityRequest value;
    value.protocolId = kRefereeAuthorityProtocolId;
    value.schemaVersion = 1;
    value.operation = operation;
    value.declarationKind = kFrozenCushionDeclarationKind;
    value.decisionId = "cushion.1";
    value.rackIdentity = a.state().rackIdentity;
    value.expectedAuthorityRevision = a.protocolState().authorityRevision;
    value.expectedAuthorityStateHash = a.protocolState().authorityStateHash;
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

void testLifecycleAndRetry()
{
    Auth auth;
    FrozenCushionDeclarationAuthority authority(&auth);
    authority.reset("rack.1");
    FrozenCushionDeclarationContext context{
        1, StraightCushionId::Head, 7, "parent.sha"};
    auto proposed = request(authority, RefereeAuthorityOperation::Propose,
        proposal(), "proposal");
    expect(authority.propose(proposed, context, 1).ok &&
        !authority.state().committed,
        "REFEREE_PROPOSE_EXACT_OBJECT_CUSHION_DOES_NOT_DECLARE");
    const auto proposalJournal = authority.protocolState().journal.size();
    expect(authority.propose(proposed, context, 1).ok &&
        authority.protocolState().journal.size() == proposalJournal,
        "PROPOSAL_EXACT_RETRY_ZERO_DELTA");
    auto finalized = request(authority,
        RefereeAuthorityOperation::FinalDeclare, final(), "final");
    expect(authority.finalDeclare(finalized, context, 2).ok &&
        authority.state().committed &&
        authority.state().phase == FrozenCushionDeclarationPhase::Active,
        "SAME_REFEREE_FINAL_COMMITS_PRE_SHOT_DECLARATION");
    expect(authority.consume("shot.1", 3) &&
        authority.state().phase == FrozenCushionDeclarationPhase::Consumed,
        "POSITIVE_ACCEPTED_SHOT_CONSUMES_EXACTLY_ONCE");
    expect(!authority.consume("shot.2", 4),
        "SECOND_SHOT_CANNOT_REUSE");
    expect(authority.finalDeclare(finalized, context, 4).ok &&
        authority.state().phase == FrozenCushionDeclarationPhase::Consumed,
        "FINAL_RETRY_DOES_NOT_REACTIVATE_CONSUMED_DECLARATION");
}

void testFourStraightCushionsAndBadBoundary()
{
    for (const auto pair : {std::pair{StraightCushionId::Head,"HEAD"},
             std::pair{StraightCushionId::Foot,"FOOT"},
             std::pair{StraightCushionId::LeftLong,"LEFT_LONG"},
             std::pair{StraightCushionId::RightLong,"RIGHT_LONG"}}) {
        Auth auth;
        FrozenCushionDeclarationAuthority authority(&auth);
        authority.reset("rack.four");
        FrozenCushionDeclarationContext context{1,pair.first,1,"state"};
        auto value = request(authority,RefereeAuthorityOperation::Propose,
            proposal(pair.second),"four."+std::string(pair.second));
        expect(authority.propose(value,context,1).ok,
            "FOUR_STRAIGHT_CUSHION_IDS_EXACT");
    }
    Auth auth;
    FrozenCushionDeclarationAuthority authority(&auth);
    authority.reset("rack.bad");
    FrozenCushionDeclarationContext context{1,StraightCushionId::Head,1,"s"};
    auto jaw = request(authority,RefereeAuthorityOperation::Propose,
        proposal("POCKET_JAW"),"jaw");
    expect(!authority.propose(jaw,context,1).ok &&
        authority.state().phase==FrozenCushionDeclarationPhase::Inactive,
        "JAW_OR_POCKET_BOUNDARY_ID_REJECTED");
    auto geometry = request(authority,RefereeAuthorityOperation::Propose,
        "{\"geometry_frozen\":true}","geometry");
    expect(!authority.propose(geometry,context,1).ok,
        "GEOMETRY_OR_CONTACT_TELEMETRY_CANNOT_DECLARE");
}

void testActorCancelAndInvalidation()
{
    Auth auth;
    FrozenCushionDeclarationAuthority authority(&auth);
    authority.reset("rack.cancel");
    FrozenCushionDeclarationContext context{1,StraightCushionId::Head,1,"s"};
    auto player = request(authority,RefereeAuthorityOperation::Propose,
        proposal(),"player");
    player.authorityRole=RefereeAuthorityRole::Player0;sign(player);
    expect(!authority.propose(player,context,1).ok,
        "PLAYER_DECLARATION_REJECTED");
    auto proposed=request(authority,RefereeAuthorityOperation::Propose,
        proposal(),"proposal");
    expect(authority.propose(proposed,context,1).ok,
        "CANCEL_FIXTURE_PROPOSAL");
    auto cancel=request(authority,RefereeAuthorityOperation::CancelProposal,
        "{\"proposal_decision_id\":\"cushion.1\"}","cancel");
    expect(authority.cancel(cancel,2).ok&&
        authority.state().phase==FrozenCushionDeclarationPhase::Cancelled,
        "SAME_REFEREE_CANCEL_BEFORE_FINAL");
    authority.reset("rack.invalidate");
    expect(authority.state().phase==FrozenCushionDeclarationPhase::Inactive,
        "RESET_INVALIDATES_DECLARATION");
}
}

int main()
{
    testLifecycleAndRetry();
    testFourStraightCushionsAndBadBoundary();
    testActorCancelAndInvalidation();
    std::cout << "frozen_cushion_declaration_tests: PASS\n";
}
