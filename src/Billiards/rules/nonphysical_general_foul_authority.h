#pragma once

#include "rules/referee_authority_protocol.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>

namespace billiardgl::rules {

inline constexpr const char* kNonphysicalGeneralFoulKind =
    "WPA_II_17_NONPHYSICAL_GENERAL_FOUL_REFEREE_EVENT";

enum class NonphysicalGeneralFoulKind {
    BothFeetOffGroundDuringShot,
    IllegalBodyClothingOrEquipmentBallTouch,
    OneHandShot,
    AuxiliaryEquipmentPlacedOnTableToShoot,
    MobileDeviceDisturbanceGeneralFoul,
    MobileDeviceRepeatLossOfRack,
};

const char* nonphysicalGeneralFoulKindName(NonphysicalGeneralFoulKind kind);
std::string canonicalNonphysicalGeneralFoulProposal(
    NonphysicalGeneralFoulKind kind, int offendingPlayer,
    const std::string& shotIdentity);
std::string canonicalNonphysicalGeneralFoulFinal(
    NonphysicalGeneralFoulKind kind, int offendingPlayer,
    const std::string& proposalDecisionId, const std::string& shotIdentity);

enum class NonphysicalGeneralFoulPhase {
    Inactive, ProposalOpen, Committed, Cancelled, Invalidated
};

// Rack-scoped and player-scoped.  The saturating enum avoids count overflow
// and prevents a third signed event from silently becoming another penalty.
enum class MobileDeviceViolationState {
    None,
    GeneralFoulCommitted,
    RackLossCommitted,
};

// This authority classifies the signed fact only.  LossOfRack is deliberately
// not represented as a match loss; GameRuntime/product/replay integration is
// outside this authority-only slice.
enum class NonphysicalGeneralFoulOutcome {
    None,
    GeneralFoulBallInHand,
    LossOfRack,
};

struct NonphysicalGeneralFoulContext {
    int offendingPlayer = -1;
    int opponentPlayer = -1;
    std::string shotIdentity;
    std::uint64_t parentRevision = 0;
    std::string parentStateHash;
};

struct NonphysicalGeneralFoulState {
    NonphysicalGeneralFoulPhase phase =
        NonphysicalGeneralFoulPhase::Inactive;
    std::string rackIdentity;
    NonphysicalGeneralFoulKind eventKind =
        NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot;
    NonphysicalGeneralFoulContext context;
    std::string proposalDecisionId;
    std::string refereeSubjectId;
    std::string proposalPayloadSha256;
    std::string finalPayloadSha256;
    bool committed = false;
    std::array<MobileDeviceViolationState, 2> mobileDeviceViolations{{
        MobileDeviceViolationState::None,
        MobileDeviceViolationState::None}};
};

struct NonphysicalGeneralFoulResult {
    bool ok = false;
    std::string errorCode;
    RefereeAuthorityAck ack;
    NonphysicalGeneralFoulOutcome outcome =
        NonphysicalGeneralFoulOutcome::None;
    bool newlyCommitted = false;
    bool idempotentReplay = false;
};

struct NonphysicalGeneralFoulConsumerCapabilities {
    // A consumer must understand outcome and idempotentReplay before enabling
    // sequential rack transactions.  Legacy consumers otherwise risk
    // applying an old exact receipt or LossOfRack as another ordinary foul.
    bool structuredRackOutcomes = false;
};

class NonphysicalGeneralFoulAuthority {
public:
    NonphysicalGeneralFoulAuthority(
        const RefereeAuthorityAuthenticator* authenticator,
        const RefereeAuthorityCommitGuard* commitGuard = nullptr,
        NonphysicalGeneralFoulConsumerCapabilities capabilities = {});
    void reset(const std::string& rackIdentity);
    NonphysicalGeneralFoulResult propose(const RefereeAuthorityRequest& request,
        const NonphysicalGeneralFoulContext& context, std::uint64_t tick);
    NonphysicalGeneralFoulResult finalDeclare(
        const RefereeAuthorityRequest& request,
        const NonphysicalGeneralFoulContext& context, std::uint64_t tick);
    NonphysicalGeneralFoulResult cancel(
        const RefereeAuthorityRequest& request, std::uint64_t tick);
    NonphysicalGeneralFoulResult exactReceipt(
        const RefereeAuthorityRequest& request) const;
    void invalidate();
    const NonphysicalGeneralFoulState& state() const { return state_; }
    const RefereeAuthorityState& protocolState() const
    {
        return protocol_.state();
    }
private:
    RefereeAuthorityProtocol protocol_;
    NonphysicalGeneralFoulState state_;
    NonphysicalGeneralFoulConsumerCapabilities capabilities_;
    std::map<std::string, NonphysicalGeneralFoulOutcome>
        receiptOutcomes_;
};

}  // namespace billiardgl::rules
