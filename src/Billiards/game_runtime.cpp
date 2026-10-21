#include "game_runtime.h"

#include "coupled_cue_contact.h"
#include "frozen_cue_topology.h"
#include "input.h"
#include "pocket_boundary.h"
#include "physics.h"
#include "rules.h"
#include "shot.h"
#include "table_specs.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace billiardgl {

namespace {

constexpr std::array<std::uint32_t, 64> kAuthorityHashRounds = {{
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

std::uint32_t authorityRotateRight(std::uint32_t value, int count)
{
    return (value >> count) | (value << (32 - count));
}

std::string authoritySha256Hex(const std::string& bytes)
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
            const std::uint32_t s0 =
                authorityRotateRight(words[index - 15], 7) ^
                authorityRotateRight(words[index - 15], 18) ^
                (words[index - 15] >> 3);
            const std::uint32_t s1 =
                authorityRotateRight(words[index - 2], 17) ^
                authorityRotateRight(words[index - 2], 19) ^
                (words[index - 2] >> 10);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (int index = 0; index < 64; ++index) {
            const std::uint32_t sum1 = authorityRotateRight(e, 6) ^
                authorityRotateRight(e, 11) ^ authorityRotateRight(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t temporary1 = h + sum1 + choice +
                kAuthorityHashRounds[index] + words[index];
            const std::uint32_t sum0 = authorityRotateRight(a, 2) ^
                authorityRotateRight(a, 13) ^ authorityRotateRight(a, 22);
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
    for (std::uint32_t value : state) output << std::setw(8) << value;
    return output.str();
}

template <typename Float>
Float authorityCanonicalZero(Float value)
{
    // IEEE signed zero is physically identical here.  Surface integration
    // may produce -0 at the exact stopping instant and normalize it to +0 on
    // a later stationary step; the authority digest must not treat that
    // representation-only cleanup as gameplay mutation.
    return value == Float{0} ? Float{0} : value;
}

std::string replayStableShotIdentity(const std::string& value)
{
    constexpr const char* prefix="runtime.stroke.";
    if(value.compare(0,std::char_traits<char>::length(prefix),prefix)!=0)
        return value;
    const std::size_t generationEnd=value.find('.',
        std::char_traits<char>::length(prefix));
    if(generationEnd==std::string::npos)return value;
    return std::string(prefix)+"*"+value.substr(generationEnd);
}

std::string cueImpactReplayBindingSha256(const CueImpactInput& input,
    const PhysicsProfile& profile)
{
    std::ostringstream canonical;
    canonical.imbue(std::locale::classic());
    canonical << std::setprecision(
            std::numeric_limits<double>::max_digits10)
              << "06g_consuming_cue_input_v1"
              << '|' << input.cueBallIndex
              << '|' << input.cueSpeedCmS
              << '|' << input.cueMassKg
              << '|' << input.direction[0]
              << '|' << input.direction[1]
              << '|' << input.direction[2]
              << '|' << input.elevationDegrees
              << '|' << input.tipOffsetCm[0]
              << '|' << input.tipOffsetCm[1]
              << '|' << input.tipOffsetRadius[0]
              << '|' << input.tipOffsetRadius[1]
              << '|' << input.chalkState.size() << ':' << input.chalkState
              << '|' << profile.id.size() << ':' << profile.id
              << '|' << authoritySha256Hex(
                    canonicalPhysicsProfileJson(profile));
    return authoritySha256Hex(canonical.str());
}

bool sameGroupChoiceRequest(const rules::GroupChoiceDecisionRequest& first,
                            const rules::GroupChoiceDecisionRequest& second)
{
    return first.rulesetId == second.rulesetId &&
        first.capabilityId == second.capabilityId &&
        first.flowId == second.flowId &&
        first.decisionId == second.decisionId &&
        first.expectedAuthorityStateRevision ==
            second.expectedAuthorityStateRevision &&
        first.idempotencyKey == second.idempotencyKey &&
        first.actorPlayerId == second.actorPlayerId &&
        first.actionId == second.actionId &&
        first.payloadCanonical == second.payloadCanonical;
}

bool sameBallInHandPlacementRequest(
    const rules::BallInHandPlacementRequest& first,
    const rules::BallInHandPlacementRequest& second)
{
    return first.rulesetId == second.rulesetId &&
        first.capabilityId == second.capabilityId &&
        first.flowId == second.flowId &&
        first.decisionId == second.decisionId &&
        first.expectedAuthorityStateRevision ==
            second.expectedAuthorityStateRevision &&
        first.idempotencyKey == second.idempotencyKey &&
        first.actorPlayerId == second.actorPlayerId &&
        first.actionId == second.actionId &&
        first.position.worldX == second.position.worldX &&
        first.position.worldZ == second.position.worldZ;
}

bool sameBehindHeadStringRespotRequest(
    const rules::BehindHeadStringRespotRequest& first,
    const rules::BehindHeadStringRespotRequest& second)
{
    return first.rulesetId == second.rulesetId &&
        first.capabilityId == second.capabilityId &&
        first.flowId == second.flowId &&
        first.decisionId == second.decisionId &&
        first.expectedAuthorityStateRevision == second.expectedAuthorityStateRevision &&
        first.expectedAuthorityStateHash == second.expectedAuthorityStateHash &&
        first.idempotencyKey == second.idempotencyKey &&
        first.actorPlayerId == second.actorPlayerId &&
        first.actionId == second.actionId &&
        first.selectedBallId == second.selectedBallId &&
        first.payloadCanonical == second.payloadCanonical;
}

bool sameEightOnBreakRequest(
    const rules::EightOnBreakDecisionRequest& first,
    const rules::EightOnBreakDecisionRequest& second)
{
    return first.rulesetId == second.rulesetId &&
        first.capabilityId == second.capabilityId &&
        first.flowId == second.flowId &&
        first.decisionId == second.decisionId &&
        first.expectedAuthorityStateRevision ==
            second.expectedAuthorityStateRevision &&
        first.idempotencyKey == second.idempotencyKey &&
        first.actorPlayerId == second.actorPlayerId &&
        first.actionId == second.actionId &&
        first.payloadCanonical == second.payloadCanonical;
}

bool sameBreakFoulRequest(
    const rules::BreakFoulDecisionRequest& first,
    const rules::BreakFoulDecisionRequest& second)
{
    return first.rulesetId == second.rulesetId &&
        first.capabilityId == second.capabilityId &&
        first.flowId == second.flowId &&
        first.decisionId == second.decisionId &&
        first.expectedAuthorityStateRevision ==
            second.expectedAuthorityStateRevision &&
        first.idempotencyKey == second.idempotencyKey &&
        first.actorPlayerId == second.actorPlayerId &&
        first.actionId == second.actionId &&
        first.payloadCanonical == second.payloadCanonical;
}

bool sameRackFoundationPoint(const Point3& first, const Point3& second)
{
    return first.x == second.x && first.y == second.y &&
        first.z == second.z;
}

bool sameRackFoundationPlacements(
    const std::vector<std::pair<int, Point3>>& first,
    const std::vector<std::pair<int, Point3>>& second)
{
    if (first.size() != second.size()) return false;
    for (std::size_t index = 0; index < first.size(); ++index) {
        if (first[index].first != second[index].first ||
            !sameRackFoundationPoint(
                first[index].second, second[index].second))
            return false;
    }
    return true;
}

bool sameOfficialRackContract(
    const rules::OfficialRackContract& first,
    const rules::OfficialRackContract& second)
{
    if (first.rulesetId != second.rulesetId ||
        first.sourceClause != second.sourceClause ||
        first.algorithmId != second.algorithmId ||
        first.deterministicSeed != second.deterministicSeed ||
        !sameRackFoundationPoint(first.footSpot, second.footSpot) ||
        !sameRackFoundationPoint(first.cueBallBreakPosition,
            second.cueBallBreakPosition) ||
        first.apexSlot != second.apexSlot ||
        first.eightBallSlot != second.eightBallSlot ||
        first.baseCornerSlots != second.baseCornerSlots ||
        first.sameGroupAdjacencyCount != second.sameGroupAdjacencyCount ||
        first.minimumPossibleSameGroupAdjacencyCount !=
            second.minimumPossibleSameGroupAdjacencyCount ||
        first.numberedBallOrderIsOfficial !=
            second.numberedBallOrderIsOfficial)
        return false;
    for (std::size_t index = 0; index < first.slots.size(); ++index) {
        const rules::RackSlot& a = first.slots[index];
        const rules::RackSlot& b = second.slots[index];
        if (a.slotIndex != b.slotIndex || a.row != b.row ||
            a.column != b.column || a.ball != b.ball ||
            !sameRackFoundationPoint(a.position, b.position))
            return false;
    }
    return true;
}

bool sameRackFoundationPlan(const rules::RackFoundationPlan& first,
                            const rules::RackFoundationPlan& second)
{
    return first.ok == second.ok && first.errorCode == second.errorCode &&
        first.operation == second.operation &&
        first.transactionId == second.transactionId &&
        first.expectedAuthorityStateRevision ==
            second.expectedAuthorityStateRevision &&
        first.baseAuthorityStateHash == second.baseAuthorityStateHash &&
        first.rerack.deterministicSeed ==
            second.rerack.deterministicSeed &&
        first.rerack.breakerPlayerId == second.rerack.breakerPlayerId &&
        first.respot.ball == second.respot.ball &&
        sameRackFoundationPoint(first.respot.nominalPosition,
            second.respot.nominalPosition) &&
        first.respot.search.stepCm == second.respot.search.stepCm &&
        first.respot.search.maximumCandidates ==
            second.respot.search.maximumCandidates &&
        first.respot.search.clearanceCm ==
            second.respot.search.clearanceCm &&
        sameOfficialRackContract(first.rackContract,
            second.rackContract) &&
        sameRackFoundationPlacements(first.placements,
            second.placements) &&
        first.candidatesExamined == second.candidatesExamined;
}

const char* stableContactError(const std::string& error)
{
    if (error == "invalid_cue_contact_input") return "invalid_cue_contact_input";
    if (error == "cue_elevation_requires_3d") return "cue_elevation_requires_3d";
    if (error == "invalid_cue_direction") return "invalid_cue_direction";
    if (error == "cue_offset_outside_ball") return "cue_offset_outside_ball";
    if (error == "miscue_offset_exceeds_reliable_radius")
        return "miscue_offset_exceeds_reliable_radius";
    if (error == "cue_not_approaching") return "cue_not_approaching";
    if (error == "cue_not_approaching_contact_normal")
        return "cue_not_approaching_contact_normal";
    if (error == "vertical_ball_impulse_requires_3d")
        return "vertical_ball_impulse_requires_3d";
    if (error == "nonfinite_state") return "nonfinite_state";
    if (error == "passive_energy_gain") return "passive_energy_gain";
    if (error == "compression_limit") return "compression_limit";
    if (error == "contact_island_limit") return "contact_island_limit";
    if (error == "cue_contact_no_release") return "cue_contact_no_release";
    if (error == "cue_contact_nonconvergence")
        return "cue_contact_nonconvergence";
    return "cue_contact_failed";
}

const char* stableShotControlError(const std::string& error)
{
    if (error == "tip_offset_out_of_range") return "tip_offset_out_of_range";
    if (error == "shot_control_nonfinite") return "shot_control_nonfinite";
    if (error == "requested_power_out_of_range") return "requested_power_out_of_range";
    if (error == "stroke_speed_out_of_range") return "stroke_speed_out_of_range";
    if (error == "cue_elevation_capability_required") return "cue_elevation_capability_required";
    if (error == "chalk_state_capability_required") return "chalk_state_capability_required";
    return "invalid_shot_control";
}

PresentationSnapshot cameraSnapshot(const GameState& state, bool cameraCandidate = false)
{
    PresentationSnapshot snapshot;
    snapshot.balls[0].position = PresentationPoint{
        state.balls[0].position.x, state.balls[0].position.y,
        state.balls[0].position.z};
    snapshot.balls[0].visible = !state.balls[0].pocketed && !state.balls[0].outOfPlay;
    if (cameraCandidate) snapshot.shotControl.yawRadians = state.aim.yaw;
    return snapshot;
}

}  // namespace

CueShotApplication applyCueShot(GameState& state, const CueImpactInput& input,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode)
{
    CueShotApplication application;
    if (input.cueBallIndex != 0) {
        application.action = ActionResult{false, "cue_ball_index_not_supported"};
        application.contact.error = "cue_ball_index_not_supported";
        return application;
    }
    if (state.balls[0].pocketed || state.balls[0].outOfPlay) {
        application.action = ActionResult{false, "cue_ball_not_in_play"};
        application.contact.error = "cue_ball_not_in_play";
        return application;
    }

    bool coupled = false;
    if (profile.frozenCueContact.enabled) {
        const FrozenCueTopology topology = detectFrozenCueTopology(
            state, input.cueBallIndex, profile, boundaryMode);
        if (topology.status == FrozenCueTopologyStatus::IslandLimit) {
            application.contact.error = "contact_island_limit";
            application.action = ActionResult{false, "contact_island_limit"};
            return application;
        }
        if (topology.status ==
            FrozenCueTopologyStatus::ContradictoryTopology) {
            application.contact.error = "cue_contact_nonconvergence";
            application.action = ActionResult{
                false, "cue_contact_nonconvergence"};
            return application;
        }
        if (topology.frozen) {
            coupled = true;
            const CoupledCueContactResult solved = solveCoupledCueContact(
                state, topology, input, profile);
            application.contact = solved.contact;
            if (solved.status == CoupledCueContactStatus::Released) {
                state = solved.state;
            } else {
                application.action = ActionResult{
                    false, stableContactError(solved.error)};
                return application;
            }
        }
    }
    if (!coupled) {
        application.contact = resolveCueContact(
            state.balls[0], input, profile.ball, profile.cue);
    }
    const bool modeledMiscue =
        application.contact.regime == CueContactRegime::Miscue;
    if (!application.contact.applied && !modeledMiscue) {
        application.action = ActionResult{
            false, stableContactError(application.contact.error)};
        return application;
    }

    state.players.nextPlayer = 1 - state.players.currentPlayer;
    state.players.illegalShot = false;
    state.players.shotTaken = true;
    state.players.updatedAfterShot = false;
    state.ballsMoving = application.contact.applied;
    state.camera.anchorMode = CameraAnchorMode::FreeLook;
    state.transitionPerspective = false;
    state.perspectiveRecorded = false;
    state.aim.mode = AimMode::Observe;
    state.players.aimingAtCueBall = false;
    state.input.hitRequested = false;
    return application;
}

CueImpactSupport evaluateCueImpactSupport(
    const CueImpactInput& input, const CueContactResult* result)
{
    CueImpactSupport support;
    if (result) {
        support.shotExecuted = result->applied;
        if (result->applied) {
            support.exactlyConsumableFields.push_back("cue_ball_index");
            support.exactlyConsumableFields.push_back("cue_speed_cm_s");
            support.exactlyConsumableFields.push_back("cue_mass_kg");
            support.exactlyConsumableFields.push_back("horizontal_direction");
            support.exactlyConsumableFields.push_back("horizontal_tip_offset");
            support.exactlyConsumableFields.push_back("vertical_tip_offset");
            support.exactlyConsumableFields.push_back("chalk_state");
        }
        if (!result->applied && !result->error.empty())
            support.unsupportedCodes.push_back(result->error);
        return support;
    }
    if (input.cueBallIndex == 0) support.exactlyConsumableFields.push_back("cue_ball_index");
    else support.unsupportedCodes.push_back("cue_ball_index_not_supported");
    if (std::fabs(input.direction[1]) <= 0.000001 &&
        std::fabs(input.elevationDegrees) <= 0.000001) {
        support.exactlyConsumableFields.push_back("horizontal_direction");
    } else {
        support.unsupportedCodes.push_back("cue_elevation_not_modeled");
    }
    support.unsupportedCodes.push_back("cue_speed_to_power_mapping_missing");
    support.unsupportedCodes.push_back("cue_mass_not_modeled");
    if (std::fabs(input.tipOffsetCm[0]) > 0.000001)
        support.unsupportedCodes.push_back("horizontal_tip_offset_not_modeled");
    if (std::fabs(input.tipOffsetCm[1]) > 0.000001)
        support.unsupportedCodes.push_back("vertical_tip_offset_not_modeled");
    support.unsupportedCodes.push_back("chalk_state_not_modeled");
    return support;
}

GameRuntime::GameRuntime()
    : physicsProfile_(defaultChinesePoolPhysicsProfile())
{
    reset();
}

GameRuntime::GameRuntime(
    std::vector<rules::RefereeAuthorityConsumerPolicy> refereePolicies,
    const rules::RefereeAuthorityAuthenticator* refereeAuthenticator,
    const rules::RefereeAuthorityCommitGuard* refereeCommitGuard)
    : physicsProfile_(defaultChinesePoolPhysicsProfile()),
      refereeAuthorityProtocol_(std::move(refereePolicies),
          refereeAuthenticator, refereeCommitGuard)
{
    reset();
}

void GameRuntime::configureStalemateAuthority(
    const rules::RefereeAuthorityAuthenticator* authenticator,
    const rules::RefereeAuthorityCommitGuard* commitGuard)
{
    stalemateAuthority_.emplace(authenticator, commitGuard);
    stalemateAuthority_->reset("runtime.rack." +
        std::to_string(eventJournalGeneration_) + "." +
        std::to_string(stalemateRackIdentityOrdinal_),
        rackOriginalBreakerPlayerId_, rackOriginalShootingOrder_);
}

void GameRuntime::configureGroupConfusionAuthority(
    const rules::RefereeAuthorityAuthenticator* authenticator,
    const rules::RefereeAuthorityCommitGuard* commitGuard)
{
    groupConfusionAuthority_.emplace(authenticator, commitGuard);
    groupConfusionAuthority_->reset("runtime.group_confusion.rack." +
        std::to_string(eventJournalGeneration_) + "." +
        std::to_string(groupConfusionRackIdentityOrdinal_),
        rackOriginalBreakerPlayerId_, rackOriginalShootingOrder_);
}

void GameRuntime::configureFrozenBallDeclarationAuthority(
    const rules::RefereeAuthorityAuthenticator* authenticator,
    const rules::RefereeAuthorityCommitGuard* commitGuard)
{
    replayDeclarationAuthorityState_ = false;
    frozenBallDeclarationAuthority_.emplace(authenticator, commitGuard);
    frozenBallDeclarationAuthority_->reset(
        "runtime.frozen_declaration.rack." +
        std::to_string(eventJournalGeneration_) + "." +
        std::to_string(frozenBallDeclarationRackIdentityOrdinal_));
}

namespace {
bool legalFrozenObject(const rules::MatchState& state, int ball);
}

void GameRuntime::configureFrozenCushionDeclarationAuthority(
    const rules::RefereeAuthorityAuthenticator* authenticator,
    const rules::RefereeAuthorityCommitGuard* commitGuard)
{
    replayDeclarationAuthorityState_ = false;
    frozenCushionDeclarationAuthority_.emplace(authenticator, commitGuard);
    frozenCushionDeclarationAuthority_->reset(
        "runtime.frozen_cushion.rack." +
        std::to_string(eventJournalGeneration_) + "." +
        std::to_string(frozenCushionDeclarationRackIdentityOrdinal_));
}

void GameRuntime::configureDoubleHitCloseCaseAuthority(
    const rules::RefereeAuthorityAuthenticator* authenticator,
    const rules::RefereeAuthorityCommitGuard* commitGuard)
{
    replayDeclarationAuthorityState_ = false;
    doubleHitCloseCaseAuthority_.emplace(authenticator,commitGuard);
    doubleHitCloseCaseAuthority_->reset("runtime.double_hit_close.rack."+
        std::to_string(eventJournalGeneration_)+"."+
        std::to_string(doubleHitCloseCaseRackIdentityOrdinal_));
}
void GameRuntime::configurePushReviewAuthority(
    const rules::RefereeAuthorityAuthenticator*a,
    const rules::RefereeAuthorityCommitGuard*g)
{replayDeclarationAuthorityState_=false;
 pushReviewAuthority_.emplace(a,g);pushReviewAuthority_->reset(
    "runtime.push_review.rack."+std::to_string(eventJournalGeneration_)+"."+
    std::to_string(pushReviewRackIdentityOrdinal_));}
void GameRuntime::configureNonphysicalGeneralFoulAuthority(
    const rules::RefereeAuthorityAuthenticator*a,
    const rules::RefereeAuthorityCommitGuard*g)
{
 // Authority credentials are immutable for a runtime lifetime.  This void
 // legacy API cannot report a rejected reconfiguration, so fail closed with
 // a no-op instead of erasing same-rack history or a committed terminal.
 if(nonphysicalGeneralFoulAuthority_)return;
 nonphysicalGeneralFoulAuthority_.emplace(a,g,
    rules::NonphysicalGeneralFoulConsumerCapabilities{true});
 nonphysicalGeneralFoulAuthority_->reset("runtime.nonphysical_foul.rack."+
    std::to_string(eventJournalGeneration_)+"."+
    std::to_string(nonphysicalGeneralFoulRackIdentityOrdinal_));}

void GameRuntime::configureUnintentionalBreakMiscueDeclarationAuthority(
    const rules::RefereeAuthorityAuthenticator* authenticator,
    const rules::RefereeAuthorityCommitGuard* commitGuard)
{
    if (unintentionalBreakMiscueDeclarationAuthority_) return;
    unintentionalBreakMiscueDeclarationAuthority_.emplace(
        authenticator, commitGuard);
    unintentionalBreakMiscueDeclarationAuthority_->reset(
        "runtime.unintentional_break_miscue.rack." +
        std::to_string(eventJournalGeneration_) + "." +
        std::to_string(unintentionalBreakMiscueRackIdentityOrdinal_));
}

const rules::UnintentionalBreakMiscueDeclarationState&
GameRuntime::unintentionalBreakMiscueDeclarationState() const
{
    static const rules::UnintentionalBreakMiscueDeclarationState empty;
    return unintentionalBreakMiscueDeclarationAuthority_ ?
        unintentionalBreakMiscueDeclarationAuthority_->state() : empty;
}

const rules::RefereeAuthorityState&
GameRuntime::unintentionalBreakMiscueDeclarationProtocolState() const
{
    static const rules::RefereeAuthorityState empty;
    return unintentionalBreakMiscueDeclarationAuthority_ ?
        unintentionalBreakMiscueDeclarationAuthority_->protocolState() :
        empty;
}

rules::UnintentionalBreakMiscueDeclarationResult
GameRuntime::submitUnintentionalBreakMiscueDeclarationProposal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!unintentionalBreakMiscueDeclarationAuthority_)
        return {false,
            "unintentional_break_miscue_authority_unconfigured", {}};
    if (rulesMode_ != RulesMode::ChineseEightBallV1 ||
        matchState_.phase != rules::MatchPhase::Break ||
        state_.ballsMoving || shotFactCollector_.active() ||
        breakFoulDecisionPending() || eightOnBreakDecisionPending() ||
        ballInHandPlacementPending() || state_.gameOver ||
        state_.balls[0].pocketed || state_.balls[0].outOfPlay)
        return {false,
            "unintentional_break_miscue_parent_context", {}};
    rules::UnintentionalBreakMiscueDeclarationContext context;
    context.actorPlayerId = matchState_.currentPlayer;
    context.parentRevision = rulesAuthorityRevision_;
    context.parentStateHash = rulesAuthorityStateHash();
    return unintentionalBreakMiscueDeclarationAuthority_->propose(
        request, context, tick_);
}

rules::UnintentionalBreakMiscueDeclarationResult
GameRuntime::submitUnintentionalBreakMiscueDeclarationFinal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!unintentionalBreakMiscueDeclarationAuthority_)
        return {false,
            "unintentional_break_miscue_authority_unconfigured", {}};
    const auto context =
        unintentionalBreakMiscueDeclarationAuthority_->state().context;
    if (rulesMode_ != RulesMode::ChineseEightBallV1 ||
        matchState_.phase != rules::MatchPhase::Break ||
        context.actorPlayerId != matchState_.currentPlayer ||
        context.parentRevision != rulesAuthorityRevision_ ||
        context.parentStateHash != rulesAuthorityStateHash() ||
        state_.ballsMoving || shotFactCollector_.active() ||
        breakFoulDecisionPending() || eightOnBreakDecisionPending() ||
        ballInHandPlacementPending() || state_.gameOver ||
        state_.balls[0].pocketed || state_.balls[0].outOfPlay)
        return {false,
            "unintentional_break_miscue_parent_state_changed", {}};
    const auto before =
        unintentionalBreakMiscueDeclarationAuthority_->state();
    const std::string parent = rulesReplayEffectStateHash();
    auto result =
        unintentionalBreakMiscueDeclarationAuthority_->finalDeclare(
            request, context, tick_);
    if (result.ok && before.phase ==
            rules::UnintentionalBreakMiscueDeclarationPhase::ProposalOpen)
        appendCommittedDeclarationReplayEvent(
            RuntimeRulesReplayEventType::
                UnintentionalBreakMiscueDeclarationCommit,
            request, before.proposalDecisionId, before.rackIdentity,
            "wpa_ii_06_g_unintentional_break_miscue_declaration_v1",
            -1, -1, parent, context.actorPlayerId);
    return result;
}

rules::UnintentionalBreakMiscueDeclarationResult
GameRuntime::submitUnintentionalBreakMiscueDeclarationCancel(
    const rules::RefereeAuthorityRequest& request)
{
    if (!unintentionalBreakMiscueDeclarationAuthority_)
        return {false,
            "unintentional_break_miscue_authority_unconfigured", {}};
    return unintentionalBreakMiscueDeclarationAuthority_->cancel(
        request, tick_);
}
const rules::NonphysicalGeneralFoulState&
GameRuntime::nonphysicalGeneralFoulState()const{
    static const rules::NonphysicalGeneralFoulState e;
    return nonphysicalGeneralFoulAuthority_?
        nonphysicalGeneralFoulAuthority_->state():e;}
const rules::RefereeAuthorityState&
GameRuntime::nonphysicalGeneralFoulProtocolState()const{
    static const rules::RefereeAuthorityState e;
    return nonphysicalGeneralFoulAuthority_?
        nonphysicalGeneralFoulAuthority_->protocolState():e;}
rules::NonphysicalGeneralFoulResult
GameRuntime::submitNonphysicalGeneralFoulProposal(
    const rules::RefereeAuthorityRequest&r){
    if(!nonphysicalGeneralFoulAuthority_)
        return {false,"nonphysical_general_foul_unconfigured",{}};
    const auto exact=nonphysicalGeneralFoulAuthority_->exactReceipt(r);
    if(exact.ok||exact.errorCode!="referee_authority_receipt_missing")
        return exact;
    const std::string shot="runtime.nonphysical_general_foul.shot."+
        std::to_string(rulesAuthorityRevision_);
    int offender=matchState_.currentPlayer;
    rules::NonphysicalGeneralFoulKind kind=
        rules::NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot;
    bool identified=false;
    for(int player=0;player<2&&!identified;++player){
        for(const auto candidate:{
            rules::NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
            rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack}){
            if(r.payloadCanonical==rules::canonicalNonphysicalGeneralFoulProposal(
                    candidate,player,shot)){
                offender=player;kind=candidate;identified=true;break;
            }
        }
    }
    const bool authorizedRepeat=identified&&
        kind==rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack&&
        mobileDeviceViolationHistory_[offender]==
            rules::MobileDeviceViolationState::GeneralFoulCommitted;
    if(rulesMode_!=RulesMode::ChineseEightBallV1||state_.ballsMoving||
       shotFactCollector_.active()||matchState_.groupChoicePending||
       breakFoulDecisionPending()||eightOnBreakDecisionPending()||
       (ballInHandPlacementPending()&&!authorizedRepeat)||state_.gameOver||
       rulesRackTerminalState_.active)
        return {false,"nonphysical_general_foul_parent_context",{}};
    rules::NonphysicalGeneralFoulContext c;
    c.offendingPlayer=offender;c.opponentPlayer=1-c.offendingPlayer;
    c.shotIdentity=shot;
    c.parentRevision=rulesAuthorityRevision_;c.parentStateHash=rulesAuthorityStateHash();
    return nonphysicalGeneralFoulAuthority_->propose(r,c,tick_);}
rules::NonphysicalGeneralFoulResult
GameRuntime::submitNonphysicalGeneralFoulFinal(
    const rules::RefereeAuthorityRequest&r){
    if(!nonphysicalGeneralFoulAuthority_)
        return {false,"nonphysical_general_foul_unconfigured",{}};
    const auto exact=nonphysicalGeneralFoulAuthority_->exactReceipt(r);
    if(exact.ok||exact.errorCode!="referee_authority_receipt_missing")
        return exact;
    const auto c=nonphysicalGeneralFoulAuthority_->state().context;
    const bool repeatLoss=
        nonphysicalGeneralFoulAuthority_->state().eventKind==
            rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack;
    if(nonphysicalGeneralFoulAuthority_->state().phase!=
            rules::NonphysicalGeneralFoulPhase::Committed&&
       (c.parentRevision!=rulesAuthorityRevision_||
        c.parentStateHash!=rulesAuthorityStateHash()||
        (!repeatLoss&&c.offendingPlayer!=matchState_.currentPlayer)||
        state_.ballsMoving||
        shotFactCollector_.active()||
        (ballInHandPlacementPending()&&
         !repeatLoss)||
        breakFoulDecisionPending()||eightOnBreakDecisionPending()||state_.gameOver))
        return {false,"nonphysical_general_foul_parent_state_changed",{}};
    const auto eventKind=nonphysicalGeneralFoulAuthority_->state().eventKind;
    const auto history=mobileDeviceViolationHistory_[c.offendingPlayer];
    if((eventKind==rules::NonphysicalGeneralFoulKind::
            MobileDeviceDisturbanceGeneralFoul&&
            history!=rules::MobileDeviceViolationState::None)||
       (eventKind==rules::NonphysicalGeneralFoulKind::
            MobileDeviceRepeatLossOfRack&&
            history!=rules::MobileDeviceViolationState::GeneralFoulCommitted))
        return {false,"nonphysical_general_foul_runtime_history",{}};
    auto result=nonphysicalGeneralFoulAuthority_->finalDeclare(r,c,tick_);
    if(!result.ok||result.idempotentReplay||!result.newlyCommitted)return result;
    const ActionResult effect=applyCommittedNonphysicalGeneralFoulEffect(
        eventKind,c,result.outcome,r.decisionId,
        nonphysicalGeneralFoulAuthority_->state().rackIdentity,true);
    if(!effect.ok){result.ok=false;result.errorCode=effect.errorCode;}
    return result;}

