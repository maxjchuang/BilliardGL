#pragma once
#include "rules/referee_authority_protocol.h"
#include <cstdint>
#include <string>
namespace billiardgl { class GameRuntime; }
namespace billiardgl::rules {
inline constexpr const char* kPushReviewDeclarationKind=
    "WPA_II_12_7_PUSH_REVIEW_REFEREE_DECLARATION";
enum class PushReviewPhase{Inactive,ProposalOpen,Active,Consumed,Cancelled,Invalidated};
struct PushReviewContext{int cueBallId=0;std::uint64_t parentRevision=0;std::string parentStateHash;};
struct PushReviewState{PushReviewPhase phase=PushReviewPhase::Inactive;std::string rackIdentity;PushReviewContext context;std::string proposalDecisionId;std::string refereeSubjectId;std::string proposalPayloadSha256;std::string finalPayloadSha256;bool committed=false;bool reviewNeeded=false;std::string consumedShotIdentity;std::uint64_t consumedAuthorityTick=0;};
struct PushReviewResult{bool ok=false;std::string errorCode;RefereeAuthorityAck ack;};
class PushReviewAuthority{
public:PushReviewAuthority(const RefereeAuthorityAuthenticator*,const RefereeAuthorityCommitGuard* = nullptr);void reset(const std::string&);PushReviewResult propose(const RefereeAuthorityRequest&,const PushReviewContext&,std::uint64_t);PushReviewResult finalDeclare(const RefereeAuthorityRequest&,const PushReviewContext&,std::uint64_t);PushReviewResult cancel(const RefereeAuthorityRequest&,std::uint64_t);bool consume(const std::string&,std::uint64_t);void invalidate();const PushReviewState&state()const{return state_;}const RefereeAuthorityState&protocolState()const{return protocol_.state();}
private:
 friend class ::billiardgl::GameRuntime;
 void restoreCommittedForReplay(const std::string&,const PushReviewContext&,
     const std::string&,const std::string&);
 RefereeAuthorityProtocol protocol_;PushReviewState state_;};
}
