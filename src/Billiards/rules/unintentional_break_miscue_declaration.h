#pragma once

#include "rules/referee_authority_protocol.h"

#include <cstdint>
#include <string>

namespace billiardgl::rules {

inline constexpr const char* kUnintentionalBreakMiscueDeclarationKind =
    "WPA_II_06_G_UNINTENTIONAL_BREAK_MISCUE_DECLARATION";

enum class UnintentionalBreakMiscueDeclarationPhase {
    Inactive,
    ProposalOpen,
    Active,
    Consumed,
    Cancelled,
    Invalidated,
};

struct UnintentionalBreakMiscueDeclarationContext {
    int actorPlayerId = -1;
    std::uint64_t parentRevision = 0;
    std::string parentStateHash;
};

struct UnintentionalBreakMiscueDeclarationState {
    UnintentionalBreakMiscueDeclarationPhase phase =
        UnintentionalBreakMiscueDeclarationPhase::Inactive;
    std::string rackIdentity;
    UnintentionalBreakMiscueDeclarationContext context;
    std::string proposalDecisionId;
    std::string refereeSubjectId;
    std::string proposalPayloadSha256;
    std::string finalPayloadSha256;
    bool committed = false;
    std::string consumedShotIdentity;
    std::string consumedStrokeBindingSha256;
    std::uint64_t consumedAuthorityTick = 0;
};

struct UnintentionalBreakMiscueDeclarationResult {
    bool ok = false;
    std::string errorCode;
    RefereeAuthorityAck ack;
};

class UnintentionalBreakMiscueDeclarationAuthority {
public:
    UnintentionalBreakMiscueDeclarationAuthority(
        const RefereeAuthorityAuthenticator* authenticator,
        const RefereeAuthorityCommitGuard* commitGuard = nullptr);

    void reset(const std::string& rackIdentity);
    UnintentionalBreakMiscueDeclarationResult propose(
        const RefereeAuthorityRequest& request,
        const UnintentionalBreakMiscueDeclarationContext& context,
        std::uint64_t tick);
    UnintentionalBreakMiscueDeclarationResult finalDeclare(
        const RefereeAuthorityRequest& request,
        const UnintentionalBreakMiscueDeclarationContext& context,
        std::uint64_t tick);
    UnintentionalBreakMiscueDeclarationResult cancel(
        const RefereeAuthorityRequest& request, std::uint64_t tick);
    bool consume(const std::string& shotIdentity, std::uint64_t tick,
        const std::string& strokeBindingSha256 = {});
    void invalidate();
    void restoreCommittedForReplay(const std::string& rackIdentity,
        const UnintentionalBreakMiscueDeclarationContext& context,
        const std::string& proposalDecisionId,
        const std::string& refereeSubjectId);

    const UnintentionalBreakMiscueDeclarationState& state() const
    {
        return state_;
    }
    const RefereeAuthorityState& protocolState() const
    {
        return protocol_.state();
    }

private:
    RefereeAuthorityProtocol protocol_;
    UnintentionalBreakMiscueDeclarationState state_;
};

}  // namespace billiardgl::rules