ActionResult GameRuntime::applyCommittedNonphysicalGeneralFoulEffect(
    rules::NonphysicalGeneralFoulKind kind,
    const rules::NonphysicalGeneralFoulContext& c,
    rules::NonphysicalGeneralFoulOutcome outcome,
    const std::string& decisionId,const std::string& rackIdentity,
    bool appendReplayEvent)
{
    switch (kind) {
    case rules::NonphysicalGeneralFoulKind::BothFeetOffGroundDuringShot:
    case rules::NonphysicalGeneralFoulKind::IllegalBodyClothingOrEquipmentBallTouch:
    case rules::NonphysicalGeneralFoulKind::OneHandShot:
    case rules::NonphysicalGeneralFoulKind::AuxiliaryEquipmentPlacedOnTableToShoot:
    case rules::NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul:
    case rules::NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack:
        break;
    default:
        return ActionResult{false,"nonphysical_general_foul_replay_kind"};
    }
    if(c.offendingPlayer<0||c.offendingPlayer>1||
       c.opponentPlayer!=1-c.offendingPlayer||rackIdentity.empty())
        return ActionResult{false,"nonphysical_general_foul_replay_context"};
    if(!replayRulesRackIdentity_.empty()&&
       replayRulesRackIdentity_!=rackIdentity)
        return ActionResult{false,"nonphysical_general_foul_replay_rack"};
    const auto previousHistory=
        mobileDeviceViolationHistory_[c.offendingPlayer];
    if(kind==rules::NonphysicalGeneralFoulKind::
            MobileDeviceDisturbanceGeneralFoul){
        if(previousHistory!=rules::MobileDeviceViolationState::None||
           outcome!=rules::NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand)
            return ActionResult{false,"nonphysical_general_foul_replay_sequence"};
    }else if(kind==rules::NonphysicalGeneralFoulKind::
            MobileDeviceRepeatLossOfRack){
        if(previousHistory!=
                rules::MobileDeviceViolationState::GeneralFoulCommitted||
           outcome!=rules::NonphysicalGeneralFoulOutcome::LossOfRack)
            return ActionResult{false,"nonphysical_general_foul_replay_sequence"};
    }else if(outcome!=
            rules::NonphysicalGeneralFoulOutcome::GeneralFoulBallInHand){
        return ActionResult{false,"nonphysical_general_foul_replay_outcome"};
    }
    const std::uint64_t previousRevision=rulesAuthorityRevision_;
    const std::string parentEffect=rulesReplayEffectStateHash();
    replayRulesRackIdentity_=rackIdentity;
    if(kind==rules::NonphysicalGeneralFoulKind::
            MobileDeviceDisturbanceGeneralFoul)
        mobileDeviceViolationHistory_[c.offendingPlayer]=
            rules::MobileDeviceViolationState::GeneralFoulCommitted;
    else if(kind==rules::NonphysicalGeneralFoulKind::
            MobileDeviceRepeatLossOfRack)
        mobileDeviceViolationHistory_[c.offendingPlayer]=
            rules::MobileDeviceViolationState::RackLossCommitted;
    if(outcome==rules::NonphysicalGeneralFoulOutcome::LossOfRack){
        nonphysicalFoulExplanation_.reset();
        invalidateBallInHandAuthority();
        matchState_.ballInHand=false;
        ballInHandPlacementScope_=rules::BallInHandPlacementScope::None;
        if(hasLastAdjudication_){
            lastAdjudication_.ballInHand=false;
            lastAdjudication_.ballInHandBehindHeadString=false;
            lastAdjudication_.nextState=matchState_;
        }
        ++rulesAuthorityRevision_;
        rulesRackTerminalState_.active=true;
        rulesRackTerminalState_.reason=
            RulesRackTerminalReason::MobileDeviceRepeatViolation;
        rulesRackTerminalState_.losingPlayer=c.offendingPlayer;
        rulesRackTerminalState_.winningPlayer=c.opponentPlayer;
        rulesRackTerminalState_.rackIdentity=
            rackIdentity;
        rulesRackTerminalState_.authorityDecisionId=decisionId;
        rulesRackTerminalState_.committedAuthorityRevision=
            rulesAuthorityRevision_;
        events_.push_back(RuntimeEvent{nextSequence_++,tick_,
            "rules_mobile_device_repeat_rack_loss_committed"});
    }else{
        matchState_.currentPlayer=c.opponentPlayer;matchState_.ballInHand=true;
        lastAdjudication_=rules::AdjudicationResult{};
        lastAdjudication_.accepted=true;lastAdjudication_.foul=true;
        lastAdjudication_.continueTurn=false;lastAdjudication_.ballInHand=true;
        lastAdjudication_.ballInHandBehindHeadString=false;
        lastAdjudication_.nextPlayer=c.opponentPlayer;
        lastAdjudication_.nextState=matchState_;hasLastAdjudication_=true;
        nonphysicalFoulExplanation_ = NonphysicalFoulExplanation{
            kind, decisionId, rackIdentity, c.offendingPlayer, c.opponentPlayer};
        ++rulesAuthorityRevision_;
        ballInHandPlacementScope_=rules::BallInHandPlacementScope::FullTable;
        stageBallInHandPlacementAuthority(
            ballInHandPlacementScope_,c.opponentPlayer);
        ballInHandPlacementOfferStateHash_=rulesAuthorityStateHash();
    }
    if(appendReplayEvent){
        RuntimeRulesReplayEvent event;
        event.sequence=nextRulesReplayEventSequence_++;
        event.tick=tick_;
        event.afterAppliedSemanticActionSequence=
            appliedSemanticActions_.empty()?0:
                appliedSemanticActions_.back().sequence;
        event.type=RuntimeRulesReplayEventType::NonphysicalGeneralFoulCommit;
        event.rackIdentity=rackIdentity;event.decisionId=decisionId;
        event.foulKind=kind;event.foulOutcome=outcome;
        event.offendingPlayer=c.offendingPlayer;
        event.opponentPlayer=c.opponentPlayer;
        event.previousRulesRevision=previousRevision;
        event.committedRulesRevision=rulesAuthorityRevision_;
        event.parentEffectSha256=parentEffect;
        event.committedEffectSha256=rulesReplayEffectStateHash();
        rulesReplayEvents_.push_back(std::move(event));
        while(rulesReplayEvents_.size()>kRulesReplayEventJournalCapacity)
            rulesReplayEvents_.pop_front();
    }
    return ActionResult{};
}

ActionResult GameRuntime::initializeRulesReplayBaseline(
    std::uint64_t revision,const std::string& effectSha256,
    RuntimeRulesReplayEffectVersion effectVersion,
    const ReplayRulesImportToken&)
{
    if(nonphysicalGeneralFoulAuthority_||state_.ballsMoving||
       shotFactCollector_.active()||rulesRackTerminalState_.active||
       mobileDeviceViolationHistory_[0]!=
            rules::MobileDeviceViolationState::None||
       mobileDeviceViolationHistory_[1]!=
            rules::MobileDeviceViolationState::None||
       matchState_.groupChoicePending||breakFoulDecisionPending()||
       eightOnBreakDecisionPending()||ballInHandPlacementPending())
        return ActionResult{false,"replay_rules_baseline_context"};
    GameRuntime candidate=*this;
    candidate.rulesAuthorityRevision_=revision;
    if(candidate.rulesReplayEffectStateHash(effectVersion)!=effectSha256)
        return ActionResult{false,"replay_rules_baseline_mismatch"};
    *this=std::move(candidate);
    return ActionResult{};
}

ActionResult GameRuntime::importRulesReplayEvent(
    const RuntimeRulesReplayEvent& event,
    RuntimeRulesReplayEffectVersion effectVersion,
    const ReplayRulesImportToken&)
{
    if(nonphysicalGeneralFoulAuthority_||
       (!replayDeclarationAuthorityState_&&(pushReviewAuthority_||
        doubleHitCloseCaseAuthority_||frozenBallDeclarationAuthority_||
        frozenCushionDeclarationAuthority_||
        unintentionalBreakMiscueDeclarationAuthority_)))
        return ActionResult{false,"replay_rules_live_authority_forbidden"};
    if(event.tick!=tick_||event.previousRulesRevision!=rulesAuthorityRevision_||
       event.parentEffectSha256!=rulesReplayEffectStateHash(effectVersion))
        return ActionResult{false,"replay_rules_parent_mismatch"};
    GameRuntime candidate=*this;
    candidate.replayRulesImportActive_=true;
    if(event.rackIdentity.empty()||
       (!candidate.replayRulesRackIdentity_.empty()&&
        candidate.replayRulesRackIdentity_!=event.rackIdentity))
        return ActionResult{false,"replay_rules_rack_mismatch"};
    candidate.replayRulesRackIdentity_=event.rackIdentity;
    ActionResult applied;
    if(event.type==RuntimeRulesReplayEventType::
            NonphysicalGeneralFoulCommit){
        rules::NonphysicalGeneralFoulContext context;
        context.offendingPlayer=event.offendingPlayer;
        context.opponentPlayer=event.opponentPlayer;
        applied=candidate.applyCommittedNonphysicalGeneralFoulEffect(
            event.foulKind,context,event.foulOutcome,event.decisionId,
            event.rackIdentity,false);
    }else if(event.type==RuntimeRulesReplayEventType::
            BallInHandPlacementCommit){
        if(!candidate.ballInHandPlacementOffer_)
            return ActionResult{false,"replay_rules_placement_context"};
        const auto offer=*candidate.ballInHandPlacementOffer_;
        rules::BallInHandPlacementRequest request;
        request.rulesetId=rules::kBallInHandRulesetId;
        request.capabilityId=rules::kFreeBallPlacementCapabilityId;
        request.flowId=offer.scope==
                rules::BallInHandPlacementScope::BehindHeadString?
            rules::kBehindHeadStringBallInHandFlowId:
            rules::kFullTableBallInHandFlowId;
        request.decisionId=event.decisionId;
        request.expectedAuthorityStateRevision=
            event.previousRulesRevision;
        request.idempotencyKey="replay.rules."+event.decisionId;
        request.actorPlayerId=event.actorPlayerId;
        request.actionId=event.actionId;
        request.position={event.worldX,event.worldZ};
        const auto result=candidate.submitBallInHandPlacement(request);
        applied=result.ok?ActionResult{}:
            ActionResult{false,"replay_rules_placement_rejected"};
    }else if(event.type==RuntimeRulesReplayEventType::GroupChoiceCommit){
        if(!candidate.groupChoiceDecisionOffer_)
            return ActionResult{false,"replay_rules_group_choice_context"};
        rules::GroupChoiceDecisionRequest request;
        request.rulesetId=rules::kGroupChoiceRulesetId;
        request.capabilityId=rules::kGroupChoiceAuthorityCapabilityId;
        request.flowId=rules::kGroupChoiceAuthorityFlowId;
        request.decisionId=event.decisionId;
        request.expectedAuthorityStateRevision=event.previousRulesRevision;
        request.idempotencyKey="replay.rules."+event.decisionId;
        request.actorPlayerId=event.actorPlayerId;
        request.actionId=event.actionId;
        const auto result=candidate.submitGroupChoiceDecision(request);
        applied=result.ok?ActionResult{}:
            ActionResult{false,"replay_rules_group_choice_rejected"};
    }else if(event.type==RuntimeRulesReplayEventType::BreakFoulChoiceCommit){
        if(event.actionId==rules::kBreakFoulRerackOpponentActionId||
           event.actionId==rules::kBreakFoulRerackOriginalActionId)
            return ActionResult{false,"replay_cross_rack_unsupported"};
        if(!candidate.breakFoulDecisionOffer_)
            return ActionResult{false,"replay_rules_break_foul_context"};
        rules::BreakFoulDecisionRequest request;
        request.rulesetId=rules::kBreakFoulRulesetId;
        request.capabilityId=rules::kBreakFoulCapabilityId;
        request.flowId=rules::kBreakFoulFlowId;
        request.decisionId=event.decisionId;
        request.expectedAuthorityStateRevision=event.previousRulesRevision;
        request.idempotencyKey="replay.rules."+event.decisionId;
        request.actorPlayerId=event.actorPlayerId;
        request.actionId=event.actionId;request.payloadCanonical="{}";
        const auto result=candidate.submitBreakFoulDecision(request);
        applied=result.ok?ActionResult{}:
            ActionResult{false,"replay_rules_break_foul_rejected"};
    }else if(event.type==RuntimeRulesReplayEventType::EightOnBreakChoiceCommit){
        if(event.actionId==rules::kEightOnBreakRerackOpponentActionId||
           event.actionId==rules::kEightOnBreakRerackOriginalActionId)
            return ActionResult{false,"replay_cross_rack_unsupported"};
        if(!candidate.eightOnBreakDecisionOffer_)
            return ActionResult{false,"replay_rules_eight_on_break_context"};
        rules::EightOnBreakDecisionRequest request;
        request.rulesetId=rules::kEightOnBreakRulesetId;
        request.capabilityId=rules::kEightOnBreakCapabilityId;
        request.flowId=rules::kEightOnBreakFlowId;
        request.decisionId=event.decisionId;
        request.expectedAuthorityStateRevision=event.previousRulesRevision;
        request.idempotencyKey="replay.rules."+event.decisionId;
        request.actorPlayerId=event.actorPlayerId;
        request.actionId=event.actionId;request.payloadCanonical="{}";
        const auto result=candidate.submitEightOnBreakDecision(request);
        applied=result.ok?ActionResult{}:
            ActionResult{false,"replay_rules_eight_on_break_rejected"};
    }else if(event.type==RuntimeRulesReplayEventType::
            BehindHeadStringRespotCommit){
        rules::BehindHeadStringRespotRequest request;
        request.rulesetId=rules::kBallInHandRulesetId;
        request.capabilityId=rules::kBehindHeadStringRespotCapabilityId;
        request.flowId=rules::kBehindHeadStringRespotFlowId;
        request.decisionId=event.decisionId;
        request.expectedAuthorityStateRevision=event.previousRulesRevision;
        request.expectedAuthorityStateHash=candidate.rulesAuthorityStateHash();
        request.idempotencyKey="replay.rules."+event.decisionId;
        request.actorPlayerId=event.actorPlayerId;
        request.actionId=event.actionId;
        request.selectedBallId=event.selectedBallId;
        if(event.actionId==rules::kChooseEquidistantRespotBallActionId)
            request.payloadCanonical="{\"selected_ball_id\":"+
                std::to_string(event.selectedBallId)+"}";
        else request.payloadCanonical="{}";
        const auto result=candidate.submitBehindHeadStringRespot(request);
        applied=result.ok?ActionResult{}:
            ActionResult{false,"replay_rules_head_string_respot_rejected"};
    }else if(event.type==RuntimeRulesReplayEventType::
            PushReviewDeclarationCommit){
        rules::PushReviewContext context;
        context.parentRevision=event.previousRulesRevision;
        context.parentStateHash=candidate.rulesAuthorityStateHash();
        if(!candidate.pushReviewAuthority_)
            candidate.pushReviewAuthority_.emplace(nullptr,nullptr);
        candidate.pushReviewAuthority_->restoreCommittedForReplay(
            event.declarationRackIdentity,context,event.proposalDecisionId,
            event.authoritySubjectId);
        candidate.replayDeclarationAuthorityState_=true;
        applied=ActionResult{};
    }else if(event.type==RuntimeRulesReplayEventType::
            DoubleHitCloseCaseDeclarationCommit){
        rules::DoubleHitCloseCaseContext context;
        context.objectBallId=event.declarationObjectBallId;
        context.parentRevision=event.previousRulesRevision;
        context.parentStateHash=candidate.rulesAuthorityStateHash();
        if(!candidate.doubleHitCloseCaseAuthority_)
            candidate.doubleHitCloseCaseAuthority_.emplace(nullptr,nullptr);
        candidate.doubleHitCloseCaseAuthority_->restoreCommittedForReplay(
            event.declarationRackIdentity,context,event.proposalDecisionId,
            event.authoritySubjectId);
        candidate.replayDeclarationAuthorityState_=true;
        applied=ActionResult{};
    }else if(event.type==RuntimeRulesReplayEventType::
            FrozenBallDeclarationCommit){
        rules::FrozenBallDeclarationContext context;
        context.objectBallId=event.declarationObjectBallId;
        context.parentRevision=event.previousRulesRevision;
        context.parentStateHash=candidate.rulesAuthorityStateHash();
        if(!candidate.frozenBallDeclarationAuthority_)
            candidate.frozenBallDeclarationAuthority_.emplace(nullptr,nullptr);
        candidate.frozenBallDeclarationAuthority_->restoreCommittedForReplay(
            event.declarationRackIdentity,context,event.proposalDecisionId,
            event.authoritySubjectId);
        candidate.replayDeclarationAuthorityState_=true;
        applied=ActionResult{};
    }else if(event.type==RuntimeRulesReplayEventType::
            FrozenCushionDeclarationCommit){
        rules::FrozenCushionDeclarationContext context;
        context.objectBallId=event.declarationObjectBallId;
        context.cushionId=static_cast<rules::StraightCushionId>(
            event.declarationCushionId);
        context.parentRevision=event.previousRulesRevision;
        context.parentStateHash=candidate.rulesAuthorityStateHash();
        if(!candidate.frozenCushionDeclarationAuthority_)
            candidate.frozenCushionDeclarationAuthority_.emplace(nullptr,nullptr);
        candidate.frozenCushionDeclarationAuthority_->restoreCommittedForReplay(
            event.declarationRackIdentity,context,event.proposalDecisionId,
            event.authoritySubjectId);
        candidate.replayDeclarationAuthorityState_=true;
        applied=ActionResult{};
    }else if(event.type==RuntimeRulesReplayEventType::
            UnintentionalBreakMiscueDeclarationCommit){
        rules::UnintentionalBreakMiscueDeclarationContext context;
        context.actorPlayerId=event.actorPlayerId;
        context.parentRevision=event.previousRulesRevision;
        context.parentStateHash=candidate.rulesAuthorityStateHash();
        if(!candidate.unintentionalBreakMiscueDeclarationAuthority_)
            candidate.unintentionalBreakMiscueDeclarationAuthority_.emplace(
                nullptr,nullptr);
        candidate.unintentionalBreakMiscueDeclarationAuthority_->
            restoreCommittedForReplay(event.declarationRackIdentity,context,
                event.proposalDecisionId,event.authoritySubjectId);
        candidate.replayDeclarationAuthorityState_=true;
        applied=ActionResult{};
    }else{
        return ActionResult{false,"replay_rules_event_unknown"};
    }
    if(!applied.ok)return applied;
    candidate.replayRulesImportActive_=false;
    if(candidate.rulesAuthorityRevision_!=event.committedRulesRevision||
       candidate.rulesReplayEffectStateHash(effectVersion)!=
            event.committedEffectSha256){
        if(event.type==RuntimeRulesReplayEventType::PushReviewDeclarationCommit)
            return ActionResult{false,"replay_rules_commit_mismatch_push"};
        if(event.type==RuntimeRulesReplayEventType::DoubleHitCloseCaseDeclarationCommit)
            return ActionResult{false,"replay_rules_commit_mismatch_double"};
        if(event.type==RuntimeRulesReplayEventType::FrozenBallDeclarationCommit)
            return ActionResult{false,"replay_rules_commit_mismatch_frozen_ball"};
        if(event.type==RuntimeRulesReplayEventType::FrozenCushionDeclarationCommit)
            return ActionResult{false,"replay_rules_commit_mismatch_frozen_cushion"};
        if(event.type==RuntimeRulesReplayEventType::
                UnintentionalBreakMiscueDeclarationCommit)
            return ActionResult{false,"replay_rules_commit_mismatch_06g"};
        return ActionResult{false,"replay_rules_commit_mismatch"};
    }
    *this=std::move(candidate);
    return ActionResult{};
}

void GameRuntime::appendCommittedDeclarationReplayEvent(
    RuntimeRulesReplayEventType type,
    const rules::RefereeAuthorityRequest& finalRequest,
    const std::string& proposalDecisionId,
    const std::string& rackIdentity,
    const std::string& factSchemaId,
    int objectBallId, int cushionId,
    const std::string& parentEffectSha256,
    int actorPlayerId)
{
    const std::string replayRackIdentity=currentRulesReplayRackIdentity();
    if(replayRulesRackIdentity_.empty())
        replayRulesRackIdentity_=replayRackIdentity;
    RuntimeRulesReplayEvent event;
    event.sequence = nextRulesReplayEventSequence_++;
    event.tick = tick_;
    event.afterAppliedSemanticActionSequence =
        appliedSemanticActions_.empty() ? 0 :
            appliedSemanticActions_.back().sequence;
    event.type = type;
    event.rackIdentity = replayRackIdentity;
    event.decisionId = finalRequest.decisionId;
    event.actionId = finalRequest.declarationKind;
    event.authorityRole = finalRequest.authorityRole;
    event.authoritySubjectId = finalRequest.authoritySubjectId;
    event.proposalDecisionId = proposalDecisionId;
    event.declarationRackIdentity = rackIdentity;
    event.factSchemaId = factSchemaId;
    event.declarationObjectBallId = objectBallId;
    event.declarationCushionId = cushionId;
    event.actorPlayerId = actorPlayerId;
    event.previousRulesRevision = rulesAuthorityRevision_;
    event.committedRulesRevision = rulesAuthorityRevision_;
    event.parentEffectSha256 = parentEffectSha256;
    event.committedEffectSha256 = rulesReplayEffectStateHash();
    rulesReplayEvents_.push_back(std::move(event));
    while (rulesReplayEvents_.size() > kRulesReplayEventJournalCapacity)
        rulesReplayEvents_.pop_front();
}

rules::NonphysicalGeneralFoulResult
GameRuntime::submitNonphysicalGeneralFoulCancel(
    const rules::RefereeAuthorityRequest&r){
    if(!nonphysicalGeneralFoulAuthority_)
        return {false,"nonphysical_general_foul_unconfigured",{}};
    return nonphysicalGeneralFoulAuthority_->cancel(r,tick_);}
const rules::PushReviewState&GameRuntime::pushReviewState()const{
    static const rules::PushReviewState e;return pushReviewAuthority_?
        pushReviewAuthority_->state():e;}
const rules::RefereeAuthorityState&GameRuntime::pushReviewProtocolState()const{
    static const rules::RefereeAuthorityState e;return pushReviewAuthority_?
        pushReviewAuthority_->protocolState():e;}
rules::PushReviewResult GameRuntime::submitPushReviewProposal(
    const rules::RefereeAuthorityRequest&r){
    if(!pushReviewAuthority_)return {false,"push_review_unconfigured",{}};
    if(rulesMode_!=RulesMode::ChineseEightBallV1||state_.ballsMoving||
        shotFactCollector_.active()||matchState_.groupChoicePending||
        breakFoulDecisionPending()||eightOnBreakDecisionPending()||
        ballInHandPlacementPending()||state_.gameOver||state_.balls[0].pocketed||state_.balls[0].outOfPlay)
        return {false,"push_review_parent_context",{}};
    rules::PushReviewContext c;c.parentRevision=rulesAuthorityRevision_;
    c.parentStateHash=rulesAuthorityStateHash();return pushReviewAuthority_->propose(r,c,tick_);}
rules::PushReviewResult GameRuntime::submitPushReviewFinal(
    const rules::RefereeAuthorityRequest&r){
    if(!pushReviewAuthority_)return {false,"push_review_unconfigured",{}};
    auto c=pushReviewAuthority_->state().context;
    if(c.parentRevision!=rulesAuthorityRevision_||c.parentStateHash!=rulesAuthorityStateHash()||
        state_.ballsMoving||shotFactCollector_.active()||state_.balls[0].pocketed||state_.balls[0].outOfPlay)
        return {false,"push_review_parent_state_changed",{}};
    const auto before=pushReviewAuthority_->state();
    const std::string parent=rulesReplayEffectStateHash();
    auto result=pushReviewAuthority_->finalDeclare(r,c,tick_);
    if(result.ok&&before.phase==rules::PushReviewPhase::ProposalOpen)
        appendCommittedDeclarationReplayEvent(
            RuntimeRulesReplayEventType::PushReviewDeclarationCommit,r,
            before.proposalDecisionId,before.rackIdentity,
            "canonical_push_classification_v1",-1,-1,parent);
    return result;}
rules::PushReviewResult GameRuntime::submitPushReviewCancel(
    const rules::RefereeAuthorityRequest&r){if(!pushReviewAuthority_)
    return {false,"push_review_unconfigured",{}};return pushReviewAuthority_->cancel(r,tick_);}

const rules::DoubleHitCloseCaseState&
GameRuntime::doubleHitCloseCaseState() const
{
    static const rules::DoubleHitCloseCaseState empty;
    return doubleHitCloseCaseAuthority_ ?
        doubleHitCloseCaseAuthority_->state() : empty;
}

const rules::RefereeAuthorityState&
GameRuntime::doubleHitCloseCaseProtocolState() const
{
    static const rules::RefereeAuthorityState empty;
    return doubleHitCloseCaseAuthority_ ?
        doubleHitCloseCaseAuthority_->protocolState() : empty;
}

rules::DoubleHitCloseCaseResult GameRuntime::submitDoubleHitCloseCaseProposal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!doubleHitCloseCaseAuthority_)
        return {false,"double_hit_close_case_authority_unconfigured",{}};
    int object=-1;
    for(int ball=1;ball<=15;++ball)
        if(request.payloadCanonical.find("\"object_ball_id\":"+
                std::to_string(ball)+",")!=std::string::npos){object=ball;break;}
    if(rulesMode_!=RulesMode::ChineseEightBallV1||state_.ballsMoving||
        shotFactCollector_.active()||matchState_.groupChoicePending||
        breakFoulDecisionPending()||eightOnBreakDecisionPending()||
        ballInHandPlacementPending()||state_.gameOver||state_.balls[0].pocketed||
        state_.balls[0].outOfPlay||object<1||state_.balls[object].pocketed||state_.balls[object].outOfPlay||
        !legalFrozenObject(matchState_,object))
        return {false,"double_hit_close_case_parent_context",{}};
    rules::DoubleHitCloseCaseContext context;
    context.objectBallId=object;context.parentRevision=rulesAuthorityRevision_;
    context.parentStateHash=rulesAuthorityStateHash();
    return doubleHitCloseCaseAuthority_->propose(request,context,tick_);
}

rules::DoubleHitCloseCaseResult GameRuntime::submitDoubleHitCloseCaseFinal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!doubleHitCloseCaseAuthority_)
        return {false,"double_hit_close_case_authority_unconfigured",{}};
    const auto context=doubleHitCloseCaseAuthority_->state().context;
    if(context.parentRevision!=rulesAuthorityRevision_||
        context.parentStateHash!=rulesAuthorityStateHash()||state_.ballsMoving||
        shotFactCollector_.active()||state_.balls[0].pocketed||
        state_.balls[0].outOfPlay||context.objectBallId<1||state_.balls[context.objectBallId].pocketed||state_.balls[context.objectBallId].outOfPlay||
        !legalFrozenObject(matchState_,context.objectBallId))
        return {false,"double_hit_close_case_parent_state_changed",{}};
    const auto before=doubleHitCloseCaseAuthority_->state();
    const std::string parent=rulesReplayEffectStateHash();
    auto result=doubleHitCloseCaseAuthority_->finalDeclare(request,context,tick_);
    if(result.ok&&before.phase==rules::DoubleHitCloseCasePhase::ProposalOpen)
        appendCommittedDeclarationReplayEvent(
            RuntimeRulesReplayEventType::DoubleHitCloseCaseDeclarationCommit,
            request,before.proposalDecisionId,before.rackIdentity,
            "canonical_double_hit_classification_v1",context.objectBallId,
            -1,parent);
    return result;
}

rules::DoubleHitCloseCaseResult GameRuntime::submitDoubleHitCloseCaseCancel(
    const rules::RefereeAuthorityRequest& request)
{
    if (!doubleHitCloseCaseAuthority_)
        return {false,"double_hit_close_case_authority_unconfigured",{}};
    return doubleHitCloseCaseAuthority_->cancel(request,tick_);
}

const rules::FrozenCushionDeclarationState&
GameRuntime::frozenCushionDeclarationState() const
{
    static const rules::FrozenCushionDeclarationState empty;
    return frozenCushionDeclarationAuthority_ ?
        frozenCushionDeclarationAuthority_->state() : empty;
}

const rules::RefereeAuthorityState&
GameRuntime::frozenCushionDeclarationProtocolState() const
{
    static const rules::RefereeAuthorityState empty;
    return frozenCushionDeclarationAuthority_ ?
        frozenCushionDeclarationAuthority_->protocolState() : empty;
}

