#pragma once

#include "rules/referee_authority_protocol.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace billiardgl::rules {

inline constexpr const char* kStalemateDirectiveKind =
    "WPA_II_21_STALEMATE_DIRECTIVE";
inline constexpr const char* kStalemateRefusalKind =
    "WPA_II_21_STALEMATE_REFUSAL";

enum class StalematePhase {
    Inactive,
    ProposalOpen,
    ContinuationActive,
    AwaitingRefereeFinal,
    RerackCommitted,
    Cancelled
};

struct StalemateState {
    StalematePhase phase = StalematePhase::Inactive;
    std::string rackIdentity;
    int originalBreakerPlayerId = -1;
    std::array<int, 2> originalShootingOrder{{0, 1}};
    std::string orderSnapshotSha256;
    std::string proposalDecisionId;
    std::string proposalPayloadSha256;
    std::string refereeSubjectId;
    std::string directiveKind;
    int perPlayerShotAllowance = 0;
    std::uint64_t windowStartAuthorityRevision = 0;
    std::array<int, 2> completedShots{{0, 0}};
    std::array<std::string, 2> lastCountedShotIdentity;
    std::string counterSnapshotSha256;
    std::vector<int> refusalHistory;
    std::vector<std::string> refusalAuditDigests;
    bool finalAuthorized = false;
    bool rerackCommitted = false;
};

struct StalemateResult {
    bool ok = false;
    std::string errorCode;
    RefereeAuthorityAck ack;
};

class StalemateAuthority {
public:
    StalemateAuthority(const RefereeAuthorityAuthenticator*,
        const RefereeAuthorityCommitGuard* = nullptr);

    void reset(const std::string&, int, const std::array<int, 2>&);
    StalemateResult propose(const RefereeAuthorityRequest&, std::uint64_t);
    StalemateResult refuse(const RefereeAuthorityRequest&, std::uint64_t);
    StalemateResult finalDeclare(
        const RefereeAuthorityRequest&, std::uint64_t);
    StalemateResult cancel(const RefereeAuthorityRequest&, std::uint64_t);
    bool recordCompletedShot(
        int, const std::string&, const std::string&);
    void markRerackCommitted();
    void forceResetState();

    const StalemateState& state() const { return state_; }
    const RefereeAuthorityState& protocolState() const
    {
        return protocol_.state();
    }

private:
    struct Receipt {
        std::string authenticatedBytes;
        std::string authenticationProof;
        StalemateResult result;
    };

    StalemateResult retryOrConflict(
        const std::map<std::string, Receipt>&,
        const RefereeAuthorityRequest&) const;
    void updateCounterDigest();

    RefereeAuthorityProtocol protocol_;
    const RefereeAuthorityCommitGuard* guard_ = nullptr;
    StalemateState state_;
    std::set<std::string> countedShots_;
    std::map<std::string, Receipt> refusalReceipts_;
    std::map<std::string, Receipt> finalReceipts_;
    std::map<std::string, Receipt> cancelReceipts_;
    std::map<std::string, std::string> detachedNonces_;
};

}  // namespace billiardgl::rules
