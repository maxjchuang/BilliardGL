#pragma once
#include "rules/referee_authority_protocol.h"
#include <cstdint>
#include <string>
namespace billiardgl { class GameRuntime; }
namespace billiardgl::rules {
inline constexpr const char* kFrozenCueObjectDeclarationKind="WPA_II_15_1_FROZEN_CUE_OBJECT_DECLARATION";
enum class FrozenBallDeclarationPhase {Inactive,ProposalOpen,Active,Consumed,Cancelled,Invalidated};
struct FrozenBallDeclarationContext {int cueBallId=0;int objectBallId=-1;std::uint64_t parentRevision=0;std::string parentStateHash;};
struct FrozenBallDeclarationState {FrozenBallDeclarationPhase phase=FrozenBallDeclarationPhase::Inactive;std::string rackIdentity;FrozenBallDeclarationContext context;std::string proposalDecisionId;std::string refereeSubjectId;std::string proposalPayloadSha256;std::string finalPayloadSha256;bool committed=false;std::string consumedShotIdentity;std::uint64_t consumedAuthorityTick=0;};
struct FrozenBallDeclarationResult {bool ok=false;std::string errorCode;RefereeAuthorityAck ack;};
class FrozenBallDeclarationAuthority {
public:
 FrozenBallDeclarationAuthority(const RefereeAuthorityAuthenticator*,
     const RefereeAuthorityCommitGuard* = nullptr);
 void reset(const std::string&);FrozenBallDeclarationResult propose(const RefereeAuthorityRequest&,const FrozenBallDeclarationContext&,std::uint64_t);FrozenBallDeclarationResult finalDeclare(const RefereeAuthorityRequest&,const FrozenBallDeclarationContext&,std::uint64_t);FrozenBallDeclarationResult cancel(const RefereeAuthorityRequest&,std::uint64_t);bool consume(const std::string&,std::uint64_t);void invalidate();
 const FrozenBallDeclarationState& state()const{return state_;}const RefereeAuthorityState& protocolState()const{return protocol_.state();}
private:
 friend class ::billiardgl::GameRuntime;
 void restoreCommittedForReplay(const std::string&,
     const FrozenBallDeclarationContext&,const std::string&,
     const std::string&);
 RefereeAuthorityProtocol protocol_;FrozenBallDeclarationState state_;
};
}