namespace {
std::optional<rules::StraightCushionId> requestedStraightCushion(
    const std::string& payload)
{
    for (const auto id : {rules::StraightCushionId::Head,
             rules::StraightCushionId::Foot,
             rules::StraightCushionId::LeftLong,
             rules::StraightCushionId::RightLong})
        if (payload.find(std::string("\"straight_cushion_id\":\"") +
                rules::straightCushionIdName(id) + "\"") !=
            std::string::npos)
            return id;
    return std::nullopt;
}
}

rules::FrozenCushionDeclarationResult
GameRuntime::submitFrozenCushionDeclarationProposal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!frozenCushionDeclarationAuthority_)
        return {false,"frozen_cushion_authority_unconfigured",{}};
    int object = -1;
    for (int ball = 1; ball <= 15; ++ball)
        if (request.payloadCanonical.find(
                "\"object_ball_id\":" + std::to_string(ball) + ",") !=
            std::string::npos) {
            object = ball;
            break;
        }
    const auto cushion = requestedStraightCushion(request.payloadCanonical);
    if (rulesMode_ != RulesMode::ChineseEightBallV1 || state_.ballsMoving ||
        shotFactCollector_.active() || matchState_.groupChoicePending ||
        breakFoulDecisionPending() || eightOnBreakDecisionPending() ||
        ballInHandPlacementPending() || state_.gameOver || object < 1 ||
        state_.balls[object].pocketed || state_.balls[object].outOfPlay || !cushion ||
        !legalFrozenObject(matchState_, object))
        return {false,"frozen_cushion_parent_context",{}};
    rules::FrozenCushionDeclarationContext context;
    context.objectBallId = object;
    context.cushionId = *cushion;
    context.parentRevision = rulesAuthorityRevision_;
    context.parentStateHash = rulesAuthorityStateHash();
    return frozenCushionDeclarationAuthority_->propose(
        request, context, tick_);
}

rules::FrozenCushionDeclarationResult
GameRuntime::submitFrozenCushionDeclarationFinal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!frozenCushionDeclarationAuthority_)
        return {false,"frozen_cushion_authority_unconfigured",{}};
    const auto context = frozenCushionDeclarationAuthority_->state().context;
    if (context.parentRevision != rulesAuthorityRevision_ ||
        context.parentStateHash != rulesAuthorityStateHash() ||
        state_.ballsMoving || shotFactCollector_.active() ||
        context.objectBallId < 1 ||
        state_.balls[context.objectBallId].pocketed || state_.balls[context.objectBallId].outOfPlay ||
        !legalFrozenObject(matchState_, context.objectBallId))
        return {false,"frozen_cushion_parent_state_changed",{}};
    const auto before=frozenCushionDeclarationAuthority_->state();
    const std::string parent=rulesReplayEffectStateHash();
    auto result=frozenCushionDeclarationAuthority_->finalDeclare(
        request, context, tick_);
    if(result.ok&&before.phase==
            rules::FrozenCushionDeclarationPhase::ProposalOpen)
        appendCommittedDeclarationReplayEvent(
            RuntimeRulesReplayEventType::FrozenCushionDeclarationCommit,
            request,before.proposalDecisionId,before.rackIdentity,
            "canonical_frozen_cushion_shot_observation_v1",
            context.objectBallId,static_cast<int>(context.cushionId),parent);
    return result;
}

rules::FrozenCushionDeclarationResult
GameRuntime::submitFrozenCushionDeclarationCancel(
    const rules::RefereeAuthorityRequest& request)
{
    if (!frozenCushionDeclarationAuthority_)
        return {false,"frozen_cushion_authority_unconfigured",{}};
    return frozenCushionDeclarationAuthority_->cancel(request, tick_);
}

const rules::FrozenBallDeclarationState&
GameRuntime::frozenBallDeclarationState() const
{
    static const rules::FrozenBallDeclarationState empty;
    return frozenBallDeclarationAuthority_ ?
        frozenBallDeclarationAuthority_->state() : empty;
}

const rules::RefereeAuthorityState&
GameRuntime::frozenBallDeclarationProtocolState() const
{
    static const rules::RefereeAuthorityState empty;
    return frozenBallDeclarationAuthority_ ?
        frozenBallDeclarationAuthority_->protocolState() : empty;
}

namespace {
bool legalFrozenObject(const rules::MatchState& state, int ball)
{
    if (ball < 1 || ball > 15) return false;
    if (state.phase == rules::MatchPhase::Break ||
        state.phase == rules::MatchPhase::OpenTable)
        return ball != 8;
    const auto group = state.groups[state.currentPlayer];
    return group == rules::BallGroup::Solids ? ball <= 7 :
        group == rules::BallGroup::Stripes ? ball >= 9 && ball <= 15 :
        false;
}
}

rules::FrozenBallDeclarationResult
GameRuntime::submitFrozenBallDeclarationProposal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!frozenBallDeclarationAuthority_)
        return {false,"frozen_declaration_authority_unconfigured",{}};
    const int object = [&] {
        for (int ball=1; ball<=15; ++ball)
            if (request.payloadCanonical.find(
                "\"object_ball_id\":"+std::to_string(ball)+",") !=
                std::string::npos) return ball;
        return -1;
    }();
    if (rulesMode_ != RulesMode::ChineseEightBallV1 || state_.ballsMoving ||
        shotFactCollector_.active() || matchState_.groupChoicePending ||
        breakFoulDecisionPending() || eightOnBreakDecisionPending() ||
        ballInHandPlacementPending() || state_.gameOver ||
        state_.balls[0].pocketed || state_.balls[0].outOfPlay || object < 1 ||
        state_.balls[object].pocketed || state_.balls[object].outOfPlay ||
        !legalFrozenObject(matchState_,object))
        return {false,"frozen_declaration_parent_context",{}};
    rules::FrozenBallDeclarationContext context;
    context.objectBallId=object;
    context.parentRevision=rulesAuthorityRevision_;
    context.parentStateHash=rulesAuthorityStateHash();
    return frozenBallDeclarationAuthority_->propose(request,context,tick_);
}

rules::FrozenBallDeclarationResult
GameRuntime::submitFrozenBallDeclarationFinal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!frozenBallDeclarationAuthority_)
        return {false,"frozen_declaration_authority_unconfigured",{}};
    const auto context=frozenBallDeclarationAuthority_->state().context;
    if (context.parentRevision!=rulesAuthorityRevision_ ||
        context.parentStateHash!=rulesAuthorityStateHash() ||
        state_.ballsMoving || shotFactCollector_.active() ||
        state_.balls[0].pocketed || state_.balls[0].outOfPlay || context.objectBallId<1 ||
        state_.balls[context.objectBallId].pocketed || state_.balls[context.objectBallId].outOfPlay ||
        !legalFrozenObject(matchState_,context.objectBallId))
        return {false,"frozen_declaration_parent_state_changed",{}};
    const auto before=frozenBallDeclarationAuthority_->state();
    const std::string parent=rulesReplayEffectStateHash();
    auto result=frozenBallDeclarationAuthority_->finalDeclare(
        request,context,tick_);
    if(result.ok&&before.phase==rules::FrozenBallDeclarationPhase::ProposalOpen)
        appendCommittedDeclarationReplayEvent(
            RuntimeRulesReplayEventType::FrozenBallDeclarationCommit,
            request,before.proposalDecisionId,before.rackIdentity,
            "frozen_cue_object_declaration_v1",context.objectBallId,-1,
            parent);
    return result;
}

rules::FrozenBallDeclarationResult
GameRuntime::submitFrozenBallDeclarationCancel(
    const rules::RefereeAuthorityRequest& request)
{
    if (!frozenBallDeclarationAuthority_)
        return {false,"frozen_declaration_authority_unconfigured",{}};
    return frozenBallDeclarationAuthority_->cancel(request,tick_);
}

const rules::GroupConfusionState& GameRuntime::groupConfusionState() const
{
    static const rules::GroupConfusionState empty;
    return groupConfusionAuthority_ ? groupConfusionAuthority_->state() :
        empty;
}

const rules::RefereeAuthorityState&
GameRuntime::groupConfusionProtocolState() const
{
    static const rules::RefereeAuthorityState empty;
    return groupConfusionAuthority_ ?
        groupConfusionAuthority_->protocolState() : empty;
}

rules::GroupConfusionResult GameRuntime::submitGroupConfusionProposal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!groupConfusionAuthority_)
        return rules::GroupConfusionResult{false,
            "group_confusion_authority_unconfigured", {}};
    if (rulesMode_ != RulesMode::ChineseEightBallV1)
        return rules::GroupConfusionResult{false,
            "ruleset_not_enabled", {}};
    if (state_.ballsMoving || shotFactCollector_.active())
        return rules::GroupConfusionResult{false,
            "group_confusion_invalid_state", {}};
    return groupConfusionAuthority_->propose(request, tick_);
}

rules::GroupConfusionResult GameRuntime::submitGroupConfusionFinal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!groupConfusionAuthority_)
        return rules::GroupConfusionResult{false,
            "group_confusion_authority_unconfigured", {}};
    const auto before = groupConfusionAuthority_->state();
    if (before.rerackCommitted)
        return groupConfusionAuthority_->finalDeclare(request, tick_);
    rules::RerackFoundationParameters parameters;
    parameters.breakerPlayerId = before.window.originalBreakerPlayerId;
    const rules::RackFoundationPlan plan =
        rules::buildRerackFoundationPlan(state_, physicsProfile_,
            parameters, rulesAuthorityRevision_,
            rulesAuthorityStateHash());
    if (!plan.ok)
        return rules::GroupConfusionResult{false, plan.errorCode, {}};
    const rules::GroupConfusionResult authorized =
        groupConfusionAuthority_->finalDeclare(request, tick_);
    if (!authorized.ok) return authorized;
    groupConfusionAuthority_->markRerackCommitting();
    groupConfusionRerackCommitInProgress_ = true;
    applyVerifiedRerackFoundation(plan);
    groupConfusionRerackCommitInProgress_ = false;
    ++rulesAuthorityRevision_;
    state_.players.currentPlayer = before.window.originalShootingOrder[0];
    state_.players.nextPlayer = before.window.originalShootingOrder[1];
    matchState_.currentPlayer = before.window.originalShootingOrder[0];
    groupConfusionAuthority_->markRerackCommitted();
    events_.push_back(RuntimeEvent{nextSequence_++, tick_,
        "rules_group_confusion_rerack_committed"});
    return authorized;
}

const rules::StalemateState& GameRuntime::stalemateState() const
{
    static const rules::StalemateState empty;
    return stalemateAuthority_ ? stalemateAuthority_->state() : empty;
}

const rules::RefereeAuthorityState& GameRuntime::stalemateProtocolState() const
{
    static const rules::RefereeAuthorityState empty;
    return stalemateAuthority_ ? stalemateAuthority_->protocolState() : empty;
}

rules::StalemateResult GameRuntime::submitStalemateProposal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!stalemateAuthority_)
        return rules::StalemateResult{false,
            "stalemate_authority_unconfigured", {}};
    return stalemateAuthority_->propose(request, tick_);
}

rules::StalemateResult GameRuntime::submitStalemateRefusal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!stalemateAuthority_)
        return rules::StalemateResult{false,
            "stalemate_authority_unconfigured", {}};
    return stalemateAuthority_->refuse(request, tick_);
}

rules::StalemateResult GameRuntime::submitStalemateCancel(
    const rules::RefereeAuthorityRequest& request)
{
    if (!stalemateAuthority_)
        return rules::StalemateResult{false,
            "stalemate_authority_unconfigured", {}};
    return stalemateAuthority_->cancel(request, tick_);
}

rules::StalemateResult GameRuntime::submitStalemateFinal(
    const rules::RefereeAuthorityRequest& request)
{
    if (!stalemateAuthority_)
        return rules::StalemateResult{false,
            "stalemate_authority_unconfigured", {}};
    const auto protocolBefore = stalemateAuthority_->state();
    if (protocolBefore.rerackCommitted)
        return stalemateAuthority_->finalDeclare(request, tick_);
    rules::RerackFoundationParameters parameters;
    parameters.breakerPlayerId =
        protocolBefore.originalBreakerPlayerId;
    const rules::RackFoundationPlan plan =
        rules::buildRerackFoundationPlan(state_, physicsProfile_,
            parameters, rulesAuthorityRevision_,
            rulesAuthorityStateHash());
    if (!plan.ok)
        return rules::StalemateResult{false,
            plan.errorCode, {}};
    const rules::StalemateResult authorized =
        stalemateAuthority_->finalDeclare(request, tick_);
    if (!authorized.ok) return authorized;
    stalemateRerackCommitInProgress_ = true;
    applyVerifiedRerackFoundation(plan);
    stalemateRerackCommitInProgress_ = false;
    ++rulesAuthorityRevision_;
    state_.players.currentPlayer =
        protocolBefore.originalShootingOrder[0];
    state_.players.nextPlayer =
        protocolBefore.originalShootingOrder[1];
    matchState_.currentPlayer =
        protocolBefore.originalShootingOrder[0];
    stalemateAuthority_->markRerackCommitted();
    events_.push_back(RuntimeEvent{nextSequence_++, tick_,
        "rules_stalemate_rerack_committed"});
    return authorized;
}

