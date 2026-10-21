#pragma once

#include "rules/referee_authority_protocol.h"

#include <cstdint>
#include <string>

namespace billiardgl { class GameRuntime; }

namespace billiardgl::rules {

inline constexpr const char* kFrozenCushionDeclarationKind =
    "WPA_II_15_2_FROZEN_OBJECT_STRAIGHT_CUSHION_DECLARATION";

enum class StraightCushionId { Head, Foot, LeftLong, RightLong };
enum class FrozenCushionDeclarationPhase {
    Inactive, ProposalOpen, Active, Consumed, Cancelled, Invalidated
};

const char* straightCushionIdName(StraightCushionId id);

struct FrozenCushionDeclarationContext {
    int objectBallId = -1;
    StraightCushionId cushionId = StraightCushionId::Head;
    std::uint64_t parentRevision = 0;
    std::string parentStateHash;
};

struct FrozenCushionDeclarationState {
    FrozenCushionDeclarationPhase phase =
        FrozenCushionDeclarationPhase::Inactive;
    std::string rackIdentity;
    FrozenCushionDeclarationContext context;
    std::string proposalDecisionId;
    std::string refereeSubjectId;
    std::string proposalPayloadSha256;
    std::string finalPayloadSha256;
    bool committed = false;
    std::string consumedShotIdentity;
    std::uint64_t consumedAuthorityTick = 0;
};

struct FrozenCushionDeclarationResult {
    bool ok = false;
    std::string errorCode;
    RefereeAuthorityAck ack;
};

class FrozenCushionDeclarationAuthority {
public:
    FrozenCushionDeclarationAuthority(
        const RefereeAuthorityAuthenticator* authenticator,
        const RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void reset(const std::string& rackIdentity);
    FrozenCushionDeclarationResult propose(
        const RefereeAuthorityRequest& request,
        const FrozenCushionDeclarationContext& context,
        std::uint64_t authorityTick);
    FrozenCushionDeclarationResult finalDeclare(
        const RefereeAuthorityRequest& request,
        const FrozenCushionDeclarationContext& context,
        std::uint64_t authorityTick);
    FrozenCushionDeclarationResult cancel(
        const RefereeAuthorityRequest& request,
        std::uint64_t authorityTick);
    bool consume(const std::string& shotIdentity,
                 std::uint64_t authorityTick);
    void invalidate();
    const FrozenCushionDeclarationState& state() const { return state_; }
    const RefereeAuthorityState& protocolState() const
    {
        return protocol_.state();
    }

private:
    friend class ::billiardgl::GameRuntime;
    void restoreCommittedForReplay(const std::string& rackIdentity,
        const FrozenCushionDeclarationContext& context,
        const std::string& proposalDecisionId,
        const std::string& refereeSubjectId);
    RefereeAuthorityProtocol protocol_;
    FrozenCushionDeclarationState state_;
};

}  // namespace billiardgl::rules
