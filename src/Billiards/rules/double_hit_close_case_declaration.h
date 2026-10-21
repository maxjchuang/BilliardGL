#pragma once

#include "rules/referee_authority_protocol.h"

#include <cstdint>
#include <string>

namespace billiardgl { class GameRuntime; }

namespace billiardgl::rules {

inline constexpr const char* kDoubleHitCloseCaseDeclarationKind =
    "WPA_II_12_5_6_CLOSE_CASE_REFEREE_DECLARATION";

enum class DoubleHitCloseCasePhase {
    Inactive, ProposalOpen, Active, Consumed, Cancelled, Invalidated
};

struct DoubleHitCloseCaseContext {
    int cueBallId = 0;
    int objectBallId = -1;
    std::uint64_t parentRevision = 0;
    std::string parentStateHash;
};

struct DoubleHitCloseCaseState {
    DoubleHitCloseCasePhase phase = DoubleHitCloseCasePhase::Inactive;
    std::string rackIdentity;
    DoubleHitCloseCaseContext context;
    std::string proposalDecisionId;
    std::string refereeSubjectId;
    std::string proposalPayloadSha256;
    std::string finalPayloadSha256;
    bool committed = false;
    bool reviewNeeded = false;
    std::string consumedShotIdentity;
    std::uint64_t consumedAuthorityTick = 0;
};

struct DoubleHitCloseCaseResult {
    bool ok = false;
    std::string errorCode;
    RefereeAuthorityAck ack;
};

class DoubleHitCloseCaseAuthority {
public:
    DoubleHitCloseCaseAuthority(
        const RefereeAuthorityAuthenticator* authenticator,
        const RefereeAuthorityCommitGuard* commitGuard = nullptr);
    void reset(const std::string& rackIdentity);
    DoubleHitCloseCaseResult propose(const RefereeAuthorityRequest& request,
        const DoubleHitCloseCaseContext& context, std::uint64_t tick);
    DoubleHitCloseCaseResult finalDeclare(
        const RefereeAuthorityRequest& request,
        const DoubleHitCloseCaseContext& context, std::uint64_t tick);
    DoubleHitCloseCaseResult cancel(const RefereeAuthorityRequest& request,
        std::uint64_t tick);
    bool consume(const std::string& shotIdentity, std::uint64_t tick);
    void invalidate();
    const DoubleHitCloseCaseState& state() const { return state_; }
    const RefereeAuthorityState& protocolState() const
    {
        return protocol_.state();
    }
private:
    friend class ::billiardgl::GameRuntime;
    void restoreCommittedForReplay(const std::string& rackIdentity,
        const DoubleHitCloseCaseContext& context,
        const std::string& proposalDecisionId,
        const std::string& refereeSubjectId);
    RefereeAuthorityProtocol protocol_;
    DoubleHitCloseCaseState state_;
};

}  // namespace billiardgl::rules