void GameRuntime::reset()
{
    state_ = GameState{};
    initializeBalls(state_);
    tick_ = 0;
    nextSequence_ = 1;
    ++eventJournalGeneration_;
    ++stalemateRackIdentityOrdinal_;
    ++groupConfusionRackIdentityOrdinal_;
    ++frozenBallDeclarationRackIdentityOrdinal_;
    ++frozenCushionDeclarationRackIdentityOrdinal_;
    ++doubleHitCloseCaseRackIdentityOrdinal_;
    ++pushReviewRackIdentityOrdinal_;
    ++nonphysicalGeneralFoulRackIdentityOrdinal_;
    ++unintentionalBreakMiscueRackIdentityOrdinal_;
    events_.clear();
    nextPhysicsStepAttemptSequence_ = 1;
    recentPhysicsSteps_.clear();
    nextAppliedSemanticSequence_ = 1;
    ++appliedSemanticActionJournalGeneration_;
    appliedSemanticActions_.clear();
    nextRulesAuthorityActionSequence_ = 1;
    rulesAuthorityActions_.clear();
    refereeAuthorityProtocol_.reset("runtime.rack." +
        std::to_string(eventJournalGeneration_));
    rackOriginalBreakerPlayerId_ = 0;
    rackOriginalShootingOrder_ = {0, 1};
    if (stalemateAuthority_)
        stalemateAuthority_->reset("runtime.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(stalemateRackIdentityOrdinal_),
            rackOriginalBreakerPlayerId_, rackOriginalShootingOrder_);
    if (groupConfusionAuthority_)
        groupConfusionAuthority_->reset(
            "runtime.group_confusion.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(groupConfusionRackIdentityOrdinal_),
            rackOriginalBreakerPlayerId_, rackOriginalShootingOrder_);
    groupConfusionCompletedShotOrdinal_ = 0;
    if (frozenBallDeclarationAuthority_)
        frozenBallDeclarationAuthority_->reset(
            "runtime.frozen_declaration.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(frozenBallDeclarationRackIdentityOrdinal_));
    nextFrozenBallDeclarationShotIdentity_ = 1;
    if (frozenCushionDeclarationAuthority_)
        frozenCushionDeclarationAuthority_->reset(
            "runtime.frozen_cushion.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(frozenCushionDeclarationRackIdentityOrdinal_));
    nextFrozenCushionDeclarationShotIdentity_ = 1;
    if(doubleHitCloseCaseAuthority_)
        doubleHitCloseCaseAuthority_->reset("runtime.double_hit_close.rack."+
            std::to_string(eventJournalGeneration_)+"."+
            std::to_string(doubleHitCloseCaseRackIdentityOrdinal_));
    nextDoubleHitCloseCaseShotIdentity_=1;
    if(pushReviewAuthority_)pushReviewAuthority_->reset("runtime.push_review.rack."+
        std::to_string(eventJournalGeneration_)+"."+std::to_string(pushReviewRackIdentityOrdinal_));
    nextPushReviewShotIdentity_=1;
    if(nonphysicalGeneralFoulAuthority_)
        nonphysicalGeneralFoulAuthority_->reset(
            "runtime.nonphysical_foul.rack."+
            std::to_string(eventJournalGeneration_)+"."+
            std::to_string(nonphysicalGeneralFoulRackIdentityOrdinal_));
    if (unintentionalBreakMiscueDeclarationAuthority_)
        unintentionalBreakMiscueDeclarationAuthority_->reset(
            "runtime.unintentional_break_miscue.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(unintentionalBreakMiscueRackIdentityOrdinal_));
    physicsTraceEnabled_ = false;
    physicsTrace_.clear();
    hasCueImpactInput_ = false;
    cueImpactInput_ = CueImpactInput{};
    hasCueContactResult_ = false;
    cueContactResult_ = CueContactResult{};
    cueContactPending_ = false;
    hasCanonicalCueTipStrokeObservation_ = false;
    canonicalCueTipStrokeObservationPending_ = false;
    canonicalCueTipStrokeObservation_ = CanonicalCueTipStrokeObservation{};
    canonicalCueTipStrokeObserver_.rollback();
    d5BShotObserver_.rollback();
    hasCanonicalCueObjectOrderObservation_=false;
    canonicalCueObjectOrderObservation_=CanonicalCueObjectOrderObservation{};
    hasCanonicalFrozenCushionShotObservation_=false;
    canonicalFrozenCushionShotObservation_=
        CanonicalFrozenCushionShotObservation{};
    boundaryMode_ = PhysicsBoundaryMode::ProductionTable;
    updateCameraFromCueBall(state_);
    resetPresentationModels();
    resetRulesState();
}

ActionResult GameRuntime::dispatch(const GameAction& action)
{
    switch (action.type) {
    case ActionType::KeyUp:
        return ActionResult{};
    case ActionType::KeyDown: {
        const unsigned char key = static_cast<unsigned char>(action.firstInt);
        if (key == 'h' || key == 'H') handleHelpKey(state_);
        else if (key == '\t') {
            GameAction toggle;
            toggle.type = ActionType::ToggleAim;
            return dispatch(toggle);
        }
        else if (key == 'c' || key == 'C') handleCameraAnchorToggleKey(state_);
        else if (key == ' ') handleCameraReturnToCueBallKey(state_);
        else if (key == 'w' || key == 'W') { state_.camera.zoom = std::fmax(10.0f, state_.camera.zoom - 10.0f); updateCameraEye(state_); }
        else if (key == 's' || key == 'S') { state_.camera.zoom = std::fmin(500.0f, state_.camera.zoom + 10.0f); updateCameraEye(state_); }
        else if (key == 'a' || key == 'A') state_.camera.panX = std::fmin(490.0f, state_.camera.panX + 10.0f);
        else if (key == 'd' || key == 'D') state_.camera.panX = std::fmax(-490.0f, state_.camera.panX - 10.0f);
        else if ((key == '+' || key == '-') && state_.aim.mode == AimMode::Aim) handleMouseWheel(state_, key == '+' ? 1 : -1, 10.0f, 20.0f, 200.0f);
        else return ActionResult{false, "unknown_key"};
        syncShotControlFromLegacyState();
        return ActionResult{};
    }
    case ActionType::SpecialKey:
        handleSpecialKey(state_, 0, 1, 2, 3, action.firstInt);
        syncShotControlFromLegacyState();
        return ActionResult{};
    case ActionType::MouseMove:
        handleMouseMove(state_, action.firstInt, action.secondInt);
        syncShotControlFromLegacyState();
        return ActionResult{};
    case ActionType::MouseWheel:
        handleMouseWheel(state_, action.firstInt, 10.0f, 20.0f, 200.0f);
        syncShotControlFromLegacyState();
        return ActionResult{};
    case ActionType::MouseButton: {
        const MouseButton button = action.firstInt == 0 ? MouseButton::Left : action.firstInt == 1 ? MouseButton::Right : MouseButton::Other;
        handleMouseButton(state_, button, action.secondInt == 0 ? ButtonState::Down : ButtonState::Up, action.thirdInt, static_cast<int>(action.first));
        syncShotControlFromLegacyState();
        if (state_.input.hitRequested) return applyShot();
        return ActionResult{};
    }
    case ActionType::Resize:
        if (action.firstInt <= 0 || action.secondInt <= 0) return ActionResult{false, "invalid_argument"};
        state_.config.width = action.firstInt; state_.config.height = action.secondInt; return ActionResult{};
    case ActionType::OrbitCamera:
        state_.camera.angleX += action.first; state_.camera.angleY += action.second; clampCameraAngles(state_); updateCameraEye(state_);
        { camera::UpdateInput input; input.orbitYawDeltaRadians = action.first; input.orbitPitchDeltaRadians = action.second; cameraStateMachine_.update(0.0f, cameraSnapshot(state_, cameraStateMachine_.presentationPreferences().m4CameraCandidate), input); }
        return ActionResult{};
    case ActionType::PanCamera:
        state_.camera.anchorMode = CameraAnchorMode::FreeLook; state_.camera.target[0] += action.first; state_.camera.target[2] += action.second; updateCameraEye(state_);
        { camera::UpdateInput input; input.panXDeltaCm = action.first; input.panZDeltaCm = action.second; cameraStateMachine_.update(0.0f, cameraSnapshot(state_, cameraStateMachine_.presentationPreferences().m4CameraCandidate), input); }
        return ActionResult{};
    case ActionType::ZoomCamera:
        state_.camera.zoom = std::fmax(10.0f, std::fmin(500.0f, state_.camera.zoom + action.first)); updateCameraEye(state_);
        { camera::UpdateInput input; input.zoomDeltaCm = action.first; cameraStateMachine_.update(0.0f, cameraSnapshot(state_, cameraStateMachine_.presentationPreferences().m4CameraCandidate), input); }
        return ActionResult{};
    case ActionType::SetAimYaw:
        if (!std::isfinite(action.first)) return ActionResult{false, "invalid_argument"};
        state_.aim.yaw = action.first;
        syncShotControlFromLegacyState();
        recordAppliedSemanticAction(
            AppliedSemanticActionType::SetAimYaw, action.first);
        return ActionResult{};
    case ActionType::SetShotPower:
        if (!std::isfinite(action.first) || action.first < 0.0f || action.first > 200.0f)
            return ActionResult{false, "invalid_argument"};
        state_.input.shotPower = action.first;
        syncShotControlFromLegacyState();
        recordAppliedSemanticAction(
            AppliedSemanticActionType::SetShotPower, action.first);
        return ActionResult{};
    case ActionType::ToggleAim:
        handleAimToggleKey(state_);
        if (state_.aim.mode == AimMode::Aim) {
            // Use the camera actually presented to the player, not the legacy
            // camera, which can disagree after semantic camera transitions.
            const float yaw = cameraStateMachine_.frame().pose.yawRadians;
            state_.aim.yaw = std::atan2(-std::sin(yaw), -std::cos(yaw));
        }
        syncShotControlFromLegacyState();
        cameraStateMachine_.transitionTo(
            state_.aim.mode == AimMode::Aim ? camera::SemanticState::AimPlayer : camera::SemanticState::ObserveFree,
            state_.aim.mode == AimMode::Aim ? camera::Preset::PlayerLow : camera::Preset::Elevated,
            cameraSnapshot(state_, cameraStateMachine_.presentationPreferences().m4CameraCandidate), true);
        return ActionResult{};
    case ActionType::ToggleHelp:
        handleHelpKey(state_);
        return ActionResult{};
    case ActionType::ToggleCameraAnchor:
        handleCameraAnchorToggleKey(state_);
        return ActionResult{};
    case ActionType::ReturnToCueBall:
        handleCameraReturnToCueBallKey(state_);
        return ActionResult{};
    case ActionType::Shoot:
        if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
        {
            const ActionResult result = applyShot();
            if (result.ok)
                recordAppliedSemanticAction(AppliedSemanticActionType::Shoot);
            return result;
        }
    default:
        return ActionResult{false, "unsupported_action"};
    }
}

ActionResult GameRuntime::applyShot()
{
    if (rulesRackTerminalState_.active)
        return ActionResult{false, "rules_rack_complete"};
    if (rulesMode_ == RulesMode::ChineseEightBallV1 &&
        (matchState_.groupChoicePending || breakFoulDecisionPending() ||
         eightOnBreakDecisionPending()))
        return ActionResult{false, pendingRulesDecisionError()};
    syncShotControlFromLegacyState();
    const ShotControlConversion converted = convertShotControls(
        shotControl_, shotControlCapabilities_, physicsProfile_);
    if (!converted.ok)
        return ActionResult{false, stableShotControlError(converted.error)};
    return applyCueImpact(converted.command.cueImpact);
}

ActionResult GameRuntime::step(int count)
{
    return stepInternal(count, true);
}

ActionResult GameRuntime::stepIsolated(int count)
{
    return stepInternal(count, false);
}

ActionResult GameRuntime::stepInternal(int count, bool publishEvents)
{
    if (count < 0) return ActionResult{false, "invalid_argument"};
    if (!rulesShotCompletionError_.empty())
        return ActionResult{false, rulesShotCompletionError_.c_str()};
    if (!tableDepartureObservationError_.empty())
        return ActionResult{false, "table_departure_observation_incomplete"};
    for (int i = 0; i < count; ++i) {
        const GameState before = state_;
        const float timeStep = physicsProfile_.solver.timeStepSeconds;
        std::vector<PhysicsPlaneObservationRequest> planeObservations;
        if (rulesMode_ == RulesMode::ChineseEightBallV1 &&
            shotFactCollector_.active() &&
            behindHeadStringFirstContactContext_) {
            PhysicsPlaneObservationRequest request;
            request.observationId =
                behindHeadStringFirstContactContext_->observationId;
            request.ballIndex = 0;
            request.axis = PhysicsPlaneAxis::Z;
            request.planeCoordinateCm =
                behindHeadStringFirstContactContext_->headStringWorldZCm;
            request.direction =
                PhysicsPlaneCrossingDirection::NegativeToPositive;
            planeObservations.push_back(request);
        }
        if(d5BShotObserver_.active()&&d5BShotObserver_.hasFrozenObservation()){
            const auto& binding=d5BShotObserver_.frozenBinding();
            PhysicsPlaneObservationRequest request;
            request.observationId=binding.planeObservationId;
            request.ballIndex=binding.declaredObjectBallId;
            request.axis=binding.planeAxis;
            request.planeCoordinateCm=binding.contactPlaneCoordinateCm;
            request.direction=binding.departureDirection;
            request.allowStartOnPlaneDeparture=true;
            planeObservations.push_back(request);
        }
        PhysicsStepTelemetry telemetry = updatePhysics(
            state_, timeStep, physicsProfile_, boundaryMode_,
            planeObservations);
        if (telemetry.stepStatus == PhysicsStepStatus::Failed) {
            if (physicsTraceEnabled_) {
                PhysicsFrame frame = capturePhysicsFrame(
                    tick_ + 1, static_cast<double>(tick_ + 1) * timeStep,
                    timeStep, before, state_, telemetry, physicsProfile_,
                    boundaryMode_);
                frame.hasCueImpactInput = hasCueImpactInput_;
                frame.cueImpactInput = cueImpactInput_;
                frame.hasCueContactResult = cueContactPending_;
                frame.cueContactResult = cueContactResult_;
                frame.hasCanonicalCueTipStrokeObservation =
                    canonicalCueTipStrokeObservationPending_;
                frame.canonicalCueTipStrokeObservation =
                    canonicalCueTipStrokeObservation_;
                physicsTrace_.append(frame);
            }
            // A §6(d) sample is frozen when the break release is accepted,
            // but history is committed only with a completed rules shot.
            // Failed physics therefore abandons the staged sample exactly.
            pendingBreakHeadStringRelease_.reset();
            canonicalCueTipStrokeObserver_.rollback();
            d5BShotObserver_.rollback();
            tableDepartureObserver_.rollback();
            hasCanonicalCueTipStrokeObservation_=false;
            canonicalCueTipStrokeObservationPending_=false;
            recordPhysicsStep(std::move(telemetry), tick_ + 1, publishEvents);
            return ActionResult{false,
                physicsFailureCodeName(telemetry.failureCode)};
        }
        ++tick_;
        if (tableDepartureObserver_.active()) {
            rules::TableDepartureTelemetryBridgeContext context;
            context.physicsTick = tick_;
            context.solverProfileId = physicsProfile_.id;
            context.boundaryMode = boundaryMode_;
            context.activeOutsideLandingSurfaceIds = activeOutsideLandingSurfaceIds_;
            context.successfulStep = telemetry.stepStatus != PhysicsStepStatus::Failed;
            context.complete = context.successfulStep;
            auto activeBefore = before;
            auto activeAfter = state_;
            for (int ball = 0; ball < kBallCount; ++ball) {
                if (activeBefore.balls[ball].outOfPlay) activeBefore.balls[ball].pocketed = true;
                if (activeAfter.balls[ball].outOfPlay) activeAfter.balls[ball].pocketed = true;
            }
            const auto adapted = rules::adaptPhysicsStepTelemetryTick(
                telemetry, activeBefore, activeAfter, physicsProfile_, context);
            if (!adapted.complete || !tableDepartureObserver_.consumeSuccessfulPhysicsTick(adapted.tick)) {
                tableDepartureObservationError_ = adapted.complete ?
                    "observer_rejected_native_tick" : adapted.error;
                if (publishEvents) events_.push_back(RuntimeEvent{nextSequence_++, tick_,
                    "rules_table_departure_observation_incomplete"});
                recordPhysicsStep(std::move(telemetry), tick_, publishEvents);
                return ActionResult{false, "table_departure_observation_incomplete"};
            }
            for (const auto& event : adapted.tick.events) {
                if (event.kind == rules::CanonicalTableDepartureEventKind::OutsideLanding)
                    activeOutsideLandingSurfaceIds_[event.ballId] = event.landingSurfaceId;
                else if (event.kind == rules::CanonicalTableDepartureEventKind::PocketCapture ||
                         event.kind == rules::CanonicalTableDepartureEventKind::AdministrativeReset ||
                         event.kind == rules::CanonicalTableDepartureEventKind::ClothReturn)
                    activeOutsideLandingSurfaceIds_[event.ballId] = -1;
            }
            for (int ball = 0; ball < kBallCount; ++ball)
                if (before.balls[ball].trajectoryGeneration != state_.balls[ball].trajectoryGeneration)
                    activeOutsideLandingSurfaceIds_[ball] = -1;
        }
        if(d5BShotObserver_.active()){
            const double tickStart=static_cast<double>(
                tick_-d5BShotObserver_.firstPhysicsTick())*timeStep;
            if(!d5BShotObserver_.consumeSuccessfulPhysicsTick(
                    telemetry,tick_,tickStart))d5BShotObserver_.rollback();
        }
        // A modeled miscue is an accepted stroke attempt with no applied
        // impulse, so physics truthfully remains stationary and cannot emit
        // its usual moving-to-stopped shot boundary.  Complete that shot only
        // after one successful physics tick has reached the native collector;
        // no contact or motion evidence is synthesized here.
        if (rulesMode_ == RulesMode::ChineseEightBallV1 &&
            shotFactCollector_.active() && hasCueContactResult_ &&
            !cueContactResult_.applied &&
            cueContactResult_.regime == CueContactRegime::Miscue &&
            before.players.shotTaken && !before.ballsMoving &&
            !state_.ballsMoving)
            state_.events.shotEnded = true;
        if (rulesMode_ == RulesMode::ChineseEightBallV1 &&
            shotFactCollector_.active()) {
            shotFactCollector_.consume(before, state_, telemetry, tick_);
            if (state_.events.shotEnded) finishRulesShot(publishEvents);
        }
        if (physicsTraceEnabled_) {
            PhysicsFrame frame = capturePhysicsFrame(
                tick_, static_cast<double>(tick_) * timeStep,
                timeStep, before, state_, telemetry, physicsProfile_, boundaryMode_);
            frame.hasCueImpactInput = hasCueImpactInput_;
            frame.cueImpactInput = cueImpactInput_;
            frame.hasCueContactResult = cueContactPending_;
            frame.cueContactResult = cueContactResult_;
            frame.hasCanonicalCueTipStrokeObservation =
                canonicalCueTipStrokeObservationPending_;
            frame.canonicalCueTipStrokeObservation =
                canonicalCueTipStrokeObservation_;
            physicsTrace_.append(frame);
            cueContactPending_ = false;
        }
        if(canonicalCueTipStrokeObservationPending_)
            hasCanonicalCueTipStrokeObservation_=true;
        canonicalCueTipStrokeObservationPending_=false;
        if (publishEvents) recordEvents();
        if (rulesMode_ == RulesMode::Legacy &&
            (state_.events.shotEnded ||
                (!state_.transitionPerspective && state_.players.shotTaken)))
            updatePlayerAfterShot(state_);
        if (state_.camera.anchorMode == CameraAnchorMode::FollowCueBall)
            updateCameraFromCueBall(state_);
        if (state_.events.shotEnded) {
            if(d5BShotObserver_.active()){
                const auto validation=d5BShotObserver_.finish(true);
                if(validation.valid&&validation.complete){
                    canonicalCueObjectOrderObservation_=
                        d5BShotObserver_.cueObjectObservation();
                    hasCanonicalCueObjectOrderObservation_=true;
                    if(d5BShotObserver_.hasFrozenObservation()){
                        canonicalFrozenCushionShotObservation_=
                            d5BShotObserver_.frozenObservation();
                        hasCanonicalFrozenCushionShotObservation_=true;
                    }
                }else d5BShotObserver_.rollback();
            }
            cameraStateMachine_.observeFromCurrentPose();
        }
        updateCameraModel(timeStep);
        recordPhysicsStep(std::move(telemetry), tick_, publishEvents);
        if (!rulesShotCompletionError_.empty())
            return ActionResult{false, rulesShotCompletionError_.c_str()};
        if (!tableDepartureObservationError_.empty())
            return ActionResult{false, "table_departure_observation_incomplete"};
    }
    return ActionResult{};
}

void GameRuntime::recordPhysicsStep(PhysicsStepTelemetry telemetry,
    std::uint64_t attemptedTick, bool publishesEvents)
{
    recentPhysicsSteps_.push_back(RuntimePhysicsStepRecord{
        eventJournalGeneration_, nextPhysicsStepAttemptSequence_++,
        attemptedTick, publishesEvents, std::move(telemetry)});
    while (recentPhysicsSteps_.size() > kPhysicsStepJournalCapacity)
        recentPhysicsSteps_.pop_front();
}

void GameRuntime::recordEvents()
{
    const struct NamedFlag { const char* name; bool value; } flags[] = {
        {"ball_collision", state_.events.ballCollision},
        {"rail_collision", state_.events.railCollision},
        {"ball_pocketed", state_.events.ballPocketed},
        {"cue_ball_pocketed", state_.events.cueBallPocketed},
        {"eight_ball_pocketed", state_.events.eightBallPocketed},
        {"shot_ended", state_.events.shotEnded}
    };
    for (const NamedFlag& flag : flags) {
        if (flag.value) events_.push_back(RuntimeEvent{nextSequence_++, tick_, flag.name});
    }
    if (state_.events.ballPocketed) {
        events_.push_back(RuntimeEvent{nextSequence_++, tick_, "score_updated"});
    }
    const bool foul = state_.events.cueBallPocketed ||
        (state_.events.shotEnded && state_.players.illegalShot);
    if (foul) {
        events_.push_back(RuntimeEvent{nextSequence_++, tick_, "foul"});
    }
    if (state_.events.shotEnded &&
        (state_.players.illegalShot ||
            state_.players.nextPlayer != state_.players.currentPlayer)) {
        events_.push_back(RuntimeEvent{
            nextSequence_++, tick_, "turn_transferred"});
    }
    if (events_.size() > 10000) events_.erase(events_.begin(), events_.begin() + (events_.size() - 10000));
}

std::vector<RuntimeEvent> GameRuntime::eventsSince(std::uint64_t sequence) const
{
    std::vector<RuntimeEvent> result;
    for (const RuntimeEvent& event : events_) {
        if (event.sequence > sequence) result.push_back(event);
    }
    return result;
}

std::vector<AppliedSemanticAction> GameRuntime::appliedSemanticActionsSince(
    std::uint64_t sequence) const
{
    std::vector<AppliedSemanticAction> result;
    for (const AppliedSemanticAction& action : appliedSemanticActions_) {
        if (action.sequence > sequence) result.push_back(action);
    }
    return result;
}

void GameRuntime::recordAppliedSemanticAction(AppliedSemanticActionType type,
    float first, float second)
{
    appliedSemanticActions_.push_back(AppliedSemanticAction{
        nextAppliedSemanticSequence_++, tick_, type, first, second});
    while (appliedSemanticActions_.size() >
        kAppliedSemanticActionJournalCapacity)
        appliedSemanticActions_.pop_front();
}

ActionResult GameRuntime::setBall(int index, const BallState& ball)
{
    if (index < 0 || index >= kBallCount) return ActionResult{false, "invalid_argument"};
    state_.balls[index] = ball;
    state_.ballsMoving = anyBallMoving(state_);
    return ActionResult{};
}

ActionResult GameRuntime::applyCueImpact(const CueImpactInput& input)
{
    if (rulesRackTerminalState_.active)
        return ActionResult{false, "rules_rack_complete"};
    if (!rulesShotCompletionError_.empty())
        return ActionResult{false, rulesShotCompletionError_.c_str()};
    if (state_.balls[0].outOfPlay)
        return ActionResult{false, "cue_ball_requires_placement"};
    if (!tableDepartureObservationError_.empty())
        return ActionResult{false, "table_departure_observation_incomplete"};
    if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
    if (frozenBallDeclarationAuthority_ &&
        frozenBallDeclarationAuthority_->state().phase ==
            rules::FrozenBallDeclarationPhase::Active &&
        (frozenBallDeclarationAuthority_->state().context.parentRevision !=
             rulesAuthorityRevision_ ||
         frozenBallDeclarationAuthority_->state().context.parentStateHash !=
             rulesAuthorityStateHash()))
        frozenBallDeclarationAuthority_->invalidate();
    if (frozenCushionDeclarationAuthority_ &&
        frozenCushionDeclarationAuthority_->state().phase ==
            rules::FrozenCushionDeclarationPhase::Active &&
        (frozenCushionDeclarationAuthority_->state().context.parentRevision !=
             rulesAuthorityRevision_ ||
         frozenCushionDeclarationAuthority_->state().context.parentStateHash !=
             rulesAuthorityStateHash()))
        frozenCushionDeclarationAuthority_->invalidate();
    if(doubleHitCloseCaseAuthority_&&
        doubleHitCloseCaseAuthority_->state().phase==
            rules::DoubleHitCloseCasePhase::Active&&
        (doubleHitCloseCaseAuthority_->state().context.parentRevision!=
             rulesAuthorityRevision_||
         doubleHitCloseCaseAuthority_->state().context.parentStateHash!=
             rulesAuthorityStateHash()))
        doubleHitCloseCaseAuthority_->invalidate();
    if(pushReviewAuthority_&&pushReviewAuthority_->state().phase==rules::PushReviewPhase::Active&&
        (pushReviewAuthority_->state().context.parentRevision!=rulesAuthorityRevision_||
         pushReviewAuthority_->state().context.parentStateHash!=rulesAuthorityStateHash()))
        pushReviewAuthority_->invalidate();
    if (unintentionalBreakMiscueDeclarationAuthority_ &&
        unintentionalBreakMiscueDeclarationAuthority_->state().phase ==
            rules::UnintentionalBreakMiscueDeclarationPhase::Active &&
        (unintentionalBreakMiscueDeclarationAuthority_->state().context.
                actorPlayerId != matchState_.currentPlayer ||
         unintentionalBreakMiscueDeclarationAuthority_->state().context.
                parentRevision != rulesAuthorityRevision_ ||
         unintentionalBreakMiscueDeclarationAuthority_->state().context.
                parentStateHash != rulesAuthorityStateHash()))
        unintentionalBreakMiscueDeclarationAuthority_->invalidate();
    if (groupConfusionAuthority_ &&
        groupConfusionAuthority_->rackStopped())
        return ActionResult{false, "rules_group_confusion_rack_stopped"};
    if (rulesMode_ == RulesMode::ChineseEightBallV1 &&
        (matchState_.groupChoicePending || breakFoulDecisionPending() ||
         eightOnBreakDecisionPending()))
        return ActionResult{false, pendingRulesDecisionError()};
    if (rulesMode_ == RulesMode::ChineseEightBallV1 &&
        matchState_.phase == rules::MatchPhase::Complete)
        return ActionResult{false, "game_over"};
    const bool ballInHandLive = rulesMode_ == RulesMode::ChineseEightBallV1 &&
        ballInHandPlacementPending();
    std::optional<rules::BallInHandPlacementOffer> livePlacementOffer;
    if (ballInHandLive) {
        if (!matchState_.ballInHand || !ballInHandPlacementOffer_ ||
            ballInHandPlacementOffer_->authorityStateRevision !=
                rulesAuthorityRevision_ ||
            ballInHandPlacementOfferStateHash_.empty() ||
            rulesAuthorityStateHash() != ballInHandPlacementOfferStateHash_)
            return ActionResult{false, "rules_placement_state_changed"};
        livePlacementOffer = ballInHandPlacementOffer_;
        if (livePlacementOffer->actorPlayerId != matchState_.currentPlayer)
            return ActionResult{false, "rules_decision_actor_mismatch"};
    } else if (rulesMode_ == RulesMode::ChineseEightBallV1 &&
            behindHeadStringFirstContactContext_ &&
            behindHeadStringFirstContactContext_->actorPlayerId !=
                matchState_.currentPlayer) {
        return ActionResult{false, "rules_head_string_actor_mismatch"};
    }
    CanonicalCueTipStrokeObservation stagedStrokeObservation;
    stagedStrokeObservation.shotIdentity="runtime.stroke."+
        std::to_string(eventJournalGeneration_)+"."+
        std::to_string(tick_)+"."+
        std::to_string(rulesAuthorityRevision_);
    stagedStrokeObservation.actorPlayerId=state_.players.currentPlayer;
    stagedStrokeObservation.rackIdentity="runtime.rack."+
        std::to_string(eventJournalGeneration_);
    stagedStrokeObservation.rulesRevision=rulesAuthorityRevision_;
    stagedStrokeObservation.rulesStateHash=rulesAuthorityStateHash();
    stagedStrokeObservation.startTick=tick_;
    stagedStrokeObservation.endTick=tick_;
    canonicalCueTipStrokeObserver_.rollback();
    if(!canonicalCueTipStrokeObserver_.begin(stagedStrokeObservation))
        return ActionResult{false,"stroke_observation_start"};
    GameState candidate = state_;
    const CueShotApplication application = applyCueShot(
        candidate, input, physicsProfile_, boundaryMode_);
    const bool positiveAppliedImpulse = application.action.ok &&
        application.contact.applied &&
        std::isfinite(application.contact.normalImpulseNs) &&
        application.contact.normalImpulseNs > 0.0;
    if (positiveAppliedImpulse && groupConfusionAuthority_)
        groupConfusionAuthority_->expireWindowOnAcceptedShoot();
    if (positiveAppliedImpulse && frozenBallDeclarationAuthority_ &&
        frozenBallDeclarationAuthority_->state().committed)
        frozenBallDeclarationAuthority_->consume(
            "runtime.frozen.accepted." +
            std::to_string(nextFrozenBallDeclarationShotIdentity_++),tick_);
    if (positiveAppliedImpulse && frozenCushionDeclarationAuthority_ &&
        frozenCushionDeclarationAuthority_->state().committed)
        frozenCushionDeclarationAuthority_->consume(
            stagedStrokeObservation.shotIdentity,
            tick_);
    if(positiveAppliedImpulse&&doubleHitCloseCaseAuthority_&&
        doubleHitCloseCaseAuthority_->state().committed)
        doubleHitCloseCaseAuthority_->consume(
            stagedStrokeObservation.shotIdentity,tick_);
    if(positiveAppliedImpulse&&pushReviewAuthority_&&pushReviewAuthority_->state().committed)
        pushReviewAuthority_->consume(
            stagedStrokeObservation.shotIdentity,tick_);
    if (application.action.ok &&
        unintentionalBreakMiscueDeclarationAuthority_ &&
        unintentionalBreakMiscueDeclarationAuthority_->state().committed) {
        if (!application.contact.applied &&
            application.contact.regime == CueContactRegime::Miscue) {
            unintentionalBreakMiscueDeclarationAuthority_->consume(
                stagedStrokeObservation.shotIdentity, tick_,
                cueImpactReplayBindingSha256(input, physicsProfile_));
        } else if (positiveAppliedImpulse) {
            // A declaration is shot-bound. A normal stroke consumes its
            // opportunity without reclassifying any later shot.
            unintentionalBreakMiscueDeclarationAuthority_->invalidate();
        }
    }
    if(!positiveAppliedImpulse)
        canonicalCueTipStrokeObserver_.rollback();
    if (ballInHandLive && !positiveAppliedImpulse)
        return application.action;
    hasCueImpactInput_ = true;
    cueImpactInput_ = input;
    hasCueContactResult_ = true;
    cueContactResult_ = application.contact;
    cueContactPending_ = true;
    if(positiveAppliedImpulse)
        for(const auto& interval:application.contact.canonicalTipIntervals)
            if(!canonicalCueTipStrokeObserver_.append(interval)){
                canonicalCueTipStrokeObserver_.rollback();break;}
    if(positiveAppliedImpulse){
        if (rulesMode_ == RulesMode::ChineseEightBallV1 &&
            boundaryMode_ == PhysicsBoundaryMode::ProductionTable) {
            rules::CanonicalTableDepartureObserverIdentity departureIdentity;
            departureIdentity.shotIdentity = stagedStrokeObservation.shotIdentity;
            departureIdentity.rackIdentity = stagedStrokeObservation.rackIdentity;
            departureIdentity.solverProfileId = physicsProfile_.id;
            departureIdentity.boundaryMode = boundaryMode_;
            departureIdentity.firstPhysicsTick = tick_ + 1;
            for (int ball = 0; ball < kBallCount; ++ball)
                departureIdentity.initialTrajectoryGenerations[ball] = candidate.balls[ball].trajectoryGeneration;
            tableDepartureObserver_.rollback();
            activeOutsideLandingSurfaceIds_.fill(-1);
            if (!tableDepartureObserver_.begin(departureIdentity))
                return ActionResult{false, "table_departure_observation_start"};
        }
        CanonicalCueObjectOrderObservation identity;
        identity.shotIdentity=stagedStrokeObservation.shotIdentity;
        identity.actorPlayerId=stagedStrokeObservation.actorPlayerId;
        identity.rackIdentity=stagedStrokeObservation.rackIdentity;
        identity.rulesRevision=stagedStrokeObservation.rulesRevision;
        identity.rulesStateHash=stagedStrokeObservation.rulesStateHash;
        identity.physicsTickStart=tick_+1;
        d5BShotObserver_.rollback();
        d5BShotObserver_.begin(identity);
        hasCanonicalCueObjectOrderObservation_=false;
        canonicalCueObjectOrderObservation_=CanonicalCueObjectOrderObservation{};
        hasCanonicalFrozenCushionShotObservation_=false;
        canonicalFrozenCushionShotObservation_=
            CanonicalFrozenCushionShotObservation{};
        if(frozenCushionDeclarationAuthority_&&
           frozenCushionDeclarationAuthority_->state().phase==
               rules::FrozenCushionDeclarationPhase::Consumed){
            const auto& frozen=frozenCushionDeclarationAuthority_->state();
            CanonicalFrozenCushionBinding binding;
            binding.declarationDecisionId=frozen.proposalDecisionId;
            binding.declaredObjectBallId=frozen.context.objectBallId;
            const int declaredCushion=static_cast<int>(frozen.context.cushionId);
            binding.namedStraightCushionFeatureId=declaredCushion==0?2:
                declaredCushion==1?3:declaredCushion==2?0:1;
            binding.declarationParentRevision=frozen.context.parentRevision;
            binding.declarationParentStateHash=frozen.context.parentStateHash;
            binding.consumedShotIdentity=stagedStrokeObservation.shotIdentity;
            binding.authenticated=true;
            binding.committedAndConsumed=frozen.committed&&
                frozen.phase==rules::FrozenCushionDeclarationPhase::Consumed;
            binding.planeObservationId="d5b.frozen.departure."+
                frozen.proposalDecisionId;
            const int feature=binding.namedStraightCushionFeatureId;
            const float radius=physicsProfile_.ball.radiusCm;
            if(feature<2){binding.planeAxis=PhysicsPlaneAxis::Z;
                binding.contactPlaneCoordinateCm=(feature==0?-1.0:1.0)*
                    (physicsProfile_.tableBoundary.playfieldLengthCm*0.5-radius);
                binding.departureDirection=feature==0?
                    PhysicsPlaneCrossingDirection::NegativeToPositive:
                    PhysicsPlaneCrossingDirection::PositiveToNegative;}
            else{binding.planeAxis=PhysicsPlaneAxis::X;
                binding.contactPlaneCoordinateCm=(feature==2?-1.0:1.0)*
                    (physicsProfile_.tableBoundary.playfieldWidthCm*0.5-radius);
                binding.departureDirection=feature==2?
                    PhysicsPlaneCrossingDirection::NegativeToPositive:
                    PhysicsPlaneCrossingDirection::PositiveToNegative;}
            d5BShotObserver_.bindFrozenCushion(binding);
        }
    }
    if (ballInHandLive) {
        const auto offer = *livePlacementOffer;
        if (offer.scope ==
                rules::BallInHandPlacementScope::BehindHeadString) {
            rules::BehindHeadStringFirstContactContext context;
            context.active = true;
            context.actorPlayerId = offer.actorPlayerId;
            context.placementDecisionId = offer.decisionId;
            context.placementAckRevision = rulesAuthorityRevision_ + 1;
            context.headStringWorldZCm = offer.headStringWorldZCm;
            context.observationId = "wpa_ii_08_head_string_cue_center";
            behindHeadStringFirstContactContext_ = context;
        }
        const std::uint64_t previousRevision = rulesAuthorityRevision_;
        matchState_.ballInHand = false;
        ballInHandPlacementScope_ =
            rules::BallInHandPlacementScope::None;
        if (hasLastAdjudication_) {
            lastAdjudication_.ballInHand = false;
            lastAdjudication_.ballInHandBehindHeadString = false;
            lastAdjudication_.nextState = matchState_;
        }
        ++rulesAuthorityRevision_;
        staleBallInHandDecisionIds_.insert(offer.decisionId);
        ballInHandPlacementOffer_.reset();
        ballInHandPlacementOfferStateHash_.clear();
        const std::string authorityHash = rulesAuthorityStateHash();
        events_.push_back(RuntimeEvent{nextSequence_++, tick_,
            "rules_ball_in_hand_consumed_by_positive_impulse"});
        std::ostringstream payload;
        payload.imbue(std::locale::classic());
        payload << std::setprecision(
            std::numeric_limits<double>::max_digits10)
            << "normal_impulse_ns=" << application.contact.normalImpulseNs;
        rulesAuthorityActions_.push_back(rules::RulesAuthorityAction{
            nextRulesAuthorityActionSequence_++, tick_, offer.decisionId,
            rules::kConsumeBallInHandPositiveImpulseActionId,
            "positive-cue-impulse", offer.actorPlayerId, previousRevision,
            rulesAuthorityRevision_, authorityHash, payload.str()});
    }
    if (application.action.ok && rulesMode_ == RulesMode::ChineseEightBallV1) {
        shotFactCollector_.begin(state_, matchState_.currentPlayer,
            matchState_.phase == rules::MatchPhase::Break,
            behindHeadStringFirstContactContext_
                ? &*behindHeadStringFirstContactContext_ : nullptr);
        if (matchState_.phase == rules::MatchPhase::Break) {
            rules::BreakHeadStringReleaseSample sample;
            sample.shotIdentity = nextBreakHeadStringShotIdentity_++;
            sample.actorPlayerId = matchState_.currentPlayer;
            sample.frozenCueBallCenterWorldXCm =
                state_.balls[0].position.x;
            sample.frozenCueBallCenterWorldZCm =
                state_.balls[0].position.z;
            sample.headStringWorldZCm =
                -physicsProfile_.tableBoundary.playfieldLengthCm * 0.25;
            pendingBreakHeadStringRelease_ = sample;
        } else {
            pendingBreakHeadStringRelease_.reset();
        }
    }
    if (application.action.ok) state_ = candidate;
    if (application.action.ok) {
        // Do not turn shooting into a camera command. An explicitly selected
        // ShotFollow preset remains opt-in; ordinary play keeps its exact view.
        if (cameraStateMachine_.frame().state != camera::SemanticState::ShotFollow)
            cameraStateMachine_.observeFromCurrentPose();
    }
    return application.action;
}

void GameRuntime::replaceState(const GameState& state)
{
    state_ = state;
    state_.ballsMoving = anyBallMoving(state_);
    resetPresentationModels();
}

ActionResult GameRuntime::replaceStateForScenario(
    const GameState& state, const CueImpactInput* cueImpact)
{
    return replaceStateForScenario(
        state, defaultChinesePoolPhysicsProfile(), cueImpact);
}

ActionResult GameRuntime::replaceStateForScenario(
    const GameState& state, const PhysicsProfile& profile,
    const CueImpactInput* cueImpact, bool executeCueImpact,
    PhysicsBoundaryMode boundaryMode)
{
    const PhysicsProfileValidation validation = validatePhysicsProfile(profile);
    if (!validation.ok) {
        return ActionResult{false, "invalid_physics_profile"};
    }
    GameState candidate = state;
    CueShotApplication application;
    CanonicalCueTipStrokeObserver scenarioStrokeObserver;
    if (cueImpact && executeCueImpact) {
        CanonicalCueTipStrokeObservation identity;
        identity.shotIdentity="scenario.stroke.0";
        identity.actorPlayerId=state.players.currentPlayer;
        identity.rackIdentity="scenario.rack.0";
        identity.rulesStateHash="scenario.state.0";
        if(!scenarioStrokeObserver.begin(identity))
            return ActionResult{false,"stroke_observation_start"};
        application = applyCueShot(
            candidate, *cueImpact, profile, boundaryMode);
        if (!application.action.ok) return application.action;
        if(application.contact.applied){
            for(const auto& interval:application.contact.canonicalTipIntervals)
                if(!scenarioStrokeObserver.append(interval)){
                    scenarioStrokeObserver.rollback();break;}
        }
    }
    replaceState(candidate);
    physicsProfile_ = profile;
    boundaryMode_ = boundaryMode;
    tick_ = 0;
    nextSequence_ = 1;
    ++eventJournalGeneration_;
    events_.clear();
    nextPhysicsStepAttemptSequence_ = 1;
    recentPhysicsSteps_.clear();
    nextAppliedSemanticSequence_ = 1;
    ++appliedSemanticActionJournalGeneration_;
    appliedSemanticActions_.clear();
    clearGameplayEvents(state_);
    physicsTrace_.clear();
    hasCueImpactInput_ = cueImpact != nullptr;
    cueImpactInput_ = cueImpact ? *cueImpact : CueImpactInput{};
    hasCueContactResult_ = cueImpact && executeCueImpact;
    cueContactResult_ = hasCueContactResult_ ?
        application.contact : CueContactResult{};
    cueContactPending_ = hasCueContactResult_;
    hasCanonicalCueTipStrokeObservation_=false;
    canonicalCueTipStrokeObservationPending_=false;
    canonicalCueTipStrokeObservation_=CanonicalCueTipStrokeObservation{};
    canonicalCueTipStrokeObserver_.rollback();
    d5BShotObserver_.rollback();
    hasCanonicalCueObjectOrderObservation_=false;
    canonicalCueObjectOrderObservation_=CanonicalCueObjectOrderObservation{};
    hasCanonicalFrozenCushionShotObservation_=false;
    canonicalFrozenCushionShotObservation_=
        CanonicalFrozenCushionShotObservation{};
    if(hasCueContactResult_)
        canonicalCueTipStrokeObserver_=std::move(scenarioStrokeObserver);
    resetRulesState();
    return ActionResult{};
}

ActionResult GameRuntime::commitAuthoritativeCueMotionObservationEnd(
    const std::string& shotIdentity,std::uint64_t endTick)
{
    if(canonicalCueTipStrokeObserver_.committed()&&
       canonicalCueTipStrokeObserver_.observation().shotIdentity==shotIdentity&&
       canonicalCueTipStrokeObserver_.observation().endTick==endTick)
        return ActionResult{};
    if(!canonicalCueTipStrokeObserver_.active()||
       canonicalCueTipStrokeObserver_.observation().shotIdentity!=shotIdentity||
       endTick!=tick_)
        return ActionResult{false,"stroke_observation_identity"};
    const auto result=canonicalCueTipStrokeObserver_.finish(endTick);
    if(!result.valid||!result.complete)
        return ActionResult{false,"stroke_observation_incomplete"};
    canonicalCueTipStrokeObservation_=
        canonicalCueTipStrokeObserver_.observation();
    hasCanonicalCueTipStrokeObservation_=false;
    canonicalCueTipStrokeObservationPending_=true;
    if(d5BShotObserver_.active()&&
       !d5BShotObserver_.bindTipObservation(
           canonicalCueTipStrokeObservation_)){
        d5BShotObserver_.rollback();
        return ActionResult{false,"d5b_tip_binding"};
    }
    return ActionResult{};
}

void GameRuntime::clearEvents()
{
    events_.clear();
    clearGameplayEvents(state_);
}

void GameRuntime::syncShotControlFromLegacyState()
{
    shotControl_.yawRadians = state_.aim.yaw;
    shotControl_.requestedPower = state_.input.shotPower;
}

ShotControlModel GameRuntime::promotedShotControl() const
{
    if (shotControl_.schemaVersion == ShotControlSchemaVersion::IntentV2)
        return shotControl_;
    ShotControlModel promoted = shotControl_;
    promoted.schemaVersion = ShotControlSchemaVersion::IntentV2;
    const CueImpactInput legacy = cueImpactFromShotControls(
        shotControl_.yawRadians, shotControl_.requestedPower, physicsProfile_);
    promoted.tipOffsetX = static_cast<float>(legacy.tipOffsetRadius[0]);
    promoted.tipOffsetY = static_cast<float>(legacy.tipOffsetRadius[1]);
    return promoted;
}

void GameRuntime::resetPresentationModels()
{
    shotControl_ = legacyShotControls(state_.aim.yaw, state_.input.shotPower);
    shotControlCapabilities_ = {};
    cameraStateMachine_.reset(camera::SemanticState::ObserveFree,
        camera::Preset::Elevated, cameraSnapshot(state_, cameraStateMachine_.presentationPreferences().m4CameraCandidate));
}

void GameRuntime::updateCameraModel(float deltaSeconds)
{
    camera::UpdateInput input;
    input.fineAimActive = state_.aim.mode == AimMode::Aim && !state_.ballsMoving;
    if (deltaSeconds > 0.0f && state_.ballsMoving)
        input.impactStrength = state_.events.ballCollision ? 1.0f :
            (state_.events.railCollision ? 0.6f : 0.0f);
    cameraStateMachine_.update(deltaSeconds, cameraSnapshot(state_, cameraStateMachine_.presentationPreferences().m4CameraCandidate), input);
}

ActionResult GameRuntime::setTipOffset(float x, float y)
{
    if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
    if (!std::isfinite(x) || !std::isfinite(y))
        return ActionResult{false, "invalid_argument"};
    ShotControlModel candidate = promotedShotControl();
    candidate.tipOffsetX = x;
    candidate.tipOffsetY = y;
    const ShotControlConversion converted = convertShotControls(
        candidate, shotControlCapabilities_, physicsProfile_);
    if (!converted.ok)
        return ActionResult{false, stableShotControlError(converted.error)};
    shotControl_ = candidate;
    recordAppliedSemanticAction(AppliedSemanticActionType::SetTipOffset, x, y);
    return ActionResult{};
}

ActionResult GameRuntime::setAssistLevel(AssistLevel level)
{
    if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
    ShotControlModel candidate = promotedShotControl();
    candidate.assistLevel = level;
    shotControl_ = candidate;
    return ActionResult{};
}

ActionResult GameRuntime::setCameraPreset(camera::SemanticState state,
                                          camera::Preset preset)
{
    if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
    cameraStateMachine_.transitionTo(state, preset, cameraSnapshot(state_, cameraStateMachine_.presentationPreferences().m4CameraCandidate));
    return ActionResult{};
}

void GameRuntime::setCameraPresentationPreferences(
    camera::PresentationPreferences preferences) noexcept
{
    cameraStateMachine_.setPresentationPreferences(preferences);
}

ActionResult GameRuntime::setCameraPresetInstant(camera::SemanticState state,
                                                 camera::Preset preset)
{
    // Deterministic rendered automation needs a completed presentation pose
    // without advancing physics merely to settle a camera transition.
    cameraStateMachine_.reset(state, preset, cameraSnapshot(state_, cameraStateMachine_.presentationPreferences().m4CameraCandidate));
    return ActionResult{};
}

ActionResult GameRuntime::restoreCameraFrame(const camera::Frame& frame)
{
    if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
    if (!cameraStateMachine_.restoreFrame(frame))
        return ActionResult{false, "camera_restore_frame_invalid"};
    return ActionResult{};
}

ActionResult GameRuntime::observeFromCurrentCameraPose()
{
    if (state_.ballsMoving) return ActionResult{false, "invalid_state"};
    if (state_.aim.mode == AimMode::Aim) {
        GameAction toggle;
        toggle.type = ActionType::ToggleAim;
        const auto result = dispatch(toggle);
        if (!result.ok) return result;
    }
    cameraStateMachine_.observeFromCurrentPose();
    return ActionResult{};
}

void GameRuntime::resetRulesState()
{
    resetRulesReplayJournal();
    tableDepartureObserver_.rollback();
    activeOutsideLandingSurfaceIds_.fill(-1);
    tableDepartureObservationError_.clear();
    rulesShotCompletionError_.clear();
    invalidateGroupChoiceAuthority();
    invalidateBallInHandAuthority();
    invalidateBehindHeadStringRespotAuthority();
    invalidateBreakFoulAuthority();
    invalidateEightOnBreakAuthority();
    invalidateRackFoundationAuthority();
    ++rulesAuthorityRevision_;
    matchState_ = rules::MatchState{};
    rulesRackTerminalState_ = RulesRackTerminalState{};
    mobileDeviceViolationHistory_.fill(
        rules::MobileDeviceViolationState::None);
    replayRulesRackIdentity_.clear();
    matchState_.currentPlayer = state_.players.currentPlayer;
    shotFactCollector_ = rules::ShotFactCollector{};
    behindHeadStringFirstContactContext_.reset();
    breakHeadStringOffenseAuthority_.resetMatch();
    nextBreakHeadStringShotIdentity_ = 1;
    pendingBreakHeadStringRelease_.reset();
    hasLastShotFacts_ = false;
    lastShotFacts_ = rules::ShotFacts{};
    hasLastAdjudication_ = false;
    lastAdjudication_ = rules::AdjudicationResult{};
    nonphysicalFoulExplanation_.reset();
    pendingCalledPocket_ = -1;
    ballInHandPlacementScope_ =
        rules::BallInHandPlacementScope::None;
    if (stalemateAuthority_) {
        ++stalemateRackIdentityOrdinal_;
        stalemateAuthority_->reset("runtime.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(stalemateRackIdentityOrdinal_),
            rackOriginalBreakerPlayerId_, rackOriginalShootingOrder_);
    }
    if (groupConfusionAuthority_) {
        ++groupConfusionRackIdentityOrdinal_;
        groupConfusionAuthority_->reset(
            "runtime.group_confusion.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(groupConfusionRackIdentityOrdinal_),
            rackOriginalBreakerPlayerId_, rackOriginalShootingOrder_);
    }
    groupConfusionCompletedShotOrdinal_ = 0;
    if (frozenBallDeclarationAuthority_) {
        ++frozenBallDeclarationRackIdentityOrdinal_;
        frozenBallDeclarationAuthority_->reset(
            "runtime.frozen_declaration.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(frozenBallDeclarationRackIdentityOrdinal_));
    }
    nextFrozenBallDeclarationShotIdentity_ = 1;
    if (frozenCushionDeclarationAuthority_) {
        ++frozenCushionDeclarationRackIdentityOrdinal_;
        frozenCushionDeclarationAuthority_->reset(
            "runtime.frozen_cushion.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(frozenCushionDeclarationRackIdentityOrdinal_));
    }
    nextFrozenCushionDeclarationShotIdentity_ = 1;
    if(doubleHitCloseCaseAuthority_){
        ++doubleHitCloseCaseRackIdentityOrdinal_;
        doubleHitCloseCaseAuthority_->reset("runtime.double_hit_close.rack."+
            std::to_string(eventJournalGeneration_)+"."+
            std::to_string(doubleHitCloseCaseRackIdentityOrdinal_));
    }
    nextDoubleHitCloseCaseShotIdentity_=1;
    if(pushReviewAuthority_){++pushReviewRackIdentityOrdinal_;pushReviewAuthority_->reset(
        "runtime.push_review.rack."+std::to_string(eventJournalGeneration_)+"."+
        std::to_string(pushReviewRackIdentityOrdinal_));}nextPushReviewShotIdentity_=1;
    if(nonphysicalGeneralFoulAuthority_){++nonphysicalGeneralFoulRackIdentityOrdinal_;
        nonphysicalGeneralFoulAuthority_->reset("runtime.nonphysical_foul.rack."+
            std::to_string(eventJournalGeneration_)+"."+
            std::to_string(nonphysicalGeneralFoulRackIdentityOrdinal_));}
    if (unintentionalBreakMiscueDeclarationAuthority_) {
        ++unintentionalBreakMiscueRackIdentityOrdinal_;
        unintentionalBreakMiscueDeclarationAuthority_->reset(
            "runtime.unintentional_break_miscue.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(unintentionalBreakMiscueRackIdentityOrdinal_));
    }
}

void GameRuntime::invalidateGroupChoiceAuthority()
{
    if (groupChoiceDecisionOffer_)
        staleGroupChoiceDecisionIds_.insert(
            groupChoiceDecisionOffer_->decisionId);
    for (const auto& receipt : groupChoiceReceipts_)
        staleGroupChoiceDecisionIds_.insert(receipt.second.request.decisionId);
    groupChoiceDecisionOffer_.reset();
    groupChoiceOfferStateHash_.clear();
    groupChoiceReceipts_.clear();
}

void GameRuntime::invalidateBallInHandAuthority()
{
    if (ballInHandPlacementOffer_)
        staleBallInHandDecisionIds_.insert(
            ballInHandPlacementOffer_->decisionId);
    for (const auto& receipt : ballInHandPlacementReceipts_)
        staleBallInHandDecisionIds_.insert(
            receipt.second.request.decisionId);
    ballInHandPlacementOffer_.reset();
    ballInHandPlacementOfferStateHash_.clear();
    ballInHandPlacementReceipts_.clear();
}

void GameRuntime::invalidateBehindHeadStringRespotAuthority()
{
    if (behindHeadStringRespotOffer_)
        staleBehindHeadStringRespotDecisionIds_.insert(
            behindHeadStringRespotOffer_->decisionId);
    for (const auto& receipt : behindHeadStringRespotReceipts_)
        staleBehindHeadStringRespotDecisionIds_.insert(
            receipt.second.request.decisionId);
    behindHeadStringRespotOffer_.reset();
    behindHeadStringRespotOfferStateHash_.clear();
    behindHeadStringRespotReceipts_.clear();
}

void GameRuntime::stageBallInHandPlacementAuthority(
    rules::BallInHandPlacementScope scope, int actorPlayerId)
{
    rules::BallInHandPlacementOffer offer;
    offer.authorityStateRevision = rulesAuthorityRevision_;
    offer.actorPlayerId = actorPlayerId;
    offer.scope = scope;
    std::ostringstream decisionId;
    if (scope == rules::BallInHandPlacementScope::BehindHeadString) {
        offer.headStringWorldZCm =
            -physicsProfile_.tableBoundary.playfieldLengthCm * 0.25;
        offer.sourceClause = rules::kBehindHeadStringSourceClause;
        offer.sourcePdfPage = 8;
        offer.headStringDefinitionSourceClause =
            rules::kHeadStringDefinitionSourceClause;
        offer.headStringDefinitionPdfPage = 6;
        offer.offeredActionIds[0] =
            rules::kConfirmBehindHeadStringPlacementActionId;
        decisionId << "rules.ball_in_hand.behind_head_string.";
    } else {
        offer.offeredActionIds[0] =
            rules::kConfirmFullTablePlacementActionId;
        decisionId << "rules.ball_in_hand.full_table.";
    }
    decisionId << rulesAuthorityRevision_ << '.'
               << nextBallInHandDecisionOrdinal_++;
    offer.decisionId = decisionId.str();
    ballInHandPlacementOffer_ = offer;
}

void GameRuntime::invalidateEightOnBreakAuthority()
{
    if (eightOnBreakDecisionOffer_)
        staleEightOnBreakDecisionIds_.insert(
            eightOnBreakDecisionOffer_->decisionId);
    for (const auto& receipt : eightOnBreakReceipts_)
        staleEightOnBreakDecisionIds_.insert(
            receipt.second.request.decisionId);
    eightOnBreakDecisionOffer_.reset();
    eightOnBreakOfferStateHash_.clear();
    eightOnBreakReceipts_.clear();
}

void GameRuntime::invalidateBreakFoulAuthority()
{
    if (breakFoulDecisionOffer_)
        staleBreakFoulDecisionIds_.insert(
            breakFoulDecisionOffer_->decisionId);
    for (const auto& receipt : breakFoulReceipts_)
        staleBreakFoulDecisionIds_.insert(
            receipt.second.request.decisionId);
    breakFoulDecisionOffer_.reset();
    breakFoulOfferStateHash_.clear();
    breakFoulReceipts_.clear();
}

void GameRuntime::invalidateRackFoundationAuthority()
{
    rackFoundationReceipts_.clear();
}

const char* GameRuntime::pendingRulesDecisionError() const
{
    if (breakFoulDecisionPending())
        return "rules_break_foul_decision_pending";
    if (eightOnBreakDecisionPending())
        return "rules_eight_on_break_decision_pending";
    if (matchState_.groupChoicePending)
        return "rules_group_choice_pending";
    if (ballInHandPlacementScope_ ==
        rules::BallInHandPlacementScope::BehindHeadString)
        return "rules_ball_in_hand_behind_head_string_pending";
    if (ballInHandPlacementScope_ ==
        rules::BallInHandPlacementScope::FullTable)
        return "rules_ball_in_hand_placement_pending";
    return "rules_decision_pending";
}

ActionResult GameRuntime::setRulesMode(RulesMode mode)
{
    if (state_.ballsMoving || shotFactCollector_.active())
        return ActionResult{false, "invalid_state"};
    rulesMode_ = mode;
    resetRulesState();
    state_.players.currentPlayer = matchState_.currentPlayer;
    state_.players.nextPlayer = matchState_.currentPlayer;
    state_.players.illegalShot = false;
    state_.players.updatedAfterShot = false;
    state_.gameOver = false;
    return ActionResult{};
}

ActionResult GameRuntime::setCalledPocket(int pocket)
{
    if (rulesMode_ != RulesMode::ChineseEightBallV1)
        return ActionResult{false, "ruleset_not_enabled"};
    if (state_.ballsMoving || shotFactCollector_.active())
        return ActionResult{false, "invalid_state"};
    if (breakFoulDecisionPending() || eightOnBreakDecisionPending())
        return ActionResult{false, pendingRulesDecisionError()};
    if (pocket < -1 || pocket >= 6)
        return ActionResult{false, "called_pocket_out_of_range"};
    pendingCalledPocket_ = pocket;
    recordAppliedSemanticAction(AppliedSemanticActionType::SetCalledPocket,
        static_cast<float>(pocket));
    return ActionResult{};
}

ActionResult GameRuntime::applyRulesAdjudication(
    const rules::ShotFacts& facts, bool publishEvents,
    const rules::BreakHeadStringReleaseSample* breakRelease,
    const CueContactResult* authoritativeStroke)
{
    if (rulesMode_ != RulesMode::ChineseEightBallV1)
        return ActionResult{false, "ruleset_not_enabled"};
    if (facts.shooter != matchState_.currentPlayer)
        return ActionResult{false, "rules_shooter_mismatch"};
    // WPA II 6(g) distinguishes an unintentional miscue that never contacts
    // the rack from other break-shot fouls and explicitly removes ball in
    // hand. Imported ShotFacts cannot distinguish that case. A completed
    // native shot, however, retains its actual successful non-miscue cue
    // contact: insufficient speed is not evidence that the tip slipped off.
    // Only finishRulesShot can supply this private producer witness.
    const bool knownNonMiscue = authoritativeStroke &&
        authoritativeStroke->applied && authoritativeStroke->error.empty() &&
        std::isfinite(authoritativeStroke->normalImpulseNs) &&
        authoritativeStroke->normalImpulseNs > 0.0 &&
        (authoritativeStroke->regime == CueContactRegime::Stick ||
         authoritativeStroke->regime == CueContactRegime::Slip ||
         authoritativeStroke->regime == CueContactRegime::Released);
    const bool noRackContact = facts.breakShot &&
        facts.firstContactBall < 1 && facts.firstContactBalls.empty();
    const bool nativeMiscue = authoritativeStroke &&
        !authoritativeStroke->applied &&
        authoritativeStroke->regime == CueContactRegime::Miscue &&
        authoritativeStroke->error ==
            "miscue_offset_exceeds_reliable_radius";
    const bool signedUnintentionalMiscue =
        unintentionalBreakMiscueDeclarationAuthority_ &&
        unintentionalBreakMiscueDeclarationAuthority_->state().phase ==
            rules::UnintentionalBreakMiscueDeclarationPhase::Consumed &&
        unintentionalBreakMiscueDeclarationAuthority_->state().committed &&
        unintentionalBreakMiscueDeclarationAuthority_->state().context.
            actorPlayerId == facts.shooter &&
        !unintentionalBreakMiscueDeclarationAuthority_->state().
            consumedShotIdentity.empty();
    const bool unintentionalNoRackContactMiscue = noRackContact &&
        nativeMiscue && signedUnintentionalMiscue &&
        facts.breakEvidenceOrigin ==
            rules::BreakEvidenceOrigin::AuthoritativeShotFactCollector &&
        facts.breakEvidenceComplete;
    if (noRackContact && !knownNonMiscue &&
        !unintentionalNoRackContactMiscue)
        return ActionResult{
            false, "rules_break_no_rack_contact_classification_pending"};
    rules::AdjudicationResult adjudication =
        chineseEightBallRules_.adjudicate(matchState_, facts);
    if (!adjudication.accepted)
        return ActionResult{false, "rules_shot_rejected"};
    const bool illegalBreak = std::find(adjudication.reasons.begin(),
        adjudication.reasons.end(), rules::FoulReason::IllegalBreak) !=
        adjudication.reasons.end();
    if (unintentionalNoRackContactMiscue) {
        // WPA II 6(g) is not ordinary break-shot ball in hand. The opponent
        // chooses only who re-breaks. Mechanical Miscue evidence alone is
        // insufficient; reaching this branch also requires the bound signed
        // declaration and complete native collector evidence above.
        adjudication.foul = true;
        adjudication.continueTurn = false;
        adjudication.ballInHand = false;
        adjudication.ballInHandBehindHeadString = false;
        adjudication.rerackAllowed = true;
        adjudication.nextPlayer = 1 - facts.shooter;
        adjudication.nextState.currentPlayer = adjudication.nextPlayer;
        adjudication.nextState.ballInHand = false;
        adjudication.decisionOptions = {
            "rerack_opponent_breaks",
            "rerack_original_breaker"};
    }
    if (facts.breakShot && illegalBreak && !adjudication.foul &&
        (facts.breakEvidenceOrigin !=
             rules::BreakEvidenceOrigin::AuthoritativeShotFactCollector ||
         !facts.breakEvidenceComplete)) {
        return ActionResult{false,
            "rules_illegal_break_requires_authoritative_evidence"};
    }
    if (breakRelease != nullptr) {
        if (!facts.breakShot ||
            breakRelease->actorPlayerId != facts.shooter) {
            return ActionResult{
                false, "rules_break_head_string_context_mismatch"};
        }
        const rules::BreakHeadStringOffenseResult headString =
            breakHeadStringOffenseAuthority_.commit(*breakRelease);
        if (!headString.ok) {
            if (headString.errorCode ==
                    "rules_break_head_string_identity_invalid")
                return ActionResult{false,
                    "rules_break_head_string_identity_invalid"};
            if (headString.errorCode ==
                    "rules_break_head_string_actor_invalid")
                return ActionResult{false,
                    "rules_break_head_string_actor_invalid"};
            if (headString.errorCode ==
                    "rules_break_head_string_position_nonfinite")
                return ActionResult{false,
                    "rules_break_head_string_position_nonfinite"};
            return ActionResult{false,
                "rules_break_head_string_identity_conflict"};
        }
        const auto outcome = headString.receipt.outcome;
        if (outcome == rules::BreakHeadStringOffenseOutcome::Reminder) {
            adjudication.textKeys.push_back(headString.receipt.textKey);
        } else if (outcome == rules::BreakHeadStringOffenseOutcome::Foul) {
            if (std::find(adjudication.reasons.begin(),
                    adjudication.reasons.end(),
                    rules::FoulReason::BreakCueBallPastHeadString) ==
                adjudication.reasons.end()) {
                adjudication.reasons.push_back(
                    rules::FoulReason::BreakCueBallPastHeadString);
                adjudication.textKeys.push_back(
                    headString.receipt.textKey);
            }
            adjudication.foul = true;
            adjudication.continueTurn = false;
            adjudication.ballInHand = true;
            adjudication.ballInHandBehindHeadString = true;
            adjudication.rerackAllowed = true;
            adjudication.nextState.ballInHand = true;
            adjudication.nextPlayer = 1 - facts.shooter;
            adjudication.nextState.currentPlayer = adjudication.nextPlayer;
            if (adjudication.eightBallMustBeSpotted) {
                adjudication.decisionOptions = {
                    "spot_eight_ball_in_hand_head_string",
                    "rerack_opponent_breaks",
                    "rerack_original_breaker"};
            } else {
                adjudication.decisionOptions = {
                    "ball_in_hand_behind_head_string",
                    "rerack_opponent_breaks",
                    "rerack_original_breaker"};
            }
        }
    }
    for (int ball : facts.objectBallsOffTable) state_.balls[ball].outOfPlay = true;
    if (facts.cueBallOffTable) state_.balls[0].outOfPlay = true;
    lastShotFacts_ = facts;
    hasLastShotFacts_ = true;
    lastAdjudication_ = adjudication;
    nonphysicalFoulExplanation_.reset();
    hasLastAdjudication_ = true;
    matchState_ = adjudication.nextState;
    ++rulesAuthorityRevision_;
    groupChoiceDecisionOffer_.reset();
    groupChoiceOfferStateHash_.clear();
    ballInHandPlacementOffer_.reset();
    ballInHandPlacementOfferStateHash_.clear();
    breakFoulDecisionOffer_.reset();
    breakFoulOfferStateHash_.clear();
    eightOnBreakDecisionOffer_.reset();
    eightOnBreakOfferStateHash_.clear();
    ballInHandPlacementScope_ =
        rules::BallInHandPlacementScope::None;
    const bool terminalAdjudication = adjudication.gameOver ||
        adjudication.nextState.phase == rules::MatchPhase::Complete;
    if (!terminalAdjudication && adjudication.groupChoiceRequired) {
        rules::GroupChoiceDecisionOffer offer;
        offer.authorityStateRevision = rulesAuthorityRevision_;
        offer.actorPlayerId = facts.shooter;
        offer.continueTurn = adjudication.continueTurn;
        offer.nextPlayer = adjudication.nextPlayer;
        std::ostringstream decisionId;
        decisionId << "rules.group_choice." << rulesAuthorityRevision_ << '.'
                   << nextGroupChoiceDecisionOrdinal_++;
        offer.decisionId = decisionId.str();
        groupChoiceDecisionOffer_ = offer;
    }
    const bool breakChoiceRequired = !terminalAdjudication && facts.breakShot &&
        !adjudication.eightBallMustBeSpotted &&
        (adjudication.foul || illegalBreak);
    if (breakChoiceRequired) {
        rules::BreakFoulDecisionOffer offer;
        offer.authorityStateRevision = rulesAuthorityRevision_;
        offer.actorPlayerId = 1 - facts.shooter;
        offer.originalBreakerPlayerId = facts.shooter;
        offer.frozenNextPlayer = adjudication.nextPlayer;
        if (unintentionalNoRackContactMiscue) {
            offer.branch = rules::BreakFoulBranch::NoRackContactMiscue;
            offer.sourceClause = rules::kBreakNoRackContactSourceClause;
            offer.sourcePdfPage = 8;
            offer.offeredActionIds = {
                rules::kBreakFoulRerackOpponentActionId,
                rules::kBreakFoulRerackOriginalActionId};
        } else if (adjudication.foul) {
            offer.branch = rules::BreakFoulBranch::BreakShotFoul;
            offer.sourceClause = rules::kBreakShotFoulSourceClause;
            offer.sourcePdfPage = 7;
            offer.offeredActionIds = {
                rules::kBreakFoulBallInHandActionId,
                rules::kBreakFoulRerackOpponentActionId,
                rules::kBreakFoulRerackOriginalActionId};
            // The entitlement is created only after the parent choice commits.
            matchState_.ballInHand = false;
            lastAdjudication_.ballInHand = false;
            lastAdjudication_.ballInHandBehindHeadString = false;
            lastAdjudication_.nextState = matchState_;
        } else {
            offer.branch = rules::BreakFoulBranch::IllegalBreak;
            offer.sourceClause = rules::kIllegalBreakSourceClause;
            offer.sourcePdfPage = 7;
            offer.offeredActionIds = {
                rules::kBreakFoulAcceptTableActionId,
                rules::kBreakFoulRerackOpponentActionId,
                rules::kBreakFoulRerackOriginalActionId};
        }
        std::ostringstream decisionId;
        decisionId << "rules.break_foul." << rulesAuthorityRevision_ << '.'
                   << nextBreakFoulDecisionOrdinal_++;
        offer.decisionId = decisionId.str();
        breakFoulDecisionOffer_ = offer;
    }
    const bool eightOnBreakChoiceRequired = !terminalAdjudication &&
        adjudication.eightBallMustBeSpotted && facts.breakShot;
    if (eightOnBreakChoiceRequired && adjudication.foul) {
        // WPA II 6(f) makes BehindHeadString ball in hand one of the
        // opponent's choices. Keep the entitlement absent until that exact
        // spot-plus-BIH parent action commits.
        matchState_.ballInHand = false;
        lastAdjudication_.ballInHand = false;
        lastAdjudication_.ballInHandBehindHeadString = false;
        lastAdjudication_.nextState = matchState_;
    }
    if (!terminalAdjudication && adjudication.ballInHand &&
        !breakChoiceRequired &&
        !eightOnBreakChoiceRequired) {
        ballInHandPlacementScope_ =
            adjudication.ballInHandBehindHeadString ?
                rules::BallInHandPlacementScope::BehindHeadString :
                rules::BallInHandPlacementScope::FullTable;
        stageBallInHandPlacementAuthority(
            ballInHandPlacementScope_, adjudication.nextPlayer);
    }
    if (eightOnBreakChoiceRequired) {
        rules::EightOnBreakDecisionOffer offer;
        offer.authorityStateRevision = rulesAuthorityRevision_;
        offer.actorPlayerId = adjudication.foul ?
            1 - facts.shooter : facts.shooter;
        offer.originalBreakerPlayerId = facts.shooter;
        offer.frozenNextPlayer = adjudication.nextPlayer;
        offer.frozenContinueTurn = adjudication.continueTurn;
        offer.branch = adjudication.foul ?
            rules::EightOnBreakBranch::FoulBreak :
            rules::EightOnBreakBranch::LegalBreak;
        if (adjudication.foul) {
            offer.offeredActionIds = {
                rules::kEightOnBreakSpotBallInHandActionId,
                rules::kEightOnBreakRerackOpponentActionId,
                rules::kEightOnBreakRerackOriginalActionId};
        } else {
            offer.offeredActionIds = {
                rules::kEightOnBreakSpotContinueActionId,
                rules::kEightOnBreakRerackOriginalActionId};
        }
        std::ostringstream decisionId;
        decisionId << "rules.eight_on_break."
                   << rulesAuthorityRevision_ << '.'
                   << nextEightOnBreakDecisionOrdinal_++;
        offer.decisionId = decisionId.str();
        eightOnBreakDecisionOffer_ = offer;
    }
    state_.players.currentPlayer = adjudication.nextPlayer;
    state_.players.nextPlayer = adjudication.nextPlayer;
    state_.players.illegalShot = adjudication.foul;
    state_.players.updatedAfterShot = true;
    state_.players.firstPocketedObjectBall =
        matchState_.phase == rules::MatchPhase::Break ||
        matchState_.phase == rules::MatchPhase::OpenTable;
    for (int player = 0; player < 2; ++player) {
        const rules::BallGroup group = matchState_.groups[player];
        state_.players.assignedBallType[player] =
            group == rules::BallGroup::Solids ? 0 :
            group == rules::BallGroup::Stripes ? 1 : -1;
    }
    state_.gameOver = adjudication.gameOver;
    pendingCalledPocket_ = -1;
    if (publishEvents) {
        events_.push_back(RuntimeEvent{nextSequence_++, tick_, "rules_adjudicated"});
        for (rules::FoulReason reason : adjudication.reasons) {
            events_.push_back(RuntimeEvent{nextSequence_++, tick_,
                std::string("rules_reason_") + rules::foulReasonCode(reason)});
        }
        if (adjudication.foul)
            events_.push_back(RuntimeEvent{nextSequence_++, tick_, "rules_foul"});
        if (adjudication.nextPlayer != facts.shooter)
            events_.push_back(RuntimeEvent{nextSequence_++, tick_,
                "rules_turn_transferred"});
        if (adjudication.gameOver)
            events_.push_back(RuntimeEvent{nextSequence_++, tick_, "rules_game_over"});
        if (adjudication.groupChoiceRequired)
            events_.push_back(RuntimeEvent{nextSequence_++, tick_,
                "rules_group_choice_pending"});
        if (adjudication.eightBallMustBeSpotted)
            events_.push_back(RuntimeEvent{nextSequence_++, tick_,
                "rules_eight_on_break_decision_pending"});
        if (breakFoulDecisionOffer_)
            events_.push_back(RuntimeEvent{nextSequence_++, tick_,
                "rules_break_foul_decision_pending"});
    }
    return ActionResult{};
}

void GameRuntime::finishRulesShot(bool publishEvents)
{
    if (tableDepartureObserver_.active()) {
        const auto validation = tableDepartureObserver_.finish(true);
        if (!validation.valid || !validation.complete) {
            tableDepartureObservationError_ = validation.error;
            if (publishEvents) events_.push_back(RuntimeEvent{nextSequence_++, tick_,
                "rules_table_departure_observation_incomplete"});
            return;
        }
    }
    if(d5BShotObserver_.active()){
        const auto validation=d5BShotObserver_.finish(true);
        if(validation.valid&&validation.complete){
            canonicalCueObjectOrderObservation_=
                d5BShotObserver_.cueObjectObservation();
            hasCanonicalCueObjectOrderObservation_=true;
            if(d5BShotObserver_.hasFrozenObservation()){
                canonicalFrozenCushionShotObservation_=
                    d5BShotObserver_.frozenObservation();
                hasCanonicalFrozenCushionShotObservation_=true;
            }
        }else d5BShotObserver_.rollback();
    }
    if ((hasCanonicalCueTipStrokeObservation_ ||
         canonicalCueTipStrokeObservationPending_) &&
        hasCanonicalCueObjectOrderObservation_) {
        bool signedCloseCaseBound = false;
        if (doubleHitCloseCaseAuthority_) {
            const auto& closeCase = doubleHitCloseCaseAuthority_->state();
            signedCloseCaseBound =
                closeCase.phase == rules::DoubleHitCloseCasePhase::Consumed &&
                closeCase.committed && closeCase.reviewNeeded &&
                closeCase.consumedShotIdentity ==
                    canonicalCueTipStrokeObservation_.shotIdentity &&
                closeCase.context.cueBallId == 0 &&
                !canonicalCueObjectOrderObservation_.contacts.empty() &&
                closeCase.context.objectBallId ==
                    canonicalCueObjectOrderObservation_.contacts.front().objectBallId;
        }
        const auto classification = rules::classifyCanonicalDoubleHit(
            canonicalCueTipStrokeObservation_,
            canonicalCueObjectOrderObservation_, signedCloseCaseBound);
        if (classification.complete)
            shotFactCollector_.bindCanonicalDoubleHitClassification(
                classification);
    }
    if (hasCanonicalCueTipStrokeObservation_ ||
        canonicalCueTipStrokeObservationPending_) {
        const rules::PushReviewState* signedPush = nullptr;
        if (pushReviewAuthority_ &&
            pushReviewAuthority_->state().phase ==
                rules::PushReviewPhase::Consumed)
            signedPush = &pushReviewAuthority_->state();
        const auto push = rules::classifyCanonicalPush(
            canonicalCueTipStrokeObservation_, signedPush);
        if (push.complete)
            shotFactCollector_.bindCanonicalPushClassification(push);
    }
    if (hasCanonicalFrozenCushionShotObservation_ &&
        hasCanonicalCueObjectOrderObservation_) {
        bool groupCleared = false;
        const rules::BallGroup group =
            matchState_.groups[matchState_.currentPlayer];
        if (group == rules::BallGroup::Solids) {
            groupCleared = true;
            for (int ball = 1; ball <= 7; ++ball)
                groupCleared = groupCleared && (state_.balls[ball].pocketed || state_.balls[ball].outOfPlay);
        } else if (group == rules::BallGroup::Stripes) {
            groupCleared = true;
            for (int ball = 9; ball <= 15; ++ball)
                groupCleared = groupCleared && (state_.balls[ball].pocketed || state_.balls[ball].outOfPlay);
        }
        std::vector<int> legalFirstContactBallIds;
        for (int ball = 1; ball < kBallCount; ++ball)
            if (rules::isLegalFirstContactBall(
                    matchState_, ball, groupCleared))
                legalFirstContactBallIds.push_back(ball);
        const auto frozen = rules::classifyCanonicalFrozenCushion(
            canonicalFrozenCushionShotObservation_,
            canonicalCueObjectOrderObservation_,
            legalFirstContactBallIds);
        if (frozen.complete)
            shotFactCollector_.bindCanonicalFrozenCushionClassification(
                frozen);
    }
    rules::ShotFacts facts = shotFactCollector_.finish();
    if (tableDepartureObserver_.committed()) {
        for (const auto& measurement : tableDepartureObserver_.observation().balls) {
            if (!measurement.complete || !measurement.cameToRestOffTable) continue;
            if (measurement.ballId == 0) facts.cueBallOffTable = true;
            else if (std::find(facts.objectBallsOffTable.begin(), facts.objectBallsOffTable.end(),
                    measurement.ballId) == facts.objectBallsOffTable.end())
                facts.objectBallsOffTable.push_back(measurement.ballId);
        }
    }
    facts.calledPocket = pendingCalledPocket_;
    const auto staged = pendingBreakHeadStringRelease_;
    // The release evidence is consumed by this adjudication. Remove the
    // pending marker before freezing any resulting decision's state hash;
    // clearing it afterwards would invalidate a freshly issued offer.
    pendingBreakHeadStringRelease_.reset();
    const auto consumedHeadStringContext =
        behindHeadStringFirstContactContext_;
    if (facts.behindHeadStringFirstContactRestrictionActive)
        behindHeadStringFirstContactContext_.reset();
    const ActionResult adjudicated = applyRulesAdjudication(
        facts, publishEvents, staged ? &*staged : nullptr,
        hasCueContactResult_ ? &cueContactResult_ : nullptr);
    if (!adjudicated.ok) {
        behindHeadStringFirstContactContext_ =
            consumedHeadStringContext;
        rulesShotCompletionError_ = adjudicated.errorCode;
        if (publishEvents) events_.push_back(RuntimeEvent{nextSequence_++, tick_,
            std::string("rules_shot_completion_rejected:") + adjudicated.errorCode});
    } else if (stalemateAuthority_) {
        const std::string shotIdentity = "runtime.completed." +
            std::to_string(tick_) + "." +
            std::to_string(facts.shooter);
        stalemateAuthority_->recordCompletedShot(facts.shooter,
            shotIdentity, stalemateAuthority_->state().rackIdentity);
    }
    if (adjudicated.ok) {
        openGroupConfusionWindowForCompletedShot(facts);
        sealPendingRulesDecisionStateHashes();
    }
    pendingBreakHeadStringRelease_.reset();
}

void GameRuntime::openGroupConfusionWindowForCompletedShot(
    const rules::ShotFacts& facts)
{
    if (groupConfusionAuthority_ &&
        matchState_.phase == rules::MatchPhase::GroupsAssigned &&
        matchState_.groups[0] != rules::BallGroup::Open &&
        matchState_.groups[1] != rules::BallGroup::Open) {
        rules::GroupConfusionWindow window;
        window.rackIdentity =
            groupConfusionAuthority_->state().window.rackIdentity;
        window.originalBreakerPlayerId = rackOriginalBreakerPlayerId_;
        window.originalShootingOrder = rackOriginalShootingOrder_;
        std::ostringstream assignment;
        assignment << static_cast<int>(matchState_.groups[0]) << '|'
                   << static_cast<int>(matchState_.groups[1]);
        window.assignmentSnapshotSha256 =
            rules::refereeAuthoritySha256Hex(assignment.str());
        window.completedShotIdentity = "runtime.group_confusion.completed." +
            std::to_string(tick_) + "." +
            std::to_string(facts.shooter) + "." +
            std::to_string(++groupConfusionCompletedShotOrdinal_);
        window.completedShotOrdinal = groupConfusionCompletedShotOrdinal_;
        window.completedShooter = facts.shooter;
        window.completedRevision = rulesAuthorityRevision_;
        window.completedStateHash = rulesAuthorityStateHash();
        groupConfusionAuthority_->openWindow(window);
    }
}

void GameRuntime::sealPendingRulesDecisionStateHashes()
{
    // Freeze each newly published offer only after the complete rules-shot
    // transaction has finished.  Hashing inside applyRulesAdjudication would
    // precede caller-owned post-adjudication authority transitions (notably
    // the completed-shot group-confusion window) and make the offer stale at
    // birth.  This helper is deliberately not called from idle stepping or
    // submit paths: later authority or physical drift must still invalidate
    // the frozen offer.
    const std::string finalStateHash = rulesAuthorityStateHash();
    if (eightOnBreakDecisionOffer_)
        eightOnBreakOfferStateHash_ = finalStateHash;
    if (groupChoiceDecisionOffer_)
        groupChoiceOfferStateHash_ = finalStateHash;
    if (breakFoulDecisionOffer_)
        breakFoulOfferStateHash_ = finalStateHash;
    if (ballInHandPlacementOffer_)
        ballInHandPlacementOfferStateHash_ = finalStateHash;
}

ActionResult GameRuntime::adjudicateRulesShot(const rules::ShotFacts& facts)
{
    if (state_.ballsMoving || shotFactCollector_.active())
        return ActionResult{false, "invalid_state"};
    if (matchState_.groupChoicePending || ballInHandPlacementPending() ||
        breakFoulDecisionPending() || eightOnBreakDecisionPending())
        return ActionResult{false, pendingRulesDecisionError()};
    if (behindHeadStringFirstContactContext_)
        return ActionResult{false,
            "rules_head_string_authoritative_telemetry_required"};
    if (facts.breakShot != (matchState_.phase == rules::MatchPhase::Break))
        return ActionResult{false, "rules_break_flag_mismatch"};
    const rules::ShotFactsValidationError validation =
        rules::validateShotFacts(facts);
    if (validation != rules::ShotFactsValidationError::None)
        return ActionResult{false,
            rules::shotFactsValidationErrorCode(validation)};
    const ActionResult result = applyRulesAdjudication(facts, true);
    if (result.ok) {
        openGroupConfusionWindowForCompletedShot(facts);
        sealPendingRulesDecisionStateHashes();
    }
    return result;
}

void GameRuntime::resetRulesReplayJournal()
{
    ++rulesReplayEventJournalGeneration_;
    nextRulesReplayEventSequence_=1;
    rulesReplayEvents_.clear();
}

std::string GameRuntime::currentRulesReplayRackIdentity() const
{
    if(!replayRulesRackIdentity_.empty())return replayRulesRackIdentity_;
    if(nonphysicalGeneralFoulAuthority_&&
       !nonphysicalGeneralFoulAuthority_->state().rackIdentity.empty())
        return nonphysicalGeneralFoulAuthority_->state().rackIdentity;
    return "runtime.nonphysical_foul.rack."+
        std::to_string(eventJournalGeneration_)+"."+
        std::to_string(nonphysicalGeneralFoulRackIdentityOrdinal_);
}

std::string GameRuntime::rulesReplayEffectStateHash(
    RuntimeRulesReplayEffectVersion version) const
{
    std::ostringstream canonical;
    canonical.imbue(std::locale::classic());
    canonical << std::setprecision(
        std::numeric_limits<float>::max_digits10)
        << "rules_replay_effect_state_v1"
        << '|' << rulesAuthorityRevision_
        << '|' << static_cast<int>(rulesMode_)
        << '|' << static_cast<int>(matchState_.phase)
        << '|' << matchState_.currentPlayer
        << '|' << (matchState_.ballInHand?1:0)
        << '|' << matchState_.winner
        << '|' << static_cast<int>(ballInHandPlacementScope_)
        << '|' << static_cast<int>(mobileDeviceViolationHistory_[0])
        << '|' << static_cast<int>(mobileDeviceViolationHistory_[1])
        << '|' << replayRulesRackIdentity_.size() << ':'
        << replayRulesRackIdentity_
        << '|' << (rulesRackTerminalState_.active?1:0)
        << '|' << static_cast<int>(rulesRackTerminalState_.reason)
        << '|' << rulesRackTerminalState_.losingPlayer
        << '|' << rulesRackTerminalState_.winningPlayer
        << '|' << rulesRackTerminalState_.authorityDecisionId.size() << ':'
        << rulesRackTerminalState_.authorityDecisionId
        << '|' << rulesRackTerminalState_.committedAuthorityRevision
        << '|' << authorityCanonicalZero(state_.balls[0].position.x)
        << '|' << authorityCanonicalZero(state_.balls[0].position.z)
        << '|' << (state_.balls[0].pocketed?1:0)
        << '|' << (state_.balls[0].outOfPlay?1:0);
    if(ballInHandPlacementOffer_){
        const auto& offer=*ballInHandPlacementOffer_;
        canonical << "|placement|" << offer.decisionId.size() << ':'
                  << offer.decisionId << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId << '|'
                  << static_cast<int>(offer.scope);
    }else canonical << "|placement_none";
    if(version==RuntimeRulesReplayEffectVersion::MobileAndPlacementV1)
        return authoritySha256Hex(canonical.str());

    canonical << "|single_rack_choices_v2"
              << '|' << static_cast<int>(matchState_.groups[0])
              << '|' << static_cast<int>(matchState_.groups[1])
              << '|' << (matchState_.groupChoicePending?1:0)
              << '|' << state_.players.currentPlayer
              << '|' << state_.players.nextPlayer
              << '|' << state_.players.assignedBallType[0]
              << '|' << state_.players.assignedBallType[1]
              << '|' << (state_.players.firstPocketedObjectBall?1:0);
    if(groupChoiceDecisionOffer_){
        const auto& offer=*groupChoiceDecisionOffer_;
        canonical << "|group_choice|" << offer.decisionId.size() << ':'
                  << offer.decisionId << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId
                  << '|' << (offer.continueTurn?1:0)
                  << '|' << offer.nextPlayer;
    }else canonical << "|group_choice_none";
    if(behindHeadStringRespotOffer_){
        const auto& offer=*behindHeadStringRespotOffer_;
        canonical << "|head_string_respot|" << offer.decisionId.size() << ':'
                  << offer.decisionId << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId << '|'
                  << offer.sourcePlacementDecisionId.size() << ':'
                  << offer.sourcePlacementDecisionId << '|'
                  << authorityCanonicalZero(offer.headStringWorldZCm)
                  << '|' << offer.tiedCandidateBallIds.size();
        for(int ball:offer.tiedCandidateBallIds)canonical << '|' << ball;
    }else canonical << "|head_string_respot_none";
    if(breakFoulDecisionOffer_){
        const auto& offer=*breakFoulDecisionOffer_;
        canonical << "|break_foul|" << offer.decisionId.size() << ':'
                  << offer.decisionId << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId
                  << '|' << offer.originalBreakerPlayerId
                  << '|' << offer.frozenNextPlayer
                  << '|' << static_cast<int>(offer.branch)
                  << '|' << offer.offeredActionIds.size();
        for(const auto& action:offer.offeredActionIds)
            canonical << '|' << action.size() << ':' << action;
    }else canonical << "|break_foul_none";
    if(eightOnBreakDecisionOffer_){
        const auto& offer=*eightOnBreakDecisionOffer_;
        canonical << "|eight_on_break|" << offer.decisionId.size() << ':'
                  << offer.decisionId << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId
                  << '|' << offer.originalBreakerPlayerId
                  << '|' << offer.frozenNextPlayer
                  << '|' << (offer.frozenContinueTurn?1:0)
                  << '|' << static_cast<int>(offer.branch)
                  << '|' << offer.offeredActionIds.size();
        for(const auto& action:offer.offeredActionIds)
            canonical << '|' << action.size() << ':' << action;
    }else canonical << "|eight_on_break_none";
    if(behindHeadStringFirstContactContext_){
        const auto& context=*behindHeadStringFirstContactContext_;
        canonical << "|head_string_contact|" << context.actorPlayerId << '|'
                  << context.placementDecisionId.size() << ':'
                  << context.placementDecisionId << '|'
                  << context.placementAckRevision << '|'
                  << authorityCanonicalZero(context.headStringWorldZCm)
                  << '|' << context.observationId.size() << ':'
                  << context.observationId;
    }else canonical << "|head_string_contact_none";
    for(int index=0;index<kBallCount;++index){
        const BallState& ball=state_.balls[index];
        canonical << "|ball|" << index
                  << '|' << authorityCanonicalZero(ball.position.x)
                  << '|' << authorityCanonicalZero(ball.position.y)
                  << '|' << authorityCanonicalZero(ball.position.z)
                  << '|' << (ball.pocketed?1:0)
                  << '|' << (ball.outOfPlay?1:0)
                  << '|' << static_cast<int>(ball.physicalDomain)
                  << '|' << static_cast<int>(ball.pocketInteraction.phase)
                  << '|' << ball.pocketInteraction.pocketId
                  << '|' << ball.pocketInteraction.captureSequence
                  << '|' << ball.trajectoryGeneration;
    }
    if(version==RuntimeRulesReplayEffectVersion::SingleRackChoicesV2)
        return authoritySha256Hex(canonical.str());

    canonical << "|single_rack_declarations_v3";
    if(pushReviewAuthority_&&pushReviewAuthority_->state().committed){
        const auto& value=pushReviewAuthority_->state();
        const std::string shot=replayStableShotIdentity(
            value.consumedShotIdentity);
        canonical << "|push_review|" << static_cast<int>(value.phase)
                  << '|' << value.rackIdentity.size() << ':' << value.rackIdentity
                  << '|' << value.context.parentRevision
                  << '|' << value.context.parentStateHash
                  << '|' << value.proposalDecisionId.size() << ':'
                  << value.proposalDecisionId
                  << '|' << value.refereeSubjectId.size() << ':'
                  << value.refereeSubjectId
                  << '|' << value.finalPayloadSha256
                  << '|' << shot.size() << ':' << shot
                  << '|' << value.consumedAuthorityTick;
    }else canonical << "|push_review_none";
    if(doubleHitCloseCaseAuthority_&&
       doubleHitCloseCaseAuthority_->state().committed){
        const auto& value=doubleHitCloseCaseAuthority_->state();
        const std::string shot=replayStableShotIdentity(
            value.consumedShotIdentity);
        canonical << "|double_hit_close_case|" << static_cast<int>(value.phase)
                  << '|' << value.rackIdentity.size() << ':' << value.rackIdentity
                  << '|' << value.context.objectBallId
                  << '|' << value.context.parentRevision
                  << '|' << value.context.parentStateHash
                  << '|' << value.proposalDecisionId.size() << ':'
                  << value.proposalDecisionId
                  << '|' << value.refereeSubjectId.size() << ':'
                  << value.refereeSubjectId
                  << '|' << value.finalPayloadSha256
                  << '|' << shot.size() << ':' << shot
                  << '|' << value.consumedAuthorityTick;
    }else canonical << "|double_hit_close_case_none";
    if(frozenBallDeclarationAuthority_&&
       frozenBallDeclarationAuthority_->state().committed){
        const auto& value=frozenBallDeclarationAuthority_->state();
        const std::string shot=replayStableShotIdentity(
            value.consumedShotIdentity);
        canonical << "|frozen_ball|" << static_cast<int>(value.phase)
                  << '|' << value.rackIdentity.size() << ':' << value.rackIdentity
                  << '|' << value.context.objectBallId
                  << '|' << value.context.parentRevision
                  << '|' << value.context.parentStateHash
                  << '|' << value.proposalDecisionId.size() << ':'
                  << value.proposalDecisionId
                  << '|' << value.refereeSubjectId.size() << ':'
                  << value.refereeSubjectId
                  << '|' << value.finalPayloadSha256
                  << '|' << shot.size() << ':' << shot
                  << '|' << value.consumedAuthorityTick;
    }else canonical << "|frozen_ball_none";
    if(frozenCushionDeclarationAuthority_&&
       frozenCushionDeclarationAuthority_->state().committed){
        const auto& value=frozenCushionDeclarationAuthority_->state();
        const std::string shot=replayStableShotIdentity(
            value.consumedShotIdentity);
        canonical << "|frozen_cushion|" << static_cast<int>(value.phase)
                  << '|' << value.rackIdentity.size() << ':' << value.rackIdentity
                  << '|' << value.context.objectBallId
                  << '|' << static_cast<int>(value.context.cushionId)
                  << '|' << value.context.parentRevision
                  << '|' << value.context.parentStateHash
                  << '|' << value.proposalDecisionId.size() << ':'
                  << value.proposalDecisionId
                  << '|' << value.refereeSubjectId.size() << ':'
                  << value.refereeSubjectId
                  << '|' << value.finalPayloadSha256
                  << '|' << shot.size() << ':' << shot
                  << '|' << value.consumedAuthorityTick;
    }else canonical << "|frozen_cushion_none";
    if(version==RuntimeRulesReplayEffectVersion::SingleRackDeclarationsV3)
        return authoritySha256Hex(canonical.str());

    if(unintentionalBreakMiscueDeclarationAuthority_&&
       unintentionalBreakMiscueDeclarationAuthority_->state().committed){
        const auto& value=
            unintentionalBreakMiscueDeclarationAuthority_->state();
        const std::string shot=replayStableShotIdentity(
            value.consumedShotIdentity);
        canonical << "|unintentional_break_miscue|"
                  << static_cast<int>(value.phase)
                  << '|' << value.rackIdentity.size() << ':'
                  << value.rackIdentity
                  << '|' << value.context.actorPlayerId
                  << '|' << value.context.parentRevision
                  << '|' << value.context.parentStateHash
                  << '|' << value.proposalDecisionId.size() << ':'
                  << value.proposalDecisionId
                  << '|' << value.refereeSubjectId.size() << ':'
                  << value.refereeSubjectId
                  << '|' << value.finalPayloadSha256
                  << '|' << shot.size() << ':' << shot
                  << '|' << value.consumedAuthorityTick;
        if (value.phase == rules::
                UnintentionalBreakMiscueDeclarationPhase::Consumed) {
            // V4 binds the exact modeled stroke and physics profile at the
            // consume boundary.  The immutable digest cannot drift if a
            // later runtime action replaces cueImpactInput_.
            canonical << "|consuming_cue_input_binding|"
                      << value.consumedStrokeBindingSha256.size() << ':'
                      << value.consumedStrokeBindingSha256;
        }
    }
    return authoritySha256Hex(canonical.str());
}

std::string GameRuntime::rulesAuthorityStateHash() const
{
    std::ostringstream canonical;
    canonical.imbue(std::locale::classic());
    canonical << std::setprecision(
        std::numeric_limits<float>::max_digits10);
    canonical << "rules_authority_state_v1"
              << '|' << rulesAuthorityRevision_
              << '|' << static_cast<int>(rulesMode_)
              << '|' << static_cast<int>(matchState_.phase)
              << '|' << matchState_.currentPlayer
              << '|' << static_cast<int>(matchState_.groups[0])
              << '|' << static_cast<int>(matchState_.groups[1])
              << '|' << (matchState_.ballInHand ? 1 : 0)
              << '|' << matchState_.winner
              << '|' << (matchState_.groupChoicePending ? 1 : 0)
              << '|' << static_cast<int>(ballInHandPlacementScope_)
              << '|' << authorityCanonicalZero(state_.balls[0].position.x)
              << '|' << authorityCanonicalZero(state_.balls[0].position.z)
              << '|' << (state_.balls[0].pocketed ? 1 : 0)
              << '|' << breakHeadStringOffenseAuthority_.stateDigest();
    canonical << "|rack_terminal"
              << '|' << (rulesRackTerminalState_.active ? 1 : 0)
              << '|' << static_cast<int>(rulesRackTerminalState_.reason)
              << '|' << rulesRackTerminalState_.losingPlayer
              << '|' << rulesRackTerminalState_.winningPlayer
              << '|' << rulesRackTerminalState_.rackIdentity.size() << ':'
              << rulesRackTerminalState_.rackIdentity
              << '|' << rulesRackTerminalState_.authorityDecisionId.size()
              << ':' << rulesRackTerminalState_.authorityDecisionId
              << '|' << rulesRackTerminalState_.committedAuthorityRevision;
    canonical << "|mobile_history|"
              << static_cast<int>(mobileDeviceViolationHistory_[0])
              << '|' << static_cast<int>(mobileDeviceViolationHistory_[1]);
    if (groupConfusionAuthority_) {
        const auto& confusion = groupConfusionAuthority_->state();
        canonical << "|group_confusion"
                  << '|' << static_cast<int>(confusion.phase)
                  << '|' << confusion.windowGeneration
                  << '|' << confusion.window.rackIdentity
                  << '|' << confusion.window.assignmentSnapshotSha256
                  << '|' << confusion.window.completedShotIdentity
                  << '|' << confusion.window.completedShotOrdinal
                  << '|' << confusion.window.completedShooter
                  << '|' << confusion.window.completedRevision
                  << '|' << confusion.window.completedStateHash
                  << '|' << confusion.rackOrderSnapshotSha256
                  << '|' << confusion.proposalDecisionId
                  << '|' << static_cast<int>(confusion.proposalActorRole)
                  << '|' << confusion.proposalActorSubjectId
                  << '|' << (confusion.finalAuthorized ? 1 : 0)
                  << '|' << (confusion.rerackCommitted ? 1 : 0);
    } else {
        canonical << "|group_confusion_none";
    }
    if (behindHeadStringFirstContactContext_) {
        canonical << "|head_string_first_contact"
                  << '|' <<
                    behindHeadStringFirstContactContext_->actorPlayerId
                  << '|' <<
                    behindHeadStringFirstContactContext_->placementDecisionId
                  << '|' <<
                    behindHeadStringFirstContactContext_->placementAckRevision
                  << '|' <<
                    authorityCanonicalZero(
                        behindHeadStringFirstContactContext_->headStringWorldZCm)
                  << '|' <<
                    behindHeadStringFirstContactContext_->observationId;
    } else {
        canonical << "|head_string_first_contact_none";
    }
    if (pendingBreakHeadStringRelease_) {
        const auto& sample = *pendingBreakHeadStringRelease_;
        canonical << "|head_string_pending"
                  << '|' << sample.shotIdentity
                  << '|' << sample.actorPlayerId
                  << '|' << authorityCanonicalZero(
                        sample.frozenCueBallCenterWorldXCm)
                  << '|' << authorityCanonicalZero(
                        sample.frozenCueBallCenterWorldZCm)
                  << '|' << authorityCanonicalZero(sample.headStringWorldZCm);
    } else {
        canonical << "|head_string_none";
    }
    if (groupChoiceDecisionOffer_) {
        const rules::GroupChoiceDecisionOffer& offer =
            *groupChoiceDecisionOffer_;
        canonical << '|' << offer.decisionId.size() << ':' << offer.decisionId
                  << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId
                  << '|' << offer.offeredActionIds[0].size() << ':'
                  << offer.offeredActionIds[0]
                  << '|' << offer.offeredActionIds[1].size() << ':'
                  << offer.offeredActionIds[1]
                  << '|' << (offer.continueTurn ? 1 : 0)
                  << '|' << offer.nextPlayer;
    } else {
        canonical << "|none";
    }
    if (ballInHandPlacementOffer_) {
        const rules::BallInHandPlacementOffer& offer =
            *ballInHandPlacementOffer_;
        canonical << '|' << offer.decisionId.size() << ':' << offer.decisionId
                  << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId
                  << '|' << static_cast<int>(offer.scope)
                  << '|' << authorityCanonicalZero(offer.headStringWorldZCm)
                  << '|' << offer.sourceClause.size() << ':'
                  << offer.sourceClause
                  << '|' << offer.sourcePdfPage
                  << '|' << offer.headStringDefinitionSourceClause.size()
                  << ':' << offer.headStringDefinitionSourceClause
                  << '|' << offer.headStringDefinitionPdfPage
                  << '|' << offer.offeredActionIds[0].size() << ':'
                  << offer.offeredActionIds[0];
    } else {
        canonical << "|none";
    }
    if (behindHeadStringRespotOffer_) {
        const auto& offer = *behindHeadStringRespotOffer_;
        canonical << "|head_string_respot"
                  << '|' << offer.decisionId.size() << ':' << offer.decisionId
                  << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId
                  << '|' << offer.sourcePlacementDecisionId.size() << ':'
                  << offer.sourcePlacementDecisionId
                  << '|' << authorityCanonicalZero(offer.headStringWorldZCm)
                  << '|' << offer.tiedCandidateBallIds.size();
        for (int ball : offer.tiedCandidateBallIds) canonical << '|' << ball;
    } else {
        canonical << "|head_string_respot_none";
    }
    if (breakFoulDecisionOffer_) {
        const rules::BreakFoulDecisionOffer& offer =
            *breakFoulDecisionOffer_;
        canonical << '|' << offer.decisionId.size() << ':'
                  << offer.decisionId
                  << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId
                  << '|' << offer.originalBreakerPlayerId
                  << '|' << offer.frozenNextPlayer
                  << '|' << static_cast<int>(offer.branch)
                  << '|' << offer.sourceClause.size() << ':'
                  << offer.sourceClause
                  << '|' << offer.sourcePdfPage
                  << '|' << offer.offeredActionIds.size();
        for (const std::string& action : offer.offeredActionIds)
            canonical << '|' << action.size() << ':' << action;
    } else {
        canonical << "|none";
    }
    if (eightOnBreakDecisionOffer_) {
        const rules::EightOnBreakDecisionOffer& offer =
            *eightOnBreakDecisionOffer_;
        canonical << '|' << offer.decisionId.size() << ':'
                  << offer.decisionId
                  << '|' << offer.authorityStateRevision
                  << '|' << offer.actorPlayerId
                  << '|' << offer.originalBreakerPlayerId
                  << '|' << offer.frozenNextPlayer
                  << '|' << (offer.frozenContinueTurn ? 1 : 0)
                  << '|' << static_cast<int>(offer.branch)
                  << '|' << offer.offeredActionIds.size();
        for (const std::string& action : offer.offeredActionIds)
            canonical << '|' << action.size() << ':' << action;
    } else {
        canonical << "|none";
    }
    for (int index = 0; index < kBallCount; ++index) {
        const BallState& ball = state_.balls[index];
        canonical << '|' << index
                  << ':' << authorityCanonicalZero(ball.position.x)
                  << ':' << authorityCanonicalZero(ball.position.y)
                  << ':' << authorityCanonicalZero(ball.position.z)
                  << ':' << authorityCanonicalZero(ball.velocity.x)
                  << ':' << authorityCanonicalZero(ball.velocity.y)
                  << ':' << authorityCanonicalZero(ball.velocity.z)
                  << ':' << authorityCanonicalZero(ball.angularVelocity.x)
                  << ':' << authorityCanonicalZero(ball.angularVelocity.y)
                  << ':' << authorityCanonicalZero(ball.angularVelocity.z)
                  << ':' << authorityCanonicalZero(ball.speed)
                  << ':' << static_cast<int>(ball.motionState)
                  << ':' << (ball.pocketed ? 1 : 0)
                  << ':' << static_cast<int>(ball.pocketInteraction.phase)
                  << ':' << ball.pocketInteraction.pocketId
                  << ':' << ball.pocketInteraction.captureSequence;
        if (ball.outOfPlay) canonical << ":out_of_play";
    }
    return authoritySha256Hex(canonical.str());
}

rules::GroupChoiceDecisionResult GameRuntime::submitGroupChoiceDecision(
    const rules::GroupChoiceDecisionRequest& request)
{
    auto rejected = [](const char* code) {
        rules::GroupChoiceDecisionResult result;
        result.errorCode = code;
        return result;
    };
    if (groupConfusionAuthority_ && groupConfusionAuthority_->rackStopped())
        return rejected("rules_group_confusion_rack_stopped");

    if (request.idempotencyKey.empty() ||
        request.idempotencyKey.size() > 128)
        return rejected("rules_decision_idempotency_key_invalid");

    const auto receipt = groupChoiceReceipts_.find(request.idempotencyKey);
    if (receipt != groupChoiceReceipts_.end()) {
        if (!sameGroupChoiceRequest(receipt->second.request, request))
            return rejected("rules_decision_idempotency_conflict");
        rules::GroupChoiceDecisionResult result;
        result.ok = true;
        result.ack = receipt->second.ack;
        return result;
    }

    if (request.rulesetId != rules::kGroupChoiceRulesetId ||
        request.capabilityId != rules::kGroupChoiceAuthorityCapabilityId ||
        request.flowId != rules::kGroupChoiceAuthorityFlowId)
        return rejected("rules_decision_context_mismatch");

    if (!groupChoiceDecisionOffer_) {
        if (staleGroupChoiceDecisionIds_.count(request.decisionId) != 0)
            return rejected("rules_decision_stale");
        return rejected("rules_group_choice_not_pending");
    }
    const rules::GroupChoiceDecisionOffer offer = *groupChoiceDecisionOffer_;
    if (request.decisionId != offer.decisionId)
        return rejected("rules_decision_stale");
    if (request.expectedAuthorityStateRevision !=
            offer.authorityStateRevision ||
        request.expectedAuthorityStateRevision != rulesAuthorityRevision_)
        return rejected("rules_decision_stale_revision");
    if (request.actorPlayerId != offer.actorPlayerId)
        return rejected("rules_decision_actor_mismatch");
    if (request.actionId != offer.offeredActionIds[0] &&
        request.actionId != offer.offeredActionIds[1])
        return rejected("rules_decision_action_not_offered");
    if (request.payloadCanonical != "{}")
        return rejected("rules_decision_invalid_payload");
    if (state_.ballsMoving || shotFactCollector_.active())
        return rejected("rules_decision_invalid_state");
    if (groupChoiceOfferStateHash_.empty() ||
        rulesAuthorityStateHash() != groupChoiceOfferStateHash_)
        return rejected("rules_group_choice_state_changed");

    const std::string replayParentEffect=rulesReplayEffectStateHash();
    const std::string replayRackIdentity=currentRulesReplayRackIdentity();
    if(replayRulesRackIdentity_.empty())
        replayRulesRackIdentity_=replayRackIdentity;

    const rules::BallGroup chosen =
        request.actionId == rules::kGroupChoiceSolidsActionId ?
            rules::BallGroup::Solids : rules::BallGroup::Stripes;
    const rules::BallGroup other = chosen == rules::BallGroup::Solids ?
        rules::BallGroup::Stripes : rules::BallGroup::Solids;
    matchState_.groups[offer.actorPlayerId] = chosen;
    matchState_.groups[1 - offer.actorPlayerId] = other;
    matchState_.phase = rules::MatchPhase::GroupsAssigned;
    matchState_.groupChoicePending = false;
    for (int player = 0; player < 2; ++player) {
        state_.players.assignedBallType[player] =
            matchState_.groups[player] == rules::BallGroup::Solids ? 0 : 1;
    }
    state_.players.firstPocketedObjectBall = false;
    if (hasLastAdjudication_) {
        lastAdjudication_.groupChoiceRequired = false;
        lastAdjudication_.decisionOptions.clear();
        lastAdjudication_.nextState = matchState_;
    }

    const std::uint64_t previousRevision = rulesAuthorityRevision_;
    ++rulesAuthorityRevision_;
    groupChoiceDecisionOffer_.reset();
    groupChoiceOfferStateHash_.clear();
    staleGroupChoiceDecisionIds_.insert(offer.decisionId);

    rules::GroupChoiceDecisionAck ack;
    ack.accepted = true;
    ack.decisionId = request.decisionId;
    ack.actionId = request.actionId;
    ack.idempotencyKey = request.idempotencyKey;
    ack.actorPlayerId = request.actorPlayerId;
    ack.previousStateRevision = previousRevision;
    ack.authoritativeStateRevision = rulesAuthorityRevision_;
    ack.effects.currentPlayer = matchState_.currentPlayer;
    ack.effects.nextPlayer = offer.nextPlayer;
    ack.effects.turnContinues = offer.continueTurn;
    ack.effects.ballInHand = matchState_.ballInHand;
    ack.effects.groups = matchState_.groups;
    ack.effects.phase = matchState_.phase;
    ack.effects.winner = matchState_.winner;
    ack.effects.groupChoicePending = matchState_.groupChoicePending;
    ack.authoritativeStateHash = rulesAuthorityStateHash();

    events_.push_back(RuntimeEvent{
        nextSequence_++, tick_, "rules_group_choice_committed"});
    rulesAuthorityActions_.push_back(rules::RulesAuthorityAction{
        nextRulesAuthorityActionSequence_++, tick_, request.decisionId,
        request.actionId, request.idempotencyKey, request.actorPlayerId,
        previousRevision, rulesAuthorityRevision_, ack.authoritativeStateHash,
        request.payloadCanonical});
    groupChoiceReceipts_[request.idempotencyKey] =
        GroupChoiceReceipt{request, ack};
    if(!replayRulesImportActive_){
        RuntimeRulesReplayEvent event;
        event.sequence=nextRulesReplayEventSequence_++;
        event.tick=tick_;
        event.afterAppliedSemanticActionSequence=
            appliedSemanticActions_.empty()?0:
                appliedSemanticActions_.back().sequence;
        event.type=RuntimeRulesReplayEventType::GroupChoiceCommit;
        event.rackIdentity=replayRackIdentity;
        event.decisionId=request.decisionId;
        event.actionId=request.actionId;
        event.actorPlayerId=request.actorPlayerId;
        event.previousRulesRevision=previousRevision;
        event.committedRulesRevision=rulesAuthorityRevision_;
        event.parentEffectSha256=replayParentEffect;
        event.committedEffectSha256=rulesReplayEffectStateHash();
        rulesReplayEvents_.push_back(std::move(event));
        while(rulesReplayEvents_.size()>kRulesReplayEventJournalCapacity)
            rulesReplayEvents_.pop_front();
    }

    rules::GroupChoiceDecisionResult result;
    result.ok = true;
    result.ack = ack;
    return result;
}

rules::BallInHandPlacementValidation
GameRuntime::validateBallInHandPlacement(
    const rules::BallInHandPlacementPosition& position) const
{
    auto rejected = [](const char* code) {
        rules::BallInHandPlacementValidation result;
        result.errorCode = code;
        return result;
    };
    if (ballInHandPlacementScope_ ==
            rules::BallInHandPlacementScope::None ||
        !ballInHandPlacementOffer_ || !matchState_.ballInHand)
        return rejected("rules_placement_not_pending");
    if (state_.ballsMoving || shotFactCollector_.active())
        return rejected("rules_decision_invalid_state");
    if (!std::isfinite(position.worldX) ||
        !std::isfinite(position.worldZ))
        return rejected("rules_placement_nonfinite");

    const double radius = physicsProfile_.ball.radiusCm;
    const double xLimit =
        physicsProfile_.tableBoundary.playfieldWidthCm * 0.5 - radius;
    const double zLimit =
        physicsProfile_.tableBoundary.playfieldLengthCm * 0.5 - radius;
    if (std::fabs(position.worldX) > xLimit ||
        std::fabs(position.worldZ) > zLimit)
        return rejected("rules_placement_out_of_bounds");

    Point3 candidate{
        static_cast<float>(position.worldX),
        kTableHeight + physicsProfile_.ball.radiusCm,
        static_cast<float>(position.worldZ)};
    if (ballInHandPlacementScope_ ==
            rules::BallInHandPlacementScope::BehindHeadString &&
        candidate.z >= ballInHandPlacementOffer_->headStringWorldZCm)
        return rejected("rules_placement_outside_head_string_area");
    const std::array<PocketBoundaryFrame, 6> pocketFrames =
        buildPocketBoundaryFrames(physicsProfile_.tableBoundary);
    for (const PocketBoundaryFrame& frame : pocketFrames) {
        const PocketBoundaryQuery query = classifyPocketPoint(
            frame, candidate, radius);
        if (query.passable &&
            query.local.depthCm >= -2.0 * radius &&
            query.local.depthCm <= frame.captureDepthCm)
            return rejected("rules_placement_forbidden_geometry");
        const double effectiveJawRadius = frame.jawRadiusCm + radius;
        for (int side = -1; side <= 1; side += 2) {
            const Point3 jaw = pocketJawCenter(frame, side);
            const double dx = position.worldX - jaw.x;
            const double dz = position.worldZ - jaw.z;
            if (query.local.depthCm >= -effectiveJawRadius &&
                query.local.depthCm <=
                    frame.throatDepthCm + effectiveJawRadius &&
                dx * dx + dz * dz <
                    effectiveJawRadius * effectiveJawRadius)
                return rejected("rules_placement_forbidden_geometry");
        }
    }

    const double minimumSeparation = radius * 2.0;
    const double minimumSeparationSquared =
        minimumSeparation * minimumSeparation;
    for (int index = 1; index < kBallCount; ++index) {
        const BallState& ball = state_.balls[index];
        if (ball.pocketed || ball.outOfPlay) continue;
        if (!std::isfinite(ball.position.x) ||
            !std::isfinite(ball.position.z))
            return rejected("rules_placement_authority_geometry_invalid");
        const double dx = position.worldX - ball.position.x;
        const double dz = position.worldZ - ball.position.z;
        if (dx * dx + dz * dz < minimumSeparationSquared - 1e-9)
            return rejected("rules_placement_overlap");
    }

    rules::BallInHandPlacementValidation result;
    result.ok = true;
    result.authoritativePosition.worldX = candidate.x;
    result.authoritativePosition.worldZ = candidate.z;
    return result;
}

rules::BallInHandPlacementResult GameRuntime::submitBallInHandPlacement(
    const rules::BallInHandPlacementRequest& request)
{
    auto rejected = [](const char* code) {
        rules::BallInHandPlacementResult result;
        result.errorCode = code;
        return result;
    };
    if (groupConfusionAuthority_ && groupConfusionAuthority_->rackStopped())
        return rejected("rules_group_confusion_rack_stopped");
    if (request.idempotencyKey.empty() ||
        request.idempotencyKey.size() > 128)
        return rejected("rules_decision_idempotency_key_invalid");

    const auto receipt = ballInHandPlacementReceipts_.find(
        request.idempotencyKey);
    if (receipt != ballInHandPlacementReceipts_.end()) {
        if (!sameBallInHandPlacementRequest(
                receipt->second.request, request))
            return rejected("rules_decision_idempotency_conflict");
        rules::BallInHandPlacementResult result;
        result.ok = true;
        result.ack = receipt->second.ack;
        return result;
    }

    if (behindHeadStringRespotOffer_)
        return rejected("rules_head_string_respot_choice_pending");
    if (!ballInHandPlacementOffer_) {
        if (staleBallInHandDecisionIds_.count(request.decisionId) != 0)
            return rejected("rules_decision_stale");
        return rejected("rules_placement_not_pending");
    }
    const rules::BallInHandPlacementOffer offer =
        *ballInHandPlacementOffer_;
    const char* expectedFlow = offer.scope ==
            rules::BallInHandPlacementScope::BehindHeadString ?
        rules::kBehindHeadStringBallInHandFlowId :
        rules::kFullTableBallInHandFlowId;
    if (request.rulesetId != rules::kBallInHandRulesetId ||
        request.capabilityId != rules::kFreeBallPlacementCapabilityId ||
        request.flowId != expectedFlow)
        return rejected("rules_decision_context_mismatch");
    if (request.decisionId != offer.decisionId)
        return rejected("rules_decision_stale");
    if (request.expectedAuthorityStateRevision !=
            offer.authorityStateRevision ||
        request.expectedAuthorityStateRevision != rulesAuthorityRevision_)
        return rejected("rules_decision_stale_revision");
    if (request.actorPlayerId != offer.actorPlayerId)
        return rejected("rules_decision_actor_mismatch");
    if (request.actionId != offer.offeredActionIds[0])
        return rejected("rules_decision_action_not_offered");
    if (ballInHandPlacementOfferStateHash_.empty() ||
        rulesAuthorityStateHash() != ballInHandPlacementOfferStateHash_)
        return rejected("rules_placement_state_changed");
    const rules::BallInHandPlacementValidation validation =
        validateBallInHandPlacement(request.position);
    if (!validation.ok) return rejected(validation.errorCode.c_str());
    const std::string replayParentEffect=rulesReplayEffectStateHash();
    const std::string replayRackIdentity=currentRulesReplayRackIdentity();
    if(replayRulesRackIdentity_.empty())
        replayRulesRackIdentity_=replayRackIdentity;

    BallState cueBall = state_.balls[0];
    cueBall.position = Point3{
        static_cast<float>(validation.authoritativePosition.worldX),
        kTableHeight + physicsProfile_.ball.radiusCm,
        static_cast<float>(validation.authoritativePosition.worldZ)};
    cueBall.pocketed = false;
    cueBall.physicalDomain = PhysicalBallDomain::ClothSupported;
    cueBall.outOfPlay = false;
    resetBallMotion(cueBall);
    resetPocketInteraction(cueBall);
    state_.balls[0] = cueBall;
    state_.ballsMoving = false;
    matchState_.ballInHand = true;
    ballInHandPlacementScope_ = offer.scope;
    if (hasLastAdjudication_)
        lastAdjudication_.nextState = matchState_;

    const std::uint64_t previousRevision = rulesAuthorityRevision_;
    ++rulesAuthorityRevision_;
    ballInHandPlacementOffer_.reset();
    ballInHandPlacementOfferStateHash_.clear();
    staleBallInHandDecisionIds_.insert(offer.decisionId);
    stageBallInHandPlacementAuthority(offer.scope, offer.actorPlayerId);
    ballInHandPlacementOfferStateHash_ = rulesAuthorityStateHash();
    if (state_.camera.anchorMode == CameraAnchorMode::FollowCueBall)
        updateCameraFromCueBall(state_);

    rules::BallInHandPlacementAck ack;
    ack.accepted = true;
    ack.decisionId = request.decisionId;
    ack.actionId = request.actionId;
    ack.idempotencyKey = request.idempotencyKey;
    ack.actorPlayerId = request.actorPlayerId;
    ack.previousStateRevision = previousRevision;
    ack.authoritativeStateRevision = rulesAuthorityRevision_;
    ack.effects.currentPlayer = matchState_.currentPlayer;
    ack.effects.nextPlayer = matchState_.currentPlayer;
    ack.effects.turnContinues = false;
    ack.effects.ballInHand = matchState_.ballInHand;
    ack.effects.ballInHandScope = ballInHandPlacementScope_;
    ack.effects.placementPending = ballInHandPlacementPending();
    ack.effects.groups = matchState_.groups;
    ack.effects.phase = matchState_.phase;
    ack.effects.winner = matchState_.winner;
    ack.effects.cueBallPosition = validation.authoritativePosition;
    ack.replacementPlacementDecisionId =
        ballInHandPlacementOffer_->decisionId;
    ack.authoritativeStateHash = ballInHandPlacementOfferStateHash_;

    events_.push_back(RuntimeEvent{
        nextSequence_++, tick_, "rules_ball_in_hand_placement_committed"});
    std::ostringstream payload;
    payload.imbue(std::locale::classic());
    payload << std::setprecision(
        std::numeric_limits<double>::max_digits10)
        << "x=" << request.position.worldX
        << ";z=" << request.position.worldZ;
    rulesAuthorityActions_.push_back(rules::RulesAuthorityAction{
        nextRulesAuthorityActionSequence_++, tick_, request.decisionId,
        request.actionId, request.idempotencyKey, request.actorPlayerId,
        previousRevision, rulesAuthorityRevision_,
        ack.authoritativeStateHash, payload.str()});
    if(!replayRulesImportActive_){
        RuntimeRulesReplayEvent event;
        event.sequence=nextRulesReplayEventSequence_++;
        event.tick=tick_;
        event.afterAppliedSemanticActionSequence=
            appliedSemanticActions_.empty()?0:
                appliedSemanticActions_.back().sequence;
        event.type=RuntimeRulesReplayEventType::BallInHandPlacementCommit;
        event.rackIdentity=replayRackIdentity;
        event.decisionId=request.decisionId;
        event.actionId=request.actionId;
        event.actorPlayerId=request.actorPlayerId;
        event.worldX=validation.authoritativePosition.worldX;
        event.worldZ=validation.authoritativePosition.worldZ;
        event.previousRulesRevision=previousRevision;
        event.committedRulesRevision=rulesAuthorityRevision_;
        event.parentEffectSha256=replayParentEffect;
        event.committedEffectSha256=rulesReplayEffectStateHash();
        rulesReplayEvents_.push_back(std::move(event));
        while(rulesReplayEvents_.size()>kRulesReplayEventJournalCapacity)
            rulesReplayEvents_.pop_front();
    }
    ballInHandPlacementReceipts_[request.idempotencyKey] =
        BallInHandPlacementReceipt{request, ack};

    rules::BallInHandPlacementResult result;
    result.ok = true;
    result.ack = ack;
    return result;
}

rules::BehindHeadStringRespotResult
GameRuntime::submitBehindHeadStringRespot(
    const rules::BehindHeadStringRespotRequest& request)
{
    auto rejected = [](const std::string& code) {
        rules::BehindHeadStringRespotResult result;
        result.errorCode = code;
        return result;
    };
    if (request.idempotencyKey.empty() || request.idempotencyKey.size() > 128)
        return rejected("rules_decision_idempotency_key_invalid");
    const auto retained = behindHeadStringRespotReceipts_.find(
        request.idempotencyKey);
    if (retained != behindHeadStringRespotReceipts_.end()) {
        if (!sameBehindHeadStringRespotRequest(retained->second.request, request))
            return rejected("rules_decision_idempotency_conflict");
        rules::BehindHeadStringRespotResult result;
        result.ok = true;
        result.ack = retained->second.ack;
        return result;
    }
    if (request.rulesetId != rules::kBallInHandRulesetId ||
        request.capabilityId != rules::kBehindHeadStringRespotCapabilityId ||
        request.flowId != rules::kBehindHeadStringRespotFlowId)
        return rejected("rules_decision_context_mismatch");
    if (rulesMode_ != RulesMode::ChineseEightBallV1 ||
        state_.ballsMoving || shotFactCollector_.active())
        return rejected("rules_decision_invalid_state");

    const bool choosing = request.actionId ==
        rules::kChooseEquidistantRespotBallActionId;
    rules::BallInHandPlacementOffer placement;
    std::vector<int> frozenTie;
    if (choosing) {
        if (!behindHeadStringRespotOffer_) {
            if (staleBehindHeadStringRespotDecisionIds_.count(
                    request.decisionId) != 0)
                return rejected("rules_decision_stale");
            return rejected("rules_head_string_respot_choice_not_pending");
        }
        const auto& offer = *behindHeadStringRespotOffer_;
        if (request.decisionId != offer.decisionId)
            return rejected("rules_decision_stale");
        if (request.actorPlayerId != offer.actorPlayerId)
            return rejected("rules_decision_actor_mismatch");
        if (request.expectedAuthorityStateRevision != rulesAuthorityRevision_ ||
            request.expectedAuthorityStateRevision != offer.authorityStateRevision)
            return rejected("rules_decision_stale_revision");
        if (request.expectedAuthorityStateHash !=
                behindHeadStringRespotOfferStateHash_ ||
            request.expectedAuthorityStateHash != rulesAuthorityStateHash())
            return rejected("rules_head_string_respot_state_changed");
        if (std::find(offer.tiedCandidateBallIds.begin(),
                offer.tiedCandidateBallIds.end(), request.selectedBallId) ==
            offer.tiedCandidateBallIds.end())
            return rejected("rules_decision_action_not_offered");
        std::ostringstream expectedPayload;
        expectedPayload << "{\"selected_ball_id\":"
                        << request.selectedBallId << '}';
        if (request.payloadCanonical != expectedPayload.str())
            return rejected("rules_decision_invalid_payload");
        if (!ballInHandPlacementOffer_ ||
            ballInHandPlacementOffer_->decisionId !=
                offer.sourcePlacementDecisionId)
            return rejected("rules_head_string_respot_state_changed");
        placement = *ballInHandPlacementOffer_;
        frozenTie = offer.tiedCandidateBallIds;
    } else {
        if (request.actionId != rules::kRequestNearestLegalRespotActionId)
            return rejected("rules_decision_action_not_offered");
        if (request.payloadCanonical != "{}" || request.selectedBallId != -1)
            return rejected("rules_decision_invalid_payload");
        if (!ballInHandPlacementOffer_ ||
            ballInHandPlacementOffer_->scope !=
                rules::BallInHandPlacementScope::BehindHeadString ||
            !matchState_.ballInHand)
            return rejected("rules_ball_in_hand_behind_head_string_not_pending");
        placement = *ballInHandPlacementOffer_;
        if (request.decisionId != placement.decisionId)
            return rejected("rules_decision_stale");
        if (request.actorPlayerId != placement.actorPlayerId)
            return rejected("rules_decision_actor_mismatch");
        if (request.expectedAuthorityStateRevision != rulesAuthorityRevision_ ||
            request.expectedAuthorityStateRevision !=
                placement.authorityStateRevision)
            return rejected("rules_decision_stale_revision");
        if (request.expectedAuthorityStateHash !=
                ballInHandPlacementOfferStateHash_ ||
            request.expectedAuthorityStateHash != rulesAuthorityStateHash())
            return rejected("rules_head_string_respot_state_changed");
    }

    const rules::BallGroup group =
        matchState_.groups[placement.actorPlayerId];
    bool groupCleared = false;
    if (group != rules::BallGroup::Open) {
        groupCleared = true;
        for (int ball = 1; ball < kBallCount; ++ball) {
            if (state_.balls[ball].pocketed || state_.balls[ball].outOfPlay) continue;
            const bool inGroup = group == rules::BallGroup::Solids ?
                ball >= 1 && ball <= 7 : ball >= 9 && ball <= 15;
            if (inGroup) { groupCleared = false; break; }
        }
    }
    std::vector<int> legal;
    for (int ball = 1; ball < kBallCount; ++ball) {
        if (state_.balls[ball].pocketed || state_.balls[ball].outOfPlay) continue;
        if (!std::isfinite(state_.balls[ball].position.z))
            return rejected("rules_head_string_respot_geometry_invalid");
        if (rules::isLegalFirstContactBall(matchState_, ball, groupCleared))
            legal.push_back(ball);
    }
    if (legal.empty())
        return rejected("rules_head_string_no_legal_object_ball");
    for (int ball : legal) {
        if (state_.balls[ball].position.z >= placement.headStringWorldZCm)
            return rejected("rules_head_string_not_all_legal_balls_behind");
    }
    double closestZ = -std::numeric_limits<double>::infinity();
    std::vector<int> candidates;
    for (int ball : legal) {
        const double z = state_.balls[ball].position.z;
        if (z > closestZ) { closestZ = z; candidates.assign(1, ball); }
        else if (z == closestZ) candidates.push_back(ball);
    }
    std::sort(candidates.begin(), candidates.end());
    if (choosing && candidates != frozenTie)
        return rejected("rules_head_string_respot_state_changed");
    const std::string replayParentEffect=rulesReplayEffectStateHash();
    const std::string replayRackIdentity=currentRulesReplayRackIdentity();
    if(replayRulesRackIdentity_.empty())
        replayRulesRackIdentity_=replayRackIdentity;
    const auto appendReplayReceipt=[&](std::uint64_t previousRevision,
                                       int selectedBall){
        if(replayRulesImportActive_)return;
        RuntimeRulesReplayEvent event;
        event.sequence=nextRulesReplayEventSequence_++;
        event.tick=tick_;
        event.afterAppliedSemanticActionSequence=
            appliedSemanticActions_.empty()?0:
                appliedSemanticActions_.back().sequence;
        event.type=RuntimeRulesReplayEventType::
            BehindHeadStringRespotCommit;
        event.rackIdentity=replayRackIdentity;
        event.decisionId=request.decisionId;
        event.actionId=request.actionId;
        event.actorPlayerId=request.actorPlayerId;
        event.selectedBallId=selectedBall;
        event.previousRulesRevision=previousRevision;
        event.committedRulesRevision=rulesAuthorityRevision_;
        event.parentEffectSha256=replayParentEffect;
        event.committedEffectSha256=rulesReplayEffectStateHash();
        rulesReplayEvents_.push_back(std::move(event));
        while(rulesReplayEvents_.size()>kRulesReplayEventJournalCapacity)
            rulesReplayEvents_.pop_front();
    };
    if (!choosing && candidates.size() > 1) {
        const std::uint64_t previousRevision = rulesAuthorityRevision_;
        ++rulesAuthorityRevision_;
        ballInHandPlacementOffer_->authorityStateRevision = rulesAuthorityRevision_;
        rules::BehindHeadStringRespotOffer offer;
        offer.authorityStateRevision = rulesAuthorityRevision_;
        offer.actorPlayerId = placement.actorPlayerId;
        offer.sourcePlacementDecisionId = placement.decisionId;
        offer.headStringWorldZCm = placement.headStringWorldZCm;
        offer.tiedCandidateBallIds = candidates;
        std::ostringstream id;
        id << "rules.ball_in_hand.all_behind_respot."
           << rulesAuthorityRevision_ << '.'
           << nextBehindHeadStringRespotDecisionOrdinal_++;
        offer.decisionId = id.str();
        behindHeadStringRespotOffer_ = offer;
        ballInHandPlacementOfferStateHash_ = rulesAuthorityStateHash();
        behindHeadStringRespotOfferStateHash_ =
            ballInHandPlacementOfferStateHash_;
        rules::BehindHeadStringRespotAck ack;
        ack.accepted = true;
        ack.tieChoiceStaged = true;
        ack.decisionId = request.decisionId;
        ack.actionId = request.actionId;
        ack.idempotencyKey = request.idempotencyKey;
        ack.actorPlayerId = request.actorPlayerId;
        ack.tiedCandidateBallIds = candidates;
        ack.previousStateRevision = previousRevision;
        ack.authoritativeStateRevision = rulesAuthorityRevision_;
        ack.replacementPlacementDecisionId = placement.decisionId;
        ack.authoritativeStateHash = behindHeadStringRespotOfferStateHash_;
        behindHeadStringRespotReceipts_[request.idempotencyKey] = {request, ack};
        appendReplayReceipt(previousRevision,-1);
        rules::BehindHeadStringRespotResult result;
        result.ok = true; result.ack = ack; return result;
    }

    const int selected = choosing ? request.selectedBallId : candidates.front();
    rules::RespotFoundationParameters parameters;
    parameters.ball = selected;
    parameters.nominalPosition = Point3{0.0f,
        kTableHeight + physicsProfile_.ball.radiusCm,
        static_cast<float>(
            physicsProfile_.tableBoundary.playfieldLengthCm * 0.25)};
    const auto plan = rules::buildRespotFoundationPlan(state_, physicsProfile_,
        parameters, rulesAuthorityRevision_, rulesAuthorityStateHash());
    if (!plan.ok) return rejected(plan.errorCode);
    const auto verified = rules::buildRespotFoundationPlan(state_, physicsProfile_,
        parameters, rulesAuthorityRevision_, rulesAuthorityStateHash());
    if (!verified.ok) return rejected(verified.errorCode);
    if (!sameRackFoundationPlan(plan, verified))
        return rejected("rules_foundation_plan_mismatch");

    const std::uint64_t previousRevision = rulesAuthorityRevision_;
    applyVerifiedRespotFoundation(verified);
    ++rulesAuthorityRevision_;
    if (behindHeadStringRespotOffer_)
        staleBehindHeadStringRespotDecisionIds_.insert(
            behindHeadStringRespotOffer_->decisionId);
    behindHeadStringRespotOffer_.reset();
    behindHeadStringRespotOfferStateHash_.clear();
    staleBallInHandDecisionIds_.insert(placement.decisionId);
    ballInHandPlacementOffer_.reset();
    ballInHandPlacementOfferStateHash_.clear();
    stageBallInHandPlacementAuthority(
        rules::BallInHandPlacementScope::BehindHeadString,
        placement.actorPlayerId);
    ballInHandPlacementOfferStateHash_ = rulesAuthorityStateHash();

    rules::BehindHeadStringRespotAck ack;
    ack.accepted = true; ack.respotCommitted = true;
    ack.decisionId = request.decisionId; ack.actionId = request.actionId;
    ack.idempotencyKey = request.idempotencyKey;
    ack.actorPlayerId = request.actorPlayerId; ack.selectedBallId = selected;
    ack.tiedCandidateBallIds = candidates;
    ack.previousStateRevision = previousRevision;
    ack.authoritativeStateRevision = rulesAuthorityRevision_;
    ack.replacementPlacementDecisionId = ballInHandPlacementOffer_->decisionId;
    ack.authoritativeStateHash = ballInHandPlacementOfferStateHash_;
    events_.push_back(RuntimeEvent{nextSequence_++, tick_,
        "rules_head_string_all_behind_respot_committed"});
    rulesAuthorityActions_.push_back(rules::RulesAuthorityAction{
        nextRulesAuthorityActionSequence_++, tick_, request.decisionId,
        request.actionId, request.idempotencyKey, request.actorPlayerId,
        previousRevision, rulesAuthorityRevision_, ack.authoritativeStateHash,
        request.payloadCanonical});
    behindHeadStringRespotReceipts_[request.idempotencyKey] = {request, ack};
    appendReplayReceipt(previousRevision,selected);
    rules::BehindHeadStringRespotResult result;
    result.ok = true; result.ack = ack; return result;
}

rules::BreakFoulDecisionResult GameRuntime::submitBreakFoulDecision(
    const rules::BreakFoulDecisionRequest& request)
{
    auto rejected = [](const std::string& code) {
        rules::BreakFoulDecisionResult result;
        result.errorCode = code;
        return result;
    };
    if (request.idempotencyKey.empty() ||
        request.idempotencyKey.size() > 128)
        return rejected("rules_decision_idempotency_key_invalid");

    const auto receipt = breakFoulReceipts_.find(request.idempotencyKey);
    if (receipt != breakFoulReceipts_.end()) {
        if (!sameBreakFoulRequest(receipt->second.request, request))
            return rejected("rules_decision_idempotency_conflict");
        rules::BreakFoulDecisionResult result;
        result.ok = true;
        result.ack = receipt->second.ack;
        return result;
    }
    if (state_.gameOver ||
        matchState_.phase == rules::MatchPhase::Complete)
        return rejected("game_over");
    if (request.rulesetId != rules::kBreakFoulRulesetId ||
        request.capabilityId != rules::kBreakFoulCapabilityId ||
        request.flowId != rules::kBreakFoulFlowId)
        return rejected("rules_decision_context_mismatch");
    if (!breakFoulDecisionOffer_) {
        if (staleBreakFoulDecisionIds_.count(request.decisionId) != 0)
            return rejected("rules_decision_stale");
        return rejected("rules_break_foul_not_pending");
    }
    const rules::BreakFoulDecisionOffer offer =
        *breakFoulDecisionOffer_;
    if (request.decisionId != offer.decisionId)
        return rejected("rules_decision_stale");
    if (request.expectedAuthorityStateRevision !=
            offer.authorityStateRevision ||
        request.expectedAuthorityStateRevision != rulesAuthorityRevision_)
        return rejected("rules_decision_stale_revision");
    if (request.actorPlayerId != offer.actorPlayerId)
        return rejected("rules_decision_actor_mismatch");
    if (std::find(offer.offeredActionIds.begin(),
            offer.offeredActionIds.end(), request.actionId) ==
        offer.offeredActionIds.end())
        return rejected("rules_decision_action_not_offered");
    if (request.payloadCanonical != "{}")
        return rejected("rules_decision_invalid_payload");
    if (state_.ballsMoving || shotFactCollector_.active())
        return rejected("rules_decision_invalid_state");
    const std::string currentHash = rulesAuthorityStateHash();
    if (breakFoulOfferStateHash_.empty() ||
        currentHash != breakFoulOfferStateHash_)
        return rejected("rules_break_foul_state_changed");

    const bool acceptTable =
        request.actionId == rules::kBreakFoulAcceptTableActionId;
    const bool ballInHand =
        request.actionId == rules::kBreakFoulBallInHandActionId;
    const bool rerack = !acceptTable && !ballInHand;
    rules::RackFoundationPlan foundation;
    int breakerPlayerId = -1;
    if (rerack) {
        breakerPlayerId =
            request.actionId == rules::kBreakFoulRerackOriginalActionId ?
            offer.originalBreakerPlayerId :
            1 - offer.originalBreakerPlayerId;
        rules::RerackFoundationParameters parameters;
        parameters.breakerPlayerId = breakerPlayerId;
        foundation = rules::buildRerackFoundationPlan(
            state_, physicsProfile_, parameters, rulesAuthorityRevision_,
            currentHash);
        if (!foundation.ok) return rejected(foundation.errorCode);
    }

    const std::string replayParentEffect=rerack?std::string{}:
        rulesReplayEffectStateHash();
    const std::string replayRackIdentity=rerack?std::string{}:
        currentRulesReplayRackIdentity();
    if(!rerack&&replayRulesRackIdentity_.empty())
        replayRulesRackIdentity_=replayRackIdentity;

    const std::uint64_t previousRevision = rulesAuthorityRevision_;
    if (rerack) {
        applyVerifiedRerackFoundation(foundation);
    } else {
        matchState_.currentPlayer = offer.frozenNextPlayer;
        matchState_.ballInHand = ballInHand;
        state_.players.currentPlayer = offer.frozenNextPlayer;
        state_.players.nextPlayer = offer.frozenNextPlayer;
        ballInHandPlacementScope_ = ballInHand ?
            rules::BallInHandPlacementScope::BehindHeadString :
            rules::BallInHandPlacementScope::None;
        if (hasLastAdjudication_) {
            lastAdjudication_.rerackAllowed = false;
            lastAdjudication_.decisionOptions.clear();
            lastAdjudication_.ballInHand = ballInHand;
            lastAdjudication_.ballInHandBehindHeadString = ballInHand;
            lastAdjudication_.nextState = matchState_;
        }
        breakFoulDecisionOffer_.reset();
        breakFoulOfferStateHash_.clear();
        staleBreakFoulDecisionIds_.insert(offer.decisionId);
    }
    ++rulesAuthorityRevision_;
    if (ballInHand) {
        stageBallInHandPlacementAuthority(
            rules::BallInHandPlacementScope::BehindHeadString,
            offer.frozenNextPlayer);
        ballInHandPlacementOfferStateHash_ = rulesAuthorityStateHash();
    }

    rules::BreakFoulDecisionAck ack;
    ack.accepted = true;
    ack.decisionId = request.decisionId;
    ack.actionId = request.actionId;
    ack.idempotencyKey = request.idempotencyKey;
    ack.actorPlayerId = request.actorPlayerId;
    ack.previousStateRevision = previousRevision;
    ack.authoritativeStateRevision = rulesAuthorityRevision_;
    ack.effects.branch = offer.branch;
    ack.effects.tableAccepted = acceptTable;
    ack.effects.rackRebuilt = rerack;
    ack.effects.originalBreakerPlayerId = offer.originalBreakerPlayerId;
    ack.effects.breakerPlayerId = breakerPlayerId;
    ack.effects.currentPlayer = matchState_.currentPlayer;
    ack.effects.nextPlayer = state_.players.nextPlayer;
    ack.effects.ballInHand = matchState_.ballInHand;
    ack.effects.ballInHandScope = ballInHandPlacementScope_;
    ack.effects.childDecisionPending = ballInHandPlacementPending();
    ack.effects.groups = matchState_.groups;
    ack.effects.phase = matchState_.phase;
    ack.effects.winner = matchState_.winner;
    ack.authoritativeStateHash = rulesAuthorityStateHash();

    events_.push_back(RuntimeEvent{nextSequence_++, tick_,
        "rules_break_foul_choice_committed"});
    rulesAuthorityActions_.push_back(rules::RulesAuthorityAction{
        nextRulesAuthorityActionSequence_++, tick_, request.decisionId,
        request.actionId, request.idempotencyKey, request.actorPlayerId,
        previousRevision, rulesAuthorityRevision_,
        ack.authoritativeStateHash, request.payloadCanonical});
    breakFoulReceipts_[request.idempotencyKey] =
        BreakFoulReceipt{request, ack};
    if(!rerack&&!replayRulesImportActive_){
        RuntimeRulesReplayEvent event;
        event.sequence=nextRulesReplayEventSequence_++;
        event.tick=tick_;
        event.afterAppliedSemanticActionSequence=
            appliedSemanticActions_.empty()?0:
                appliedSemanticActions_.back().sequence;
        event.type=RuntimeRulesReplayEventType::BreakFoulChoiceCommit;
        event.rackIdentity=replayRackIdentity;
        event.decisionId=request.decisionId;
        event.actionId=request.actionId;
        event.actorPlayerId=request.actorPlayerId;
        event.previousRulesRevision=previousRevision;
        event.committedRulesRevision=rulesAuthorityRevision_;
        event.parentEffectSha256=replayParentEffect;
        event.committedEffectSha256=rulesReplayEffectStateHash();
        rulesReplayEvents_.push_back(std::move(event));
        while(rulesReplayEvents_.size()>kRulesReplayEventJournalCapacity)
            rulesReplayEvents_.pop_front();
    }

    rules::BreakFoulDecisionResult result;
    result.ok = true;
    result.ack = ack;
    return result;
}

rules::EightOnBreakDecisionResult GameRuntime::submitEightOnBreakDecision(
    const rules::EightOnBreakDecisionRequest& request)
{
    auto rejected = [](const std::string& code) {
        rules::EightOnBreakDecisionResult result;
        result.errorCode = code;
        return result;
    };
    if (request.idempotencyKey.empty() ||
        request.idempotencyKey.size() > 128)
        return rejected("rules_decision_idempotency_key_invalid");

    const auto receipt = eightOnBreakReceipts_.find(
        request.idempotencyKey);
    if (receipt != eightOnBreakReceipts_.end()) {
        if (!sameEightOnBreakRequest(receipt->second.request, request))
            return rejected("rules_decision_idempotency_conflict");
        rules::EightOnBreakDecisionResult result;
        result.ok = true;
        result.ack = receipt->second.ack;
        return result;
    }
    if (request.rulesetId != rules::kEightOnBreakRulesetId ||
        request.capabilityId != rules::kEightOnBreakCapabilityId ||
        request.flowId != rules::kEightOnBreakFlowId)
        return rejected("rules_decision_context_mismatch");
    if (!eightOnBreakDecisionOffer_) {
        if (staleEightOnBreakDecisionIds_.count(request.decisionId) != 0)
            return rejected("rules_decision_stale");
        return rejected("rules_eight_on_break_not_pending");
    }
    const rules::EightOnBreakDecisionOffer offer =
        *eightOnBreakDecisionOffer_;
    if (request.decisionId != offer.decisionId)
        return rejected("rules_decision_stale");
    if (request.expectedAuthorityStateRevision !=
            offer.authorityStateRevision ||
        request.expectedAuthorityStateRevision != rulesAuthorityRevision_)
        return rejected("rules_decision_stale_revision");
    if (request.actorPlayerId != offer.actorPlayerId)
        return rejected("rules_decision_actor_mismatch");
    if (std::find(offer.offeredActionIds.begin(),
            offer.offeredActionIds.end(), request.actionId) ==
        offer.offeredActionIds.end())
        return rejected("rules_decision_action_not_offered");
    if (request.payloadCanonical != "{}")
        return rejected("rules_decision_invalid_payload");
    if (state_.ballsMoving || shotFactCollector_.active())
        return rejected("rules_decision_invalid_state");
    const std::string currentHash = rulesAuthorityStateHash();
    if (eightOnBreakOfferStateHash_.empty() ||
        currentHash != eightOnBreakOfferStateHash_)
        return rejected("rules_eight_on_break_state_changed");

    const bool respot =
        request.actionId == rules::kEightOnBreakSpotContinueActionId ||
        request.actionId == rules::kEightOnBreakSpotBallInHandActionId;
    rules::RackFoundationPlan foundation;
    int breakerPlayerId = -1;
    if (respot) {
        rules::RespotFoundationParameters parameters;
        parameters.ball = 8;
        parameters.nominalPosition = Point3{
            0.0f, kTableHeight + physicsProfile_.ball.radiusCm,
            static_cast<float>(
                physicsProfile_.tableBoundary.playfieldLengthCm * 0.25)};
        foundation = rules::buildRespotFoundationPlan(
            state_, physicsProfile_, parameters, rulesAuthorityRevision_,
            currentHash);
    } else {
        breakerPlayerId =
            request.actionId ==
                rules::kEightOnBreakRerackOriginalActionId ?
            offer.originalBreakerPlayerId :
            1 - offer.originalBreakerPlayerId;
        rules::RerackFoundationParameters parameters;
        parameters.breakerPlayerId = breakerPlayerId;
        foundation = rules::buildRerackFoundationPlan(
            state_, physicsProfile_, parameters, rulesAuthorityRevision_,
            currentHash);
    }
    if (!foundation.ok) return rejected(foundation.errorCode);

    const std::string replayParentEffect=!respot?std::string{}:
        rulesReplayEffectStateHash();
    const std::string replayRackIdentity=!respot?std::string{}:
        currentRulesReplayRackIdentity();
    if(respot&&replayRulesRackIdentity_.empty())
        replayRulesRackIdentity_=replayRackIdentity;

    const std::uint64_t previousRevision = rulesAuthorityRevision_;
    if (respot) {
        applyVerifiedRespotFoundation(foundation);
        matchState_.currentPlayer = offer.frozenNextPlayer;
        matchState_.ballInHand =
            offer.branch == rules::EightOnBreakBranch::FoulBreak;
        ballInHandPlacementScope_ = matchState_.ballInHand ?
            rules::BallInHandPlacementScope::BehindHeadString :
            rules::BallInHandPlacementScope::None;
        state_.players.currentPlayer = offer.frozenNextPlayer;
        state_.players.nextPlayer = offer.frozenNextPlayer;
        if (hasLastAdjudication_) {
            const bool foulSpot =
                offer.branch == rules::EightOnBreakBranch::FoulBreak;
            lastAdjudication_.eightBallMustBeSpotted = false;
            lastAdjudication_.rerackAllowed = false;
            lastAdjudication_.ballInHand = foulSpot;
            lastAdjudication_.ballInHandBehindHeadString = foulSpot;
            lastAdjudication_.decisionOptions.clear();
            lastAdjudication_.nextState = matchState_;
        }
        eightOnBreakDecisionOffer_.reset();
        eightOnBreakOfferStateHash_.clear();
        staleEightOnBreakDecisionIds_.insert(offer.decisionId);
    } else {
        applyVerifiedRerackFoundation(foundation);
    }
    ++rulesAuthorityRevision_;
    if (respot &&
        offer.branch == rules::EightOnBreakBranch::FoulBreak) {
        stageBallInHandPlacementAuthority(
            rules::BallInHandPlacementScope::BehindHeadString,
            offer.frozenNextPlayer);
        ballInHandPlacementOfferStateHash_ = rulesAuthorityStateHash();
    }

    rules::EightOnBreakDecisionAck ack;
    ack.accepted = true;
    ack.decisionId = request.decisionId;
    ack.actionId = request.actionId;
    ack.idempotencyKey = request.idempotencyKey;
    ack.actorPlayerId = request.actorPlayerId;
    ack.previousStateRevision = previousRevision;
    ack.authoritativeStateRevision = rulesAuthorityRevision_;
    ack.effects.branch = offer.branch;
    ack.effects.eightBallRespotted = respot;
    ack.effects.rackRebuilt = !respot;
    ack.effects.originalBreakerPlayerId =
        offer.originalBreakerPlayerId;
    ack.effects.breakerPlayerId = breakerPlayerId;
    ack.effects.currentPlayer = matchState_.currentPlayer;
    ack.effects.nextPlayer = state_.players.nextPlayer;
    ack.effects.turnContinues = respot &&
        offer.branch == rules::EightOnBreakBranch::LegalBreak;
    ack.effects.ballInHand = matchState_.ballInHand;
    ack.effects.ballInHandScope = ballInHandPlacementScope_;
    ack.effects.decisionPending = eightOnBreakDecisionPending();
    ack.effects.groups = matchState_.groups;
    ack.effects.phase = matchState_.phase;
    ack.effects.winner = matchState_.winner;
    ack.effects.eightBallPosition = state_.balls[8].position;
    ack.authoritativeStateHash = rulesAuthorityStateHash();

    events_.push_back(RuntimeEvent{nextSequence_++, tick_,
        "rules_eight_on_break_choice_committed"});
    rulesAuthorityActions_.push_back(rules::RulesAuthorityAction{
        nextRulesAuthorityActionSequence_++, tick_, request.decisionId,
        request.actionId, request.idempotencyKey, request.actorPlayerId,
        previousRevision, rulesAuthorityRevision_,
        ack.authoritativeStateHash, request.payloadCanonical});
    eightOnBreakReceipts_[request.idempotencyKey] =
        EightOnBreakReceipt{request, ack};
    if(respot&&!replayRulesImportActive_){
        RuntimeRulesReplayEvent event;
        event.sequence=nextRulesReplayEventSequence_++;
        event.tick=tick_;
        event.afterAppliedSemanticActionSequence=
            appliedSemanticActions_.empty()?0:
                appliedSemanticActions_.back().sequence;
        event.type=RuntimeRulesReplayEventType::EightOnBreakChoiceCommit;
        event.rackIdentity=replayRackIdentity;
        event.decisionId=request.decisionId;
        event.actionId=request.actionId;
        event.actorPlayerId=request.actorPlayerId;
        event.previousRulesRevision=previousRevision;
        event.committedRulesRevision=rulesAuthorityRevision_;
        event.parentEffectSha256=replayParentEffect;
        event.committedEffectSha256=rulesReplayEffectStateHash();
        rulesReplayEvents_.push_back(std::move(event));
        while(rulesReplayEvents_.size()>kRulesReplayEventJournalCapacity)
            rulesReplayEvents_.pop_front();
    }

    rules::EightOnBreakDecisionResult result;
    result.ok = true;
    result.ack = ack;
    return result;
}

rules::RackFoundationPlan GameRuntime::stageOfficialRerack(
    const rules::RerackFoundationParameters& parameters) const
{
    if (rulesMode_ != RulesMode::ChineseEightBallV1) {
        rules::RackFoundationPlan plan;
        plan.errorCode = "ruleset_not_enabled";
        return plan;
    }
    if (state_.ballsMoving || shotFactCollector_.active()) {
        rules::RackFoundationPlan plan;
        plan.errorCode = "rules_decision_invalid_state";
        return plan;
    }
    if (breakFoulDecisionPending() || eightOnBreakDecisionPending() ||
        ballInHandPlacementPending()) {
        rules::RackFoundationPlan plan;
        plan.errorCode = pendingRulesDecisionError();
        return plan;
    }
    return rules::buildRerackFoundationPlan(state_, physicsProfile_,
        parameters, rulesAuthorityRevision_, rulesAuthorityStateHash());
}

rules::RackFoundationPlan GameRuntime::stageBallRespot(
    const rules::RespotFoundationParameters& parameters) const
{
    if (rulesMode_ != RulesMode::ChineseEightBallV1) {
        rules::RackFoundationPlan plan;
        plan.operation = rules::RackFoundationOperation::Respot;
        plan.errorCode = "ruleset_not_enabled";
        return plan;
    }
    if (state_.ballsMoving || shotFactCollector_.active()) {
        rules::RackFoundationPlan plan;
        plan.operation = rules::RackFoundationOperation::Respot;
        plan.errorCode = "rules_decision_invalid_state";
        return plan;
    }
    if (breakFoulDecisionPending() || eightOnBreakDecisionPending() ||
        ballInHandPlacementPending()) {
        rules::RackFoundationPlan plan;
        plan.operation = rules::RackFoundationOperation::Respot;
        plan.errorCode = pendingRulesDecisionError();
        return plan;
    }
    return rules::buildRespotFoundationPlan(state_, physicsProfile_,
        parameters, rulesAuthorityRevision_, rulesAuthorityStateHash());
}

void GameRuntime::applyVerifiedRerackFoundation(
    const rules::RackFoundationPlan& plan)
{
    resetRulesReplayJournal();
    canonicalCueTipStrokeObserver_.rollback();
    hasCanonicalCueTipStrokeObservation_=false;
    canonicalCueTipStrokeObservationPending_=false;
    canonicalCueTipStrokeObservation_=CanonicalCueTipStrokeObservation{};
    d5BShotObserver_.rollback();
    hasCanonicalCueObjectOrderObservation_=false;
    hasCanonicalFrozenCushionShotObservation_=false;
    for (const auto& placement : plan.placements) {
        BallState& ball = state_.balls[placement.first];
        ball.position = placement.second;
        ball.pocketed = false;
        ball.outOfPlay = false;
        ball.physicalDomain = PhysicalBallDomain::ClothSupported;
        resetBallMotion(ball);
        resetPocketInteraction(ball);
    }
    state_.ballsMoving = false;
    state_.pocketedBallCount = 0;
    state_.nextPocketCaptureSequence = 1;
    clearGameplayEvents(state_);
    state_.players.currentPlayer = plan.rerack.breakerPlayerId;
    state_.players.nextPlayer = plan.rerack.breakerPlayerId;
    state_.players.assignedBallType[0] = -1;
    state_.players.assignedBallType[1] = -1;
    state_.players.firstPocketedObjectBall = true;
    state_.players.illegalShot = false;
    state_.players.updatedAfterShot = false;
    state_.players.shotTaken = false;
    state_.players.aimingAtCueBall = false;
    state_.input.hitRequested = false;
    state_.gameOver = false;
    matchState_ = rules::MatchState{};
    rulesRackTerminalState_ = RulesRackTerminalState{};
    mobileDeviceViolationHistory_.fill(
        rules::MobileDeviceViolationState::None);
    replayRulesRackIdentity_.clear();
    matchState_.currentPlayer = plan.rerack.breakerPlayerId;
    shotFactCollector_ = rules::ShotFactCollector{};
    pendingBreakHeadStringRelease_.reset();
    hasLastShotFacts_ = false;
    lastShotFacts_ = rules::ShotFacts{};
    hasLastAdjudication_ = false;
    lastAdjudication_ = rules::AdjudicationResult{};
    nonphysicalFoulExplanation_.reset();
    pendingCalledPocket_ = -1;
    invalidateGroupChoiceAuthority();
    invalidateBallInHandAuthority();
    invalidateBreakFoulAuthority();
    invalidateEightOnBreakAuthority();
    ballInHandPlacementScope_ =
        rules::BallInHandPlacementScope::None;
    if (state_.camera.anchorMode == CameraAnchorMode::FollowCueBall)
        updateCameraFromCueBall(state_);
    rackOriginalBreakerPlayerId_ = plan.rerack.breakerPlayerId;
    rackOriginalShootingOrder_ = {plan.rerack.breakerPlayerId,
        1 - plan.rerack.breakerPlayerId};
    if (stalemateAuthority_ && !stalemateRerackCommitInProgress_) {
        ++stalemateRackIdentityOrdinal_;
        stalemateAuthority_->reset("runtime.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(stalemateRackIdentityOrdinal_),
            rackOriginalBreakerPlayerId_, rackOriginalShootingOrder_);
    }
    if (groupConfusionAuthority_ &&
        !groupConfusionRerackCommitInProgress_) {
        ++groupConfusionRackIdentityOrdinal_;
        groupConfusionAuthority_->reset(
            "runtime.group_confusion.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(groupConfusionRackIdentityOrdinal_),
            rackOriginalBreakerPlayerId_, rackOriginalShootingOrder_);
        groupConfusionCompletedShotOrdinal_ = 0;
    }
    if (frozenBallDeclarationAuthority_) {
        ++frozenBallDeclarationRackIdentityOrdinal_;
        frozenBallDeclarationAuthority_->reset(
            "runtime.frozen_declaration.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(frozenBallDeclarationRackIdentityOrdinal_));
        nextFrozenBallDeclarationShotIdentity_ = 1;
    }
    if (frozenCushionDeclarationAuthority_) {
        ++frozenCushionDeclarationRackIdentityOrdinal_;
        frozenCushionDeclarationAuthority_->reset(
            "runtime.frozen_cushion.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(frozenCushionDeclarationRackIdentityOrdinal_));
        nextFrozenCushionDeclarationShotIdentity_ = 1;
    }
    if(doubleHitCloseCaseAuthority_){
        ++doubleHitCloseCaseRackIdentityOrdinal_;
        doubleHitCloseCaseAuthority_->reset("runtime.double_hit_close.rack."+
            std::to_string(eventJournalGeneration_)+"."+
            std::to_string(doubleHitCloseCaseRackIdentityOrdinal_));
        nextDoubleHitCloseCaseShotIdentity_=1;
    }
    if(pushReviewAuthority_){++pushReviewRackIdentityOrdinal_;pushReviewAuthority_->reset(
        "runtime.push_review.rack."+std::to_string(eventJournalGeneration_)+"."+
        std::to_string(pushReviewRackIdentityOrdinal_));nextPushReviewShotIdentity_=1;}
    if(nonphysicalGeneralFoulAuthority_){++nonphysicalGeneralFoulRackIdentityOrdinal_;
        nonphysicalGeneralFoulAuthority_->reset("runtime.nonphysical_foul.rack."+
            std::to_string(eventJournalGeneration_)+"."+
            std::to_string(nonphysicalGeneralFoulRackIdentityOrdinal_));}
    if (unintentionalBreakMiscueDeclarationAuthority_) {
        ++unintentionalBreakMiscueRackIdentityOrdinal_;
        unintentionalBreakMiscueDeclarationAuthority_->reset(
            "runtime.unintentional_break_miscue.rack." +
            std::to_string(eventJournalGeneration_) + "." +
            std::to_string(unintentionalBreakMiscueRackIdentityOrdinal_));
    }
}

void GameRuntime::applyVerifiedRespotFoundation(
    const rules::RackFoundationPlan& plan)
{
    const auto& placement = plan.placements.front();
    BallState& ball = state_.balls[placement.first];
    ball.position = placement.second;
    ball.pocketed = false;
    ball.outOfPlay = false;
    ball.physicalDomain = PhysicalBallDomain::ClothSupported;
    resetBallMotion(ball);
    resetPocketInteraction(ball);
    state_.pocketedBallCount = 0;
    for (const BallState& current : state_.balls)
        if (current.pocketed) ++state_.pocketedBallCount;
    if (placement.first == 8 && hasLastAdjudication_)
        lastAdjudication_.eightBallMustBeSpotted = false;
    if (state_.camera.anchorMode == CameraAnchorMode::FollowCueBall)
        updateCameraFromCueBall(state_);
}

rules::RackFoundationResult GameRuntime::commitRackFoundation(
    const rules::RackFoundationPlan& plan,
    const std::string& idempotencyKey)
{
    auto rejected = [](const std::string& code) {
        rules::RackFoundationResult result;
        result.errorCode = code;
        return result;
    };
    if (idempotencyKey.empty() || idempotencyKey.size() > 128)
        return rejected("rules_decision_idempotency_key_invalid");
    const auto receipt = rackFoundationReceipts_.find(idempotencyKey);
    if (receipt != rackFoundationReceipts_.end()) {
        if (!sameRackFoundationPlan(receipt->second.plan, plan))
            return rejected("rules_decision_idempotency_conflict");
        rules::RackFoundationResult result;
        result.ok = true;
        result.ack = receipt->second.ack;
        return result;
    }
    if (!plan.ok || plan.transactionId.empty())
        return rejected(plan.errorCode.empty() ?
            "rules_foundation_plan_invalid" : plan.errorCode);
    if (rulesMode_ != RulesMode::ChineseEightBallV1)
        return rejected("ruleset_not_enabled");
    if (plan.expectedAuthorityStateRevision != rulesAuthorityRevision_)
        return rejected("rules_decision_stale_revision");
    if (plan.baseAuthorityStateHash != rulesAuthorityStateHash())
        return rejected("rules_foundation_state_changed");
    if (state_.ballsMoving || shotFactCollector_.active())
        return rejected("rules_decision_invalid_state");

    rules::RackFoundationPlan verified;
    if (plan.operation == rules::RackFoundationOperation::Rerack)
        verified = rules::buildRerackFoundationPlan(state_, physicsProfile_,
            plan.rerack, rulesAuthorityRevision_,
            rulesAuthorityStateHash());
    else
        verified = rules::buildRespotFoundationPlan(state_, physicsProfile_,
            plan.respot, rulesAuthorityRevision_,
            rulesAuthorityStateHash());
    if (!verified.ok) return rejected(verified.errorCode);
    if (!sameRackFoundationPlan(verified, plan))
        return rejected("rules_foundation_plan_mismatch");

    if (plan.operation == rules::RackFoundationOperation::Rerack)
        applyVerifiedRerackFoundation(verified);
    else
        applyVerifiedRespotFoundation(verified);

    const std::uint64_t previousRevision = rulesAuthorityRevision_;
    ++rulesAuthorityRevision_;
    rules::RackFoundationAck ack;
    ack.accepted = true;
    ack.transactionId = verified.transactionId;
    ack.actionId = plan.operation == rules::RackFoundationOperation::Rerack ?
        rules::kFoundationRerackActionId : rules::kFoundationRespotActionId;
    ack.idempotencyKey = idempotencyKey;
    ack.previousStateRevision = previousRevision;
    ack.authoritativeStateRevision = rulesAuthorityRevision_;
    ack.effects.operation = plan.operation;
    ack.effects.placements = verified.placements;
    ack.effects.breakerPlayerId =
        plan.operation == rules::RackFoundationOperation::Rerack ?
            plan.rerack.breakerPlayerId : -1;
    ack.effects.currentPlayer = matchState_.currentPlayer;
    ack.effects.nextPlayer = state_.players.nextPlayer;
    ack.effects.ballInHand = matchState_.ballInHand;
    ack.effects.groups = matchState_.groups;
    ack.effects.phase = matchState_.phase;
    ack.effects.winner = matchState_.winner;
    ack.authoritativeStateHash = rulesAuthorityStateHash();
    const char* eventName =
        plan.operation == rules::RackFoundationOperation::Rerack ?
            "rules_foundation_rerack_committed" :
            "rules_foundation_respot_committed";
    events_.push_back(RuntimeEvent{nextSequence_++, tick_, eventName});
    rulesAuthorityActions_.push_back(rules::RulesAuthorityAction{
        nextRulesAuthorityActionSequence_++, tick_, verified.transactionId,
        ack.actionId, idempotencyKey, matchState_.currentPlayer,
        previousRevision, rulesAuthorityRevision_,
        ack.authoritativeStateHash, verified.transactionId});
    rackFoundationReceipts_[idempotencyKey] =
        RackFoundationReceipt{verified, ack};

    rules::RackFoundationResult result;
    result.ok = true;
    result.ack = ack;
    return result;
}

}  // namespace billiardgl
