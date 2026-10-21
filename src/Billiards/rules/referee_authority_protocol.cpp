#include "rules/referee_authority_protocol.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace billiardgl::rules {
namespace {

constexpr std::array<std::uint32_t, 64> kShaRounds = {{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
    0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
    0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
    0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2}};

std::uint32_t rotateRight(std::uint32_t value, int count)
{
    return (value >> count) | (value << (32 - count));
}

std::string protocolSha256Hex(const std::string& bytes)
{
    std::vector<std::uint8_t> message(bytes.begin(), bytes.end());
    const std::uint64_t bitLength =
        static_cast<std::uint64_t>(message.size()) * 8;
    message.push_back(0x80);
    while (message.size() % 64 != 56) message.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8)
        message.push_back(static_cast<std::uint8_t>(bitLength >> shift));
    std::array<std::uint32_t, 8> state = {{
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
    for (std::size_t offset = 0; offset < message.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (int index = 0; index < 16; ++index) {
            const std::size_t base = offset +
                static_cast<std::size_t>(index) * 4;
            words[index] =
                (static_cast<std::uint32_t>(message[base]) << 24) |
                (static_cast<std::uint32_t>(message[base + 1]) << 16) |
                (static_cast<std::uint32_t>(message[base + 2]) << 8) |
                message[base + 3];
        }
        for (int index = 16; index < 64; ++index) {
            const std::uint32_t s0 = rotateRight(words[index - 15], 7) ^
                rotateRight(words[index - 15], 18) ^
                (words[index - 15] >> 3);
            const std::uint32_t s1 = rotateRight(words[index - 2], 17) ^
                rotateRight(words[index - 2], 19) ^
                (words[index - 2] >> 10);
            words[index] = words[index - 16] + s0 +
                words[index - 7] + s1;
        }
        std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (int index = 0; index < 64; ++index) {
            const std::uint32_t sum1 = rotateRight(e, 6) ^
                rotateRight(e, 11) ^ rotateRight(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t temporary1 = h + sum1 + choice +
                kShaRounds[index] + words[index];
            const std::uint32_t sum0 = rotateRight(a, 2) ^
                rotateRight(a, 13) ^ rotateRight(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temporary2 = sum0 + majority;
            h = g; g = f; f = e; e = d + temporary1;
            d = c; c = b; b = a; a = temporary1 + temporary2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::uint32_t word : state) output << std::setw(8) << word;
    return output.str();
}

void appendField(std::ostringstream& output, const std::string& value)
{
    output << value.size() << ':' << value << '|';
}

bool boundedIdentity(const std::string& value, std::size_t maximum = 128)
{
    return !value.empty() && value.size() <= maximum;
}

bool parseJsonString(const std::string& text, std::size_t& cursor,
                     std::string& decoded)
{
    if (cursor >= text.size() || text[cursor++] != '"') return false;
    decoded.clear();
    while (cursor < text.size()) {
        const unsigned char ch = text[cursor++];
        if (ch == '"') return true;
        if (ch < 0x20) return false;
        if (ch != '\\') {
            decoded.push_back(static_cast<char>(ch));
            continue;
        }
        if (cursor >= text.size()) return false;
        const char escaped = text[cursor++];
        const char* names = "\"\\/bfnrt";
        const char* values = "\"\\/\b\f\n\r\t";
        const char* found = std::strchr(names, escaped);
        if (found) {
            decoded.push_back(values[found - names]);
            continue;
        }
        if (escaped != 'u' || cursor + 4 > text.size()) return false;
        // Preserve a normalized marker; consumer keys are ASCII and payload
        // string contents are opaque after syntax validation.
        for (int index = 0; index < 4; ++index) {
            const char digit = text[cursor++];
            if (!((digit >= '0' && digit <= '9') ||
                  (digit >= 'a' && digit <= 'f') ||
                  (digit >= 'A' && digit <= 'F')))
                return false;
        }
        decoded.push_back('?');
    }
    return false;
}

bool parsePayloadValue(const std::string& text, std::size_t& cursor,
                       RefereeAuthorityPayloadType type)
{
    if (type == RefereeAuthorityPayloadType::ArrayTwoNumbers) {
        if (cursor >= text.size() || text[cursor++] != '[') return false;
        if (!parsePayloadValue(text, cursor,
                RefereeAuthorityPayloadType::Number) ||
            cursor >= text.size() || text[cursor++] != ',' ||
            !parsePayloadValue(text, cursor,
                RefereeAuthorityPayloadType::Number) ||
            cursor >= text.size() || text[cursor++] != ']') return false;
        return true;
    }
    if (type == RefereeAuthorityPayloadType::String) {
        std::string ignored;
        return parseJsonString(text, cursor, ignored);
    }
    const char* literal = type == RefereeAuthorityPayloadType::Null ?
        "null" : nullptr;
    if (type == RefereeAuthorityPayloadType::Bool) {
        if (text.compare(cursor, 4, "true") == 0) {
            cursor += 4;
            return true;
        }
        if (text.compare(cursor, 5, "false") == 0) {
            cursor += 5;
            return true;
        }
        return false;
    }
    if (literal) {
        const std::size_t size = std::strlen(literal);
        if (text.compare(cursor, size, literal) != 0) return false;
        cursor += size;
        return true;
    }
    const std::size_t start = cursor;
    if (cursor < text.size() && text[cursor] == '-') ++cursor;
    if (cursor >= text.size()) return false;
    if (text[cursor] == '0') {
        ++cursor;
        if (cursor < text.size() && text[cursor] >= '0' &&
            text[cursor] <= '9') return false;
    } else {
        if (text[cursor] < '1' || text[cursor] > '9') return false;
        while (cursor < text.size() && text[cursor] >= '0' &&
               text[cursor] <= '9') ++cursor;
    }
    if (cursor < text.size() && text[cursor] == '.') {
        ++cursor;
        const std::size_t fraction = cursor;
        while (cursor < text.size() && text[cursor] >= '0' &&
               text[cursor] <= '9') ++cursor;
        if (cursor == fraction) return false;
    }
    if (cursor < text.size() &&
        (text[cursor] == 'e' || text[cursor] == 'E')) {
        ++cursor;
        if (cursor < text.size() &&
            (text[cursor] == '+' || text[cursor] == '-')) ++cursor;
        const std::size_t exponent = cursor;
        while (cursor < text.size() && text[cursor] >= '0' &&
               text[cursor] <= '9') ++cursor;
        if (cursor == exponent) return false;
    }
    const double value = std::strtod(
        text.substr(start, cursor - start).c_str(), nullptr);
    return std::isfinite(value);
}

bool sameRoleSubject(const RefereeAuthorityDeclarationRecord& record,
                     const RefereeAuthorityRequest& request)
{
    return record.authorityRole == request.authorityRole &&
        record.authoritySubjectId == request.authoritySubjectId;
}

bool payloadMatchesPolicy(const RefereeAuthorityConsumerPolicy& policy,
                          RefereeAuthorityOperation operation,
                          const std::string& canonical)
{
    if (canonical.empty() || canonical.size() > policy.maxPayloadBytes)
        return false;
    std::size_t cursor = 0;
    if (cursor >= canonical.size() || canonical[cursor++] != '{')
        return false;
    const auto* fields = &policy.exactPayloadKeyTypes;
    if (operation == RefereeAuthorityOperation::Propose &&
        !policy.proposalPayloadKeyTypes.empty())
        fields = &policy.proposalPayloadKeyTypes;
    else if (operation == RefereeAuthorityOperation::FinalDeclare &&
        !policy.finalPayloadKeyTypes.empty())
        fields = &policy.finalPayloadKeyTypes;
    else if (operation == RefereeAuthorityOperation::CancelProposal &&
        !policy.cancelPayloadKeyTypes.empty())
        fields = &policy.cancelPayloadKeyTypes;
    std::size_t index = 0;
    for (const auto& required : *fields) {
        if (index++ != 0 &&
            (cursor >= canonical.size() || canonical[cursor++] != ','))
            return false;
        std::string key;
        if (!parseJsonString(canonical, cursor, key) ||
            key != required.first || cursor >= canonical.size() ||
            canonical[cursor++] != ':' ||
            !parsePayloadValue(canonical, cursor, required.second))
            return false;
    }
    return cursor < canonical.size() && canonical[cursor++] == '}' &&
        cursor == canonical.size();
}

bool roleAllowed(const RefereeAuthorityConsumerPolicy& policy,
                 RefereeAuthorityRole role)
{
    return std::find(policy.allowedActorRoles.begin(),
        policy.allowedActorRoles.end(), role) !=
        policy.allowedActorRoles.end();
}

std::string recordCanonical(
    const std::optional<RefereeAuthorityDeclarationRecord>& record)
{
    if (!record) return "none";
    std::ostringstream value;
    value << refereeAuthorityOperationName(record->operation) << '|';
    appendField(value, record->declarationKind);
    appendField(value, record->decisionId);
    appendField(value, record->rackIdentity);
    value << refereeAuthorityRoleName(record->authorityRole) << '|';
    appendField(value, record->authoritySubjectId);
    appendField(value, record->authenticationNonce);
    value << record->expiresAtAuthorityTick << '|';
    appendField(value, record->idempotencyKey);
    appendField(value, record->payloadSha256);
    return value.str();
}

RefereeAuthorityDeclarationRecord recordFrom(
    const RefereeAuthorityRequest& request)
{
    RefereeAuthorityDeclarationRecord value;
    value.operation = request.operation;
    value.declarationKind = request.declarationKind;
    value.decisionId = request.decisionId;
    value.rackIdentity = request.rackIdentity;
    value.authorityRole = request.authorityRole;
    value.authoritySubjectId = request.authoritySubjectId;
    value.authenticationNonce = request.authenticationNonce;
    value.expiresAtAuthorityTick = request.expiresAtAuthorityTick;
    value.idempotencyKey = request.idempotencyKey;
    value.payloadCanonical = request.payloadCanonical;
    value.payloadSha256 = protocolSha256Hex(request.payloadCanonical);
    return value;
}

RefereeAuthorityResult rejected(const char* code)
{
    RefereeAuthorityResult value;
    value.errorCode = code;
    return value;
}

}  // namespace

const char* refereeAuthorityRoleName(RefereeAuthorityRole role)
{
    switch (role) {
    case RefereeAuthorityRole::Referee: return "REFEREE";
    case RefereeAuthorityRole::Player0: return "PLAYER_0";
    case RefereeAuthorityRole::Player1: return "PLAYER_1";
    }
    return "UNKNOWN";
}

const char* refereeAuthorityOperationName(
    RefereeAuthorityOperation operation)
{
    switch (operation) {
    case RefereeAuthorityOperation::Propose: return "PROPOSE";
    case RefereeAuthorityOperation::FinalDeclare: return "FINAL_DECLARE";
    case RefereeAuthorityOperation::CancelProposal:
        return "CANCEL_PROPOSAL";
    }
    return "UNKNOWN";
}

std::string canonicalRefereeAuthenticatedBytes(
    const RefereeAuthorityRequest& request)
{
    std::ostringstream value;
    appendField(value, request.protocolId);
    value << request.schemaVersion << '|'
          << refereeAuthorityOperationName(request.operation) << '|';
    appendField(value, request.declarationKind);
    appendField(value, request.decisionId);
    appendField(value, request.rackIdentity);
    value << request.expectedAuthorityRevision << '|';
    appendField(value, request.expectedAuthorityStateHash);
    value << refereeAuthorityRoleName(request.authorityRole) << '|';
    appendField(value, request.authoritySubjectId);
    appendField(value, request.credentialId);
    appendField(value, request.keyVersion);
    appendField(value, request.authenticationNonce);
    value << request.expiresAtAuthorityTick << '|';
    appendField(value, request.idempotencyKey);
    appendField(value, request.payloadCanonical);
    return value.str();
}

std::string refereeAuthoritySha256Hex(const std::string& bytes)
{
    return protocolSha256Hex(bytes);
}

RefereeAuthorityProtocol::RefereeAuthorityProtocol()
{
    reset("runtime.rack.unconfigured");
}

RefereeAuthorityProtocol::RefereeAuthorityProtocol(
    std::vector<RefereeAuthorityConsumerPolicy> policies,
    const RefereeAuthorityAuthenticator* authenticator,
    const RefereeAuthorityCommitGuard* commitGuard)
    : policies_(std::move(policies)),
      authenticator_(authenticator),
      commitGuard_(commitGuard)
{
    reset("runtime.rack.unconfigured");
}

void RefereeAuthorityProtocol::reset(const std::string& rackIdentity)
{
    state_ = RefereeAuthorityState{};
    state_.rackIdentity = rackIdentity;
    state_.resetGeneration = ++resetGeneration_;
    state_.auditDigest = protocolSha256Hex(
        "referee_authority_audit_v1|empty");
    state_.authorityStateHash = stateHash(state_);
    receipts_.clear();
    acceptedNonces_.clear();
}

const RefereeAuthorityConsumerPolicy* RefereeAuthorityProtocol::policy(
    const std::string& declarationKind) const
{
    const auto found = std::find_if(policies_.begin(), policies_.end(),
        [&](const RefereeAuthorityConsumerPolicy& candidate) {
            return candidate.declarationKind == declarationKind;
        });
    return found == policies_.end() ? nullptr : &*found;
}

std::string RefereeAuthorityProtocol::stateHash(
    const RefereeAuthorityState& state) const
{
    std::ostringstream canonical;
    appendField(canonical, state.protocolId);
    appendField(canonical, state.rackIdentity);
    canonical << state.resetGeneration << '|'
              << state.authorityRevision << '|';
    appendField(canonical, recordCanonical(state.liveProposal));
    appendField(canonical, recordCanonical(state.latestFinalDeclaration));
    appendField(canonical, state.auditDigest);
    return protocolSha256Hex(canonical.str());
}

RefereeAuthorityResult RefereeAuthorityProtocol::submit(
    const RefereeAuthorityRequest& request,
    std::uint64_t currentAuthorityTick)
{
    const std::string authenticatedBytes =
        canonicalRefereeAuthenticatedBytes(request);
    if (request.operation != RefereeAuthorityOperation::Propose &&
        request.operation != RefereeAuthorityOperation::FinalDeclare &&
        request.operation != RefereeAuthorityOperation::CancelProposal)
        return rejected("referee_authority_operation_unknown");
    const RefereeAuthorityResult receipt = exactReceipt(request);
    if (receipt.ok ||
        receipt.errorCode != "referee_authority_receipt_missing")
        return receipt;
    const auto priorNonce = acceptedNonces_.find(request.authenticationNonce);
    if (priorNonce != acceptedNonces_.end())
        return rejected(priorNonce->second == authenticatedBytes ?
            "referee_authority_nonce_replay" :
            "referee_authority_nonce_conflict");

    if (request.protocolId != kRefereeAuthorityProtocolId ||
        request.schemaVersion != 1)
        return rejected("referee_authority_schema_mismatch");
    const RefereeAuthorityConsumerPolicy* consumer =
        policy(request.declarationKind);
    if (!consumer)
        return rejected("referee_authority_declaration_kind_unknown");
    if (!roleAllowed(*consumer, request.authorityRole))
        return rejected("referee_authority_role_forbidden");
    if (!boundedIdentity(request.decisionId) ||
        !boundedIdentity(request.rackIdentity) ||
        !boundedIdentity(request.expectedAuthorityStateHash, 64) ||
        !boundedIdentity(request.authoritySubjectId) ||
        !boundedIdentity(request.credentialId) ||
        !boundedIdentity(request.keyVersion) ||
        !boundedIdentity(request.authenticationNonce) ||
        !boundedIdentity(request.authenticationProof, 512) ||
        !boundedIdentity(request.idempotencyKey))
        return rejected("referee_authority_identity_invalid");
    if (request.rackIdentity != state_.rackIdentity ||
        request.expectedAuthorityRevision != state_.authorityRevision ||
        request.expectedAuthorityStateHash != state_.authorityStateHash)
        return rejected("referee_authority_state_stale");
    if (request.expiresAtAuthorityTick < currentAuthorityTick ||
        request.expiresAtAuthorityTick - currentAuthorityTick >
            consumer->maxLifetimeTicks)
        return rejected("referee_authority_expired");
    if (!payloadMatchesPolicy(*consumer, request.operation,
            request.payloadCanonical))
        return rejected("referee_authority_payload_invalid");
    if (!authenticator_ || !authenticator_->verify(
            RefereeAuthenticationInput{request.credentialId,
                request.keyVersion, request.authenticationProof,
                authenticatedBytes}))
        return rejected("referee_authority_authentication_failed");

    if (request.operation == RefereeAuthorityOperation::Propose) {
        if (state_.liveProposal)
            return rejected("referee_authority_proposal_pending");
    } else {
        if (!state_.liveProposal)
            return rejected("referee_authority_proposal_missing");
        const RefereeAuthorityDeclarationRecord& proposal =
            *state_.liveProposal;
        if (proposal.declarationKind != request.declarationKind ||
            proposal.decisionId != request.decisionId ||
            proposal.rackIdentity != request.rackIdentity ||
            ((!consumer->finalPayloadMayDifferFromProposal &&
              request.operation == RefereeAuthorityOperation::FinalDeclare &&
              proposal.payloadCanonical != request.payloadCanonical) ||
             (!consumer->cancelPayloadMayDifferFromProposal &&
              request.operation == RefereeAuthorityOperation::CancelProposal &&
              proposal.payloadCanonical != request.payloadCanonical)))
            return rejected("referee_authority_proposal_mismatch");
        if (proposal.expiresAtAuthorityTick < currentAuthorityTick)
            return rejected("referee_authority_proposal_expired");
        if ((request.operation ==
                RefereeAuthorityOperation::FinalDeclare ||
             consumer->sameActorCancelRequired) &&
            !sameRoleSubject(proposal, request))
            return rejected("referee_authority_actor_mismatch");
    }

    RefereeAuthorityState candidate = state_;
    const std::uint64_t previousRevision = candidate.authorityRevision;
    ++candidate.authorityRevision;
    if (request.operation == RefereeAuthorityOperation::Propose) {
        candidate.liveProposal = recordFrom(request);
    } else if (request.operation ==
            RefereeAuthorityOperation::FinalDeclare) {
        candidate.latestFinalDeclaration = recordFrom(request);
        candidate.liveProposal.reset();
    } else {
        candidate.liveProposal.reset();
    }

    RefereeAuthorityAuditEntry journal;
    journal.sequence = candidate.journal.size() + 1;
    journal.operation = request.operation;
    journal.declarationKind = request.declarationKind;
    journal.decisionId = request.decisionId;
    journal.rackIdentity = request.rackIdentity;
    journal.authorityRole = request.authorityRole;
    journal.authoritySubjectId = request.authoritySubjectId;
    journal.previousAuthorityRevision = previousRevision;
    journal.authoritativeRevision = candidate.authorityRevision;
    journal.payloadSha256 = protocolSha256Hex(request.payloadCanonical);
    journal.proposalPending = candidate.liveProposal.has_value();
    journal.finalDeclarationCommitted =
        request.operation == RefereeAuthorityOperation::FinalDeclare;
    candidate.journal.push_back(journal);
    candidate.auditDigest = protocolSha256Hex(state_.auditDigest + "|" +
        std::to_string(journal.sequence) + "|" +
        refereeAuthorityOperationName(journal.operation) + "|" +
        journal.declarationKind + "|" + journal.decisionId + "|" +
        journal.rackIdentity + "|" +
        refereeAuthorityRoleName(journal.authorityRole) + "|" +
        journal.authoritySubjectId + "|" + journal.payloadSha256);
    candidate.authorityStateHash = stateHash(candidate);

    RefereeAuthorityAck ack;
    ack.accepted = true;
    ack.operation = request.operation;
    ack.declarationKind = request.declarationKind;
    ack.decisionId = request.decisionId;
    ack.proposalDecisionId = request.decisionId;
    ack.authorityRole = request.authorityRole;
    ack.authoritySubjectId = request.authoritySubjectId;
    ack.credentialId = request.credentialId;
    ack.keyVersion = request.keyVersion;
    ack.authenticationNonce = request.authenticationNonce;
    ack.idempotencyKey = request.idempotencyKey;
    ack.previousAuthorityRevision = previousRevision;
    ack.authoritativeRevision = candidate.authorityRevision;
    ack.authoritativeStateHash = candidate.authorityStateHash;
    ack.expiresAtAuthorityTick = request.expiresAtAuthorityTick;
    ack.proposalPending = candidate.liveProposal.has_value();
    ack.finalDeclarationCommitted = journal.finalDeclarationCommitted;
    ack.payloadSha256 = journal.payloadSha256;

    if (commitGuard_ && !commitGuard_->allowCommit())
        return rejected("referee_authority_commit_failed");
    state_ = std::move(candidate);
    receipts_.emplace(request.idempotencyKey,
        Receipt{authenticatedBytes,
            protocolSha256Hex(request.authenticationProof),
            ack});
    acceptedNonces_.emplace(request.authenticationNonce,
        authenticatedBytes);
    RefereeAuthorityResult value;
    value.ok = true;
    value.ack = ack;
    return value;
}

RefereeAuthorityResult RefereeAuthorityProtocol::exactReceipt(
    const RefereeAuthorityRequest& request) const
{
    const auto priorReceipt = receipts_.find(request.idempotencyKey);
    if (priorReceipt == receipts_.end())
        return rejected("referee_authority_receipt_missing");
    const std::string authenticatedBytes =
        canonicalRefereeAuthenticatedBytes(request);
    const std::string proofDigest =
        protocolSha256Hex(request.authenticationProof);
    if (priorReceipt->second.authenticatedBytes == authenticatedBytes &&
        priorReceipt->second.authenticationProofSha256 == proofDigest &&
        priorReceipt->second.ack.authenticationNonce ==
            request.authenticationNonce &&
        priorReceipt->second.ack.payloadSha256 ==
            protocolSha256Hex(request.payloadCanonical) &&
        priorReceipt->second.ack.credentialId == request.credentialId &&
        priorReceipt->second.ack.keyVersion == request.keyVersion &&
        !proofDigest.empty()) {
        RefereeAuthorityResult value;
        value.ok = true;
        value.ack = priorReceipt->second.ack;
        return value;
    }
    return rejected("referee_authority_idempotency_conflict");
}

RefereeAuthorityResult RefereeAuthorityProtocol::verifyDetached(
    const RefereeAuthorityRequest& request,
    std::uint64_t currentAuthorityTick) const
{
    if (request.protocolId != kRefereeAuthorityProtocolId ||
        request.schemaVersion != 1 ||
        request.operation != RefereeAuthorityOperation::Propose)
        return rejected("referee_authority_schema_mismatch");
    const RefereeAuthorityConsumerPolicy* consumer =
        policy(request.declarationKind);
    if (!consumer || !roleAllowed(*consumer, request.authorityRole))
        return rejected("referee_authority_role_forbidden");
    if (request.rackIdentity != state_.rackIdentity ||
        request.expectedAuthorityRevision != state_.authorityRevision ||
        request.expectedAuthorityStateHash != state_.authorityStateHash)
        return rejected("referee_authority_state_stale");
    if (request.expiresAtAuthorityTick < currentAuthorityTick ||
        request.expiresAtAuthorityTick - currentAuthorityTick >
            consumer->maxLifetimeTicks)
        return rejected("referee_authority_expired");
    if (!boundedIdentity(request.decisionId) ||
        !boundedIdentity(request.authoritySubjectId) ||
        !boundedIdentity(request.credentialId) ||
        !boundedIdentity(request.keyVersion) ||
        !boundedIdentity(request.authenticationNonce) ||
        !boundedIdentity(request.authenticationProof, 512) ||
        !boundedIdentity(request.idempotencyKey) ||
        !payloadMatchesPolicy(*consumer, request.operation,
            request.payloadCanonical))
        return rejected("referee_authority_payload_invalid");
    if (!authenticator_ || !authenticator_->verify(
            RefereeAuthenticationInput{request.credentialId,
                request.keyVersion, request.authenticationProof,
                canonicalRefereeAuthenticatedBytes(request)}))
        return rejected("referee_authority_authentication_failed");
    RefereeAuthorityResult value;
    value.ok = true;
    value.ack.accepted = true;
    value.ack.operation = request.operation;
    value.ack.declarationKind = request.declarationKind;
    value.ack.decisionId = request.decisionId;
    value.ack.authorityRole = request.authorityRole;
    value.ack.authoritySubjectId = request.authoritySubjectId;
    value.ack.authenticationNonce = request.authenticationNonce;
    value.ack.idempotencyKey = request.idempotencyKey;
    value.ack.payloadSha256 = protocolSha256Hex(
        request.payloadCanonical);
    return value;
}

}  // namespace billiardgl::rules
