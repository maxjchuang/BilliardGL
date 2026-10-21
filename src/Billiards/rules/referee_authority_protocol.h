#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace billiardgl::rules {

inline constexpr const char* kRefereeAuthorityProtocolId =
    "authenticated_referee_authority_v1";

enum class RefereeAuthorityRole {
    Referee,
    Player0,
    Player1,
};

enum class RefereeAuthorityOperation {
    Propose,
    FinalDeclare,
    CancelProposal,
};

enum class RefereeAuthorityPayloadType {
    Null,
    Bool,
    Number,
    String,
    ArrayTwoNumbers,
};

const char* refereeAuthorityRoleName(RefereeAuthorityRole role);
const char* refereeAuthorityOperationName(RefereeAuthorityOperation operation);

struct RefereeAuthorityConsumerPolicy {
    std::string declarationKind;
    std::vector<RefereeAuthorityRole> allowedActorRoles;
    std::map<std::string, RefereeAuthorityPayloadType>
        exactPayloadKeyTypes;
    std::map<std::string, RefereeAuthorityPayloadType>
        proposalPayloadKeyTypes;
    std::map<std::string, RefereeAuthorityPayloadType>
        finalPayloadKeyTypes;
    std::map<std::string, RefereeAuthorityPayloadType>
        cancelPayloadKeyTypes;
    bool finalPayloadMayDifferFromProposal = false;
    bool cancelPayloadMayDifferFromProposal = false;
    std::size_t maxPayloadBytes = 0;
    std::uint64_t maxLifetimeTicks = 0;
    bool proposalRequired = true;
    bool sameActorCancelRequired = true;
};

struct RefereeAuthorityRequest {
    std::string protocolId;
    int schemaVersion = 0;
    RefereeAuthorityOperation operation =
        RefereeAuthorityOperation::Propose;
    std::string declarationKind;
    std::string decisionId;
    std::string rackIdentity;
    std::uint64_t expectedAuthorityRevision = 0;
    std::string expectedAuthorityStateHash;
    RefereeAuthorityRole authorityRole = RefereeAuthorityRole::Referee;
    std::string authoritySubjectId;
    std::string credentialId;
    std::string keyVersion;
    std::string authenticationNonce;
    std::string authenticationProof;
    std::uint64_t expiresAtAuthorityTick = 0;
    std::string idempotencyKey;
    std::string payloadCanonical;
};

std::string canonicalRefereeAuthenticatedBytes(
    const RefereeAuthorityRequest& request);
std::string refereeAuthoritySha256Hex(const std::string& bytes);

struct RefereeAuthenticationInput {
    std::string credentialId;
    std::string keyVersion;
    std::string authenticationProof;
    std::string authenticatedBytes;
};

class RefereeAuthorityAuthenticator {
public:
    virtual ~RefereeAuthorityAuthenticator() = default;
    virtual bool verify(const RefereeAuthenticationInput& input) const = 0;
};

class RefereeAuthorityCommitGuard {
public:
    virtual ~RefereeAuthorityCommitGuard() = default;
    virtual bool allowCommit() const = 0;
};

struct RefereeAuthorityDeclarationRecord {
    RefereeAuthorityOperation operation =
        RefereeAuthorityOperation::Propose;
    std::string declarationKind;
    std::string decisionId;
    std::string rackIdentity;
    RefereeAuthorityRole authorityRole = RefereeAuthorityRole::Referee;
    std::string authoritySubjectId;
    std::string authenticationNonce;
    std::uint64_t expiresAtAuthorityTick = 0;
    std::string idempotencyKey;
    std::string payloadCanonical;
    std::string payloadSha256;
};

struct RefereeAuthorityAuditEntry {
    std::uint64_t sequence = 0;
    RefereeAuthorityOperation operation =
        RefereeAuthorityOperation::Propose;
    std::string declarationKind;
    std::string decisionId;
    std::string rackIdentity;
    RefereeAuthorityRole authorityRole = RefereeAuthorityRole::Referee;
    std::string authoritySubjectId;
    std::uint64_t previousAuthorityRevision = 0;
    std::uint64_t authoritativeRevision = 0;
    std::string payloadSha256;
    bool proposalPending = false;
    bool finalDeclarationCommitted = false;
};

struct RefereeAuthorityAck {
    bool accepted = false;
    RefereeAuthorityOperation operation =
        RefereeAuthorityOperation::Propose;
    std::string declarationKind;
    std::string decisionId;
    std::string proposalDecisionId;
    RefereeAuthorityRole authorityRole = RefereeAuthorityRole::Referee;
    std::string authoritySubjectId;
    std::string credentialId;
    std::string keyVersion;
    std::string authenticationNonce;
    std::string idempotencyKey;
    std::uint64_t previousAuthorityRevision = 0;
    std::uint64_t authoritativeRevision = 0;
    std::string authoritativeStateHash;
    std::uint64_t expiresAtAuthorityTick = 0;
    bool proposalPending = false;
    bool finalDeclarationCommitted = false;
    std::string payloadSha256;
};

struct RefereeAuthorityResult {
    bool ok = false;
    std::string errorCode;
    RefereeAuthorityAck ack;
};

struct RefereeAuthorityState {
    std::string protocolId = kRefereeAuthorityProtocolId;
    std::string rackIdentity;
    std::uint64_t resetGeneration = 0;
    std::uint64_t authorityRevision = 0;
    std::string authorityStateHash;
    std::optional<RefereeAuthorityDeclarationRecord> liveProposal;
    std::optional<RefereeAuthorityDeclarationRecord>
        latestFinalDeclaration;
    std::vector<RefereeAuthorityAuditEntry> journal;
    std::string auditDigest;
};

class RefereeAuthorityProtocol {
public:
    RefereeAuthorityProtocol();
    RefereeAuthorityProtocol(
        std::vector<RefereeAuthorityConsumerPolicy> policies,
        const RefereeAuthorityAuthenticator* authenticator,
        const RefereeAuthorityCommitGuard* commitGuard = nullptr);

    void reset(const std::string& rackIdentity);
    // Returns the original acknowledgement only when every authenticated
    // request byte and proof matches a receipt in the current rack.  This is
    // intentionally checked before live-state validation by consumers that
    // support more than one transaction per rack.  A reused idempotency key
    // with different bytes remains a conflict, not a new request.
    RefereeAuthorityResult exactReceipt(
        const RefereeAuthorityRequest& request) const;
    RefereeAuthorityResult submit(const RefereeAuthorityRequest& request,
                                  std::uint64_t currentAuthorityTick);
    RefereeAuthorityResult verifyDetached(
        const RefereeAuthorityRequest& request,
        std::uint64_t currentAuthorityTick) const;
    const RefereeAuthorityState& state() const { return state_; }

private:
    struct Receipt {
        std::string authenticatedBytes;
        std::string authenticationProofSha256;
        RefereeAuthorityAck ack;
    };

    const RefereeAuthorityConsumerPolicy* policy(
        const std::string& declarationKind) const;
    std::string stateHash(const RefereeAuthorityState& state) const;

    std::vector<RefereeAuthorityConsumerPolicy> policies_;
    const RefereeAuthorityAuthenticator* authenticator_ = nullptr;
    const RefereeAuthorityCommitGuard* commitGuard_ = nullptr;
    RefereeAuthorityState state_;
    std::map<std::string, Receipt> receipts_;
    std::map<std::string, std::string> acceptedNonces_;
    std::uint64_t resetGeneration_ = 0;
};

}  // namespace billiardgl::rules
