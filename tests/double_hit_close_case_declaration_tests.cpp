#include "rules/double_hit_close_case_declaration.h"

#include <cstdlib>
#include <iostream>

namespace {
using namespace billiardgl::rules;
class Auth final:public RefereeAuthorityAuthenticator{
public:bool verify(const RefereeAuthenticationInput& i)const override{
return i.credentialId=="credential"&&i.keyVersion=="v1"&&
i.authenticationProof==refereeAuthoritySha256Hex(i.authenticatedBytes);}};
void expect(bool v,const char*m){if(!v){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}}
void sign(RefereeAuthorityRequest&r){r.authenticationProof=
refereeAuthoritySha256Hex(canonicalRefereeAuthenticatedBytes(r));}
std::string proposal(){return "{\"close_case_attested\":true,\"cue_ball_id\":0,\"object_ball_id\":1,\"relation\":\"CUE_OBJECT_CLOSE_CASE_REQUIRES_POST_SHOT_REVIEW\"}";}
std::string final(){return "{\"close_case_attested\":true,\"cue_ball_id\":0,\"object_ball_id\":1,\"proposal_decision_id\":\"close.1\",\"relation\":\"CUE_OBJECT_CLOSE_CASE_REQUIRES_POST_SHOT_REVIEW\"}";}
RefereeAuthorityRequest request(const DoubleHitCloseCaseAuthority&a,
RefereeAuthorityOperation op,const std::string&p,const std::string&key){
RefereeAuthorityRequest r;r.protocolId=kRefereeAuthorityProtocolId;
r.schemaVersion=1;r.operation=op;r.declarationKind=kDoubleHitCloseCaseDeclarationKind;
r.decisionId="close.1";r.rackIdentity=a.state().rackIdentity;
r.expectedAuthorityRevision=a.protocolState().authorityRevision;
r.expectedAuthorityStateHash=a.protocolState().authorityStateHash;
r.authorityRole=RefereeAuthorityRole::Referee;r.authoritySubjectId="referee";
r.credentialId="credential";r.keyVersion="v1";r.authenticationNonce="nonce."+key;
r.expiresAtAuthorityTick=100;r.idempotencyKey=key;r.payloadCanonical=p;sign(r);return r;}
DoubleHitCloseCaseContext context(){return {0,1,5,"parent.sha"};}
void testLifecycle(){Auth auth;DoubleHitCloseCaseAuthority a(&auth);a.reset("rack.1");
auto c=context();auto p=request(a,RefereeAuthorityOperation::Propose,proposal(),"p");
expect(a.propose(p,c,1).ok&&!a.state().reviewNeeded,
"REFEREE_PROPOSE_CLOSE_CASE_DOES_NOT_ATTEST");
auto journal=a.protocolState().journal.size();expect(a.propose(p,c,1).ok&&
a.protocolState().journal.size()==journal,"EXACT_RETRY_ZERO_DELTA");
auto f=request(a,RefereeAuthorityOperation::FinalDeclare,final(),"f");
expect(a.finalDeclare(f,c,2).ok&&a.state().reviewNeeded&&a.state().committed,
"SAME_REFEREE_FINAL_ATTESTS_REVIEW_NEEDED_ONLY");
expect(a.consume("shot.1",3)&&a.state().phase==DoubleHitCloseCasePhase::Consumed,
"POSITIVE_ACCEPTED_SHOT_CONSUMES_EXACTLY_ONCE");
expect(!a.consume("shot.2",4),"SECOND_SHOT_CANNOT_REUSE");
expect(a.finalDeclare(f,c,4).ok&&a.state().phase==DoubleHitCloseCasePhase::Consumed,
"FINAL_RETRY_DOES_NOT_REACTIVATE");}
void testBadPayloadAndActor(){Auth auth;DoubleHitCloseCaseAuthority a(&auth);a.reset("rack.bad");auto c=context();
for(const auto&bad:{std::string("{\"double_hit\":true}"),std::string("{\"foul\":true}"),std::string("{\"tip_interval_count\":2}"),std::string("{\"distance_threshold\":0.1}"),std::string("{\"geometry_close\":true}")}){
auto r=request(a,RefereeAuthorityOperation::Propose,bad,"bad."+std::to_string(bad.size()));
expect(!a.propose(r,c,1).ok,"FORBIDDEN_FACT_OR_THRESHOLD_PAYLOAD_REJECTED");}
auto player=request(a,RefereeAuthorityOperation::Propose,proposal(),"player");
player.authorityRole=RefereeAuthorityRole::Player0;sign(player);
expect(!a.propose(player,c,1).ok&&a.state().phase==DoubleHitCloseCasePhase::Inactive,
"PLAYER_DECLARATION_REJECTED");}
void testCancel(){Auth auth;DoubleHitCloseCaseAuthority a(&auth);a.reset("rack.cancel");auto c=context();
auto p=request(a,RefereeAuthorityOperation::Propose,proposal(),"p");expect(a.propose(p,c,1).ok,"proposal");
auto cancel=request(a,RefereeAuthorityOperation::CancelProposal,"{\"proposal_decision_id\":\"close.1\"}","cancel");
expect(a.cancel(cancel,2).ok&&a.state().phase==DoubleHitCloseCasePhase::Cancelled,
"SAME_REFEREE_CANCEL_BEFORE_FINAL");}
}
int main(){testLifecycle();testBadPayloadAndActor();testCancel();
std::cout<<"double_hit_close_case_declaration_tests: PASS\n";}
