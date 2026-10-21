#pragma once

#include "rules/referee_authority_protocol.h"

#include <array>
#include <cstdint>
#include <string>

namespace billiardgl::rules {

inline constexpr const char* kGroupConfusionDeclarationKind =
    "WPA_II_19_GROUP_CONFUSION_DECLARATION";

enum class GroupConfusionPhase {
    Inactive,
    TimelyWindowOpen,
    RealizationProposed,
    RerackCommitting,
    RerackCommitted,
    WindowExpired,
};

struct GroupConfusionWindow {
    std::string rackIdentity;
    int originalBreakerPlayerId = -1;
    std::array<int, 2> originalShootingOrder{{0, 1}};
    std::string assignmentSnapshotSha256;
    std::string completedShotIdentity;
    std::uint64_t completedShotOrdinal = 0;
    int completedShooter = -1;
    std::uint64_t completedRevision = 0;
    std::string completedStateHash;
};

struct GroupConfusionState {
    GroupConfusionPhase phase = GroupConfusionPhase::Inactive;
    GroupConfusionWindow window;
    std::uint64_t windowGeneration = 0;
    std::string rackOrderSnapshotSha256;
    std::string proposalDecisionId;
    RefereeAuthorityRole proposalActorRole =
        RefereeAuthorityRole::Referee;
    std::string proposalActorSubjectId;
    std::string proposalPayloadSha256;
    bool finalAuthorized = false;
    bool rerackCommitted = false;
};

struct GroupConfusionResult {
    bool ok = false;
    std::string errorCode;
    RefereeAuthorityAck ack;
};

class GroupConfusionAuthority {
public:
    GroupConfusionAuthority(const RefereeAuthorityAuthenticator*,
        const RefereeAuthorityCommitGuard* = nullptr);

    void reset(const std::string&, int, const std::array<int, 2>&);
    bool openWindow(const GroupConfusionWindow&);
    void expireWindowOnAcceptedShoot();
    GroupConfusionResult propose(
        const RefereeAuthorityRequest&, std::uint64_t);
    GroupConfusionResult finalDeclare(
        const RefereeAuthorityRequest&, std::uint64_t);
    GroupConfusionResult cancel(
        const RefereeAuthorityRequest&, std::uint64_t) const;
    void markRerackCommitting();
    void markRerackCommitted();

    bool rackStopped() const
    {
        return state_.phase == GroupConfusionPhase::RealizationProposed ||
            state_.phase == GroupConfusionPhase::RerackCommitting;
    }
    const GroupConfusionState& state() const { return state_; }
    const RefereeAuthorityState& protocolState() const
    {
        return protocol_.state();
    }

private:
    RefereeAuthorityProtocol protocol_;
    GroupConfusionState state_;
    std::string resetRackIdentity_;
    int resetOriginalBreaker_ = -1;
    std::array<int, 2> resetOriginalOrder_{{0, 1}};
};

}  // namespace billiardgl::rules
