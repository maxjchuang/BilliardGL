#include "replay/replay_recorder.h"
#include "replay/product_replay_content_registry.h"
#include "replay/replay_transport_model.h"
#include "rules/rack_executor.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

class ReplayRefereeAuthenticator final :
    public billiardgl::rules::RefereeAuthorityAuthenticator {
public:
    bool verify(const billiardgl::rules::RefereeAuthenticationInput& input)
        const override
    {
        return input.credentialId=="credential"&&input.keyVersion=="v1"&&
            input.authenticationProof==billiardgl::sha256Hex(
                "secret|"+input.authenticatedBytes);
    }
};

billiardgl::rules::RefereeAuthorityRequest refereeRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& decision,const std::string& key,
    const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId=kRefereeAuthorityProtocolId;request.schemaVersion=1;
    request.operation=operation;
    request.declarationKind=kNonphysicalGeneralFoulKind;
    request.decisionId=decision;
    request.rackIdentity=runtime.nonphysicalGeneralFoulState().rackIdentity;
    request.expectedAuthorityRevision=
        runtime.nonphysicalGeneralFoulProtocolState().authorityRevision;
    request.expectedAuthorityStateHash=
        runtime.nonphysicalGeneralFoulProtocolState().authorityStateHash;
    request.authorityRole=RefereeAuthorityRole::Referee;
    request.authoritySubjectId="referee";
    request.credentialId="credential";request.keyVersion="v1";
    request.authenticationNonce=key+".nonce";
    request.expiresAtAuthorityTick=10000;
    request.idempotencyKey=key;request.payloadCanonical=payload;
    request.authenticationProof=billiardgl::sha256Hex("secret|"+
        canonicalRefereeAuthenticatedBytes(request));
    return request;
}

billiardgl::rules::RefereeAuthorityRequest unintentionalBreakMiscueRequest(
    const billiardgl::GameRuntime& runtime,
    billiardgl::rules::RefereeAuthorityOperation operation,
    const std::string& key, const std::string& payload)
{
    using namespace billiardgl::rules;
    RefereeAuthorityRequest request;
    request.protocolId = kRefereeAuthorityProtocolId;
    request.schemaVersion = 1;
    request.operation = operation;
    request.declarationKind = kUnintentionalBreakMiscueDeclarationKind;
    request.decisionId = "replay.unintentional_break_miscue.1";
    request.rackIdentity =
        runtime.unintentionalBreakMiscueDeclarationState().rackIdentity;
    request.expectedAuthorityRevision =
        runtime.unintentionalBreakMiscueDeclarationProtocolState().
            authorityRevision;
    request.expectedAuthorityStateHash =
        runtime.unintentionalBreakMiscueDeclarationProtocolState().
            authorityStateHash;
    request.authorityRole = RefereeAuthorityRole::Referee;
    request.authoritySubjectId = "referee.06g";
    request.credentialId = "credential";
    request.keyVersion = "v1";
    request.authenticationNonce = key + ".nonce";
    request.expiresAtAuthorityTick = 10000;
    request.idempotencyKey = key;
    request.payloadCanonical = payload;
    request.authenticationProof = billiardgl::sha256Hex("secret|" +
        canonicalRefereeAuthenticatedBytes(request));
    return request;
}

}  // namespace

int main()
{
    try {
        billiardgl::GameRuntime runtime;
        const std::string generationZeroCanonical =
            billiardgl::canonicalGameState(runtime.state());
        const std::string generationZeroHash =
            billiardgl::gameStateSha256(runtime.state());
        expect(generationZeroCanonical.find("trajectory_generation") ==
                std::string::npos &&
            generationZeroHash ==
                "2845f5fce2453d9ac477bed4d0b76ab99570ef047d4861c22f2684c01699ad62",
            "generation-zero canonical state preserves the legacy hash payload");
        runtime.mutableState().balls[0].trajectoryGeneration =
            std::numeric_limits<std::uint64_t>::max();
        expect(billiardgl::canonicalGameState(runtime.state()).find(
                "\"trajectory_generation_schema\":\"uint64_decimal_v1\"") !=
                std::string::npos &&
            billiardgl::gameStateSha256(runtime.state()) != generationZeroHash,
            "nonzero trajectory generation is explicitly versioned into the state hash");
        const std::string executable = "billiardgl-recorder-test-v1";
        const std::string content = billiardgl::sha256Hex("training-pack-v1");
        {
            const billiardgl::GameState base = runtime.state();
            const std::string baseAuthority =
                billiardgl::replay::replayAuthorityStateSha256(base);
            const auto differs = [&](const billiardgl::GameState& state) {
                return billiardgl::replay::replayAuthorityStateSha256(state) !=
                    baseAuthority;
            };
            auto mutation = base;
            mutation.balls[0].physicalDomain =
                billiardgl::PhysicalBallDomain::Airborne;
            expect(differs(mutation), "authority digest covers physical domain");
            mutation = base;
            mutation.balls[0].pocketInteraction.phase =
                billiardgl::PocketInteractionPhase::Approaching;
            expect(differs(mutation), "authority digest covers pocket phase");
            mutation = base;
            mutation.balls[0].pocketInteraction.pocketId = 4;
            expect(differs(mutation), "authority digest covers pocket id");
            mutation = base;
            mutation.balls[0].pocketInteraction.captureSequence = 7;
            expect(differs(mutation), "authority digest covers capture sequence");
            mutation = base;
            mutation.balls[0].trajectoryGeneration = 8;
            expect(differs(mutation), "authority digest covers trajectory generation");
            mutation = base;
            mutation.balls[1].outOfPlay = true;
            expect(differs(mutation), "authority digest covers out-of-play state");
            mutation = base;
            mutation.nextPocketCaptureSequence = 9;
            expect(differs(mutation),
                "authority digest covers next pocket capture sequence");
        }
        {
            billiardgl::GameRuntime unsupported;
            unsupported.mutableState().balls[0].physicalDomain =
                billiardgl::PhysicalBallDomain::Airborne;
            billiardgl::replay::Recorder rejected;
            expect(rejected.begin(unsupported, executable, content, 70).errorCode ==
                    "replay_recording_initial_state_unsupported" &&
                !rejected.active(),
                "Replay v1 refuses an initial physical domain it cannot serialize");
            unsupported.mutableState().balls[0].physicalDomain =
                billiardgl::PhysicalBallDomain::ClothSupported;
            unsupported.mutableState().balls[0].pocketInteraction.phase =
                billiardgl::PocketInteractionPhase::ThroatCrossed;
            expect(rejected.begin(unsupported, executable, content, 70).errorCode ==
                    "replay_recording_initial_state_unsupported",
                "Replay v1 refuses an initial pocket lifecycle it cannot serialize");
            unsupported.mutableState().balls[0].pocketInteraction = {};
            unsupported.mutableState().nextPocketCaptureSequence = 2;
            expect(rejected.begin(unsupported, executable, content, 70).errorCode ==
                    "replay_recording_initial_state_unsupported",
                "Replay v1 refuses an unrepresentable initial capture sequence");
        }
        {
            using namespace billiardgl;
            using namespace billiardgl::rules;
            const std::string proposalPayload =
                "{\"actor_player_id\":0,\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
            const std::string finalPayload =
                "{\"actor_player_id\":0,\"proposal_decision_id\":\"replay.unintentional_break_miscue.1\",\"relation\":\"UNINTENTIONAL_BREAK_MISCUE_IF_NATIVE_MISCUE_NO_RACK_CONTACT\"}";
            ReplayRefereeAuthenticator authenticator;
            const auto prepare = [&](GameRuntime& candidate) {
                expect(candidate.setRulesMode(
                           RulesMode::ChineseEightBallV1).ok,
                    "06G recorder fixture enables Chinese rules");
                candidate.configureUnintentionalBreakMiscueDeclarationAuthority(
                    &authenticator);
            };
            const auto propose = [&](GameRuntime& candidate,
                                     const std::string& key) {
                auto request = unintentionalBreakMiscueRequest(candidate,
                    RefereeAuthorityOperation::Propose, key,
                    proposalPayload);
                expect(candidate.
                        submitUnintentionalBreakMiscueDeclarationProposal(
                            request).ok,
                    "06G recorder fixture opens a signed live declaration");
            };
            const auto finalize = [&](GameRuntime& candidate,
                                      const std::string& key) {
                auto request = unintentionalBreakMiscueRequest(candidate,
                    RefereeAuthorityOperation::FinalDeclare, key,
                    finalPayload);
                expect(candidate.
                        submitUnintentionalBreakMiscueDeclarationFinal(
                            request).ok,
                    "06G recorder fixture commits the signed declaration");
            };

            GameRuntime proposalOnly;
            prepare(proposalOnly);
            replay::Recorder proposalRecorder;
            expect(proposalRecorder.begin(
                       proposalOnly, executable, content, 70).ok,
                "recorder begins while the 06G authority is inactive");
            propose(proposalOnly, "replay.06g.proposal.pending");
            const auto pending = proposalRecorder.captureApplied(proposalOnly);
            expect(!pending.ok && pending.errorCode ==
                    "replay_rules_declaration_proposal_pending" &&
                !proposalRecorder.active(),
                "06G ProposalOpen remains explicitly fail-closed");
            replay::Recorder rejectedAtProposal;
            expect(rejectedAtProposal.begin(proposalOnly, executable,
                       content, 70).errorCode ==
                    "replay_rules_declaration_proposal_pending",
                "recording cannot begin inside a 06G proposal");

            GameRuntime cancelled;
            prepare(cancelled);
            replay::Recorder cancelledRecorder;
            expect(cancelledRecorder.begin(
                       cancelled, executable, content, 70).ok,
                "cancelled 06G fixture starts before its proposal");
            propose(cancelled, "replay.06g.cancel.proposal");
            auto cancel = unintentionalBreakMiscueRequest(cancelled,
                RefereeAuthorityOperation::CancelProposal,
                "replay.06g.cancel.final",
                "{\"proposal_decision_id\":\"replay.unintentional_break_miscue.1\"}");
            expect(cancelled.submitUnintentionalBreakMiscueDeclarationCancel(
                       cancel).ok &&
                cancelledRecorder.captureApplied(cancelled).errorCode ==
                    "replay_rules_declaration_cancel_unsupported",
                "06G Cancelled remains explicitly fail-closed");

            GameRuntime invalidated;
            prepare(invalidated);
            replay::Recorder invalidatedRecorder;
            expect(invalidatedRecorder.begin(
                       invalidated, executable, content, 70).ok,
                "invalidated 06G fixture starts before declaration");
            propose(invalidated, "replay.06g.invalidate.proposal");
            finalize(invalidated, "replay.06g.invalidate.final");
            CueImpactInput center;
            center.cueBallIndex = 0;
            center.cueSpeedCmS = 100.0;
            center.cueMassKg = invalidated.physicsProfile().cue.effectiveMassKg;
            center.direction = {{0.0, 0.0, 1.0}};
            center.chalkState = "CHALKED";
            expect(invalidated.applyCueImpact(center).ok &&
                invalidated.unintentionalBreakMiscueDeclarationState().phase ==
                    UnintentionalBreakMiscueDeclarationPhase::Invalidated &&
                invalidatedRecorder.captureApplied(invalidated).errorCode ==
                    "replay_rules_declaration_transition_unsupported",
                "06G Invalidated remains explicitly fail-closed");

            GameRuntime alreadyActive;
            prepare(alreadyActive);
            propose(alreadyActive, "replay.06g.active.proposal");
            finalize(alreadyActive, "replay.06g.active.final");
            replay::Recorder rejectedAtActive;
            expect(rejectedAtActive.begin(alreadyActive, executable,
                       content, 70).errorCode ==
                    "replay_recording_rules_state_unsupported",
                "recording cannot start after an unrecorded 06G commit");

            GameRuntime live;
            prepare(live);
            replay::Recorder miscueRecorder;
            expect(miscueRecorder.begin(live, executable, content, 70).ok,
                "06G recorder starts before the signed declaration");
            propose(live, "replay.06g.record.proposal");
            finalize(live, "replay.06g.record.final");
            const float exactTip = std::nextafter(
                live.physicsProfile().cue.maximumReliableOffsetRadius,
                std::numeric_limits<float>::infinity());
            expect(live.setTipOffset(exactTip, 0.0f).ok,
                "06G replay fixture uses the ordinary semantic tip action");
            GameAction shoot;
            shoot.type = ActionType::Shoot;
            expect(live.dispatch(shoot).ok &&
                live.hasCueContactResult() &&
                !live.cueContactResult().applied &&
                live.cueContactResult().regime == CueContactRegime::Miscue &&
                live.unintentionalBreakMiscueDeclarationState().phase ==
                    UnintentionalBreakMiscueDeclarationPhase::Consumed &&
                live.unintentionalBreakMiscueDeclarationState().
                    consumedStrokeBindingSha256.size() == 64,
                "existing semantic input produces a native modeled miscue without injected facts");
            expect(live.step(1).ok && live.hasLastShotFacts() &&
                live.lastShotFacts().breakEvidenceOrigin ==
                    BreakEvidenceOrigin::AuthoritativeShotFactCollector &&
                live.lastShotFacts().breakEvidenceComplete &&
                live.lastShotFacts().firstContactBall < 1 &&
                live.lastShotFacts().firstContactBalls.empty() &&
                live.breakFoulDecisionOffer() &&
                live.breakFoulDecisionOffer()->branch ==
                    BreakFoulBranch::NoRackContactMiscue &&
                miscueRecorder.finish(live, 1).ok,
                "native collector completes 06G before the recorder seals one shot");
            const replay::Document recorded = miscueRecorder.document();
            expect(recorded.formatVersion == replay::kReplayRulesFormatVersion &&
                recorded.rulesReceiptSchema ==
                    replay::kReplaySingleRackMiscueDeclarationReceiptSchema &&
                recorded.rulesReceipts.size() == 1 &&
                recorded.rulesReceipts[0].event.type ==
                    RuntimeRulesReplayEventType::
                        UnintentionalBreakMiscueDeclarationCommit &&
                recorded.rulesReceipts[0].event.actorPlayerId == 0 &&
                recorded.physicsProfileSha256 ==
                    replay::physicsProfileSha256(live.physicsProfile()) &&
                recorded.actions.size() == 2 &&
                recorded.actions[0].type ==
                    replay::SemanticActionType::SetTipOffset &&
                recorded.actions[0].first == exactTip &&
                recorded.actions[1].type ==
                    replay::SemanticActionType::Shoot,
                "V4 records the signed commit and exact semantic tip before Shoot");

            const std::filesystem::path miscuePath =
                std::filesystem::temp_directory_path() /
                "billiardgl_recorded_replay_v4_06g.json";
            std::error_code miscueIgnored;
            std::filesystem::remove(miscuePath, miscueIgnored);
            std::filesystem::remove(
                miscuePath.string() + ".tmp", miscueIgnored);
            expect(miscueRecorder.saveAtomic(miscuePath.string()).ok,
                "V4 06G document saves atomically");
            std::ifstream miscueInput(miscuePath, std::ios::binary);
            const std::string miscueBytes(
                (std::istreambuf_iterator<char>(miscueInput)),
                std::istreambuf_iterator<char>());
            expect(miscueBytes.find("credential") == std::string::npos &&
                miscueBytes.find("authentication") == std::string::npos &&
                miscueBytes.find("nonce") == std::string::npos &&
                miscueBytes.find("proof") == std::string::npos &&
                miscueBytes.find("secret") == std::string::npos,
                "V4 06G receipt is secret-free");
            const auto miscueJson = json::parse(miscueBytes);
            replay::Document disk;
            expect(miscueJson.ok && replay::parse(miscueJson.value, disk).ok,
                "V4 06G receipt round-trips through disk JSON");
            replay::Player seekPlayer;
            expect(seekPlayer.load(disk, live.physicsProfile(), executable,
                       content).ok &&
                seekPlayer.seekShot(0).ok &&
                seekPlayer.runtime().
                    unintentionalBreakMiscueDeclarationState().phase ==
                    UnintentionalBreakMiscueDeclarationPhase::Active &&
                seekPlayer.runtime().shotControl().tipOffsetX == exactTip,
                "seek imports only verified committed authority and replays the exact pre-shot tip");
            const std::string importedEffect = seekPlayer.runtime().
                rulesReplayEffectStateHash();
            auto replaySelfSign = unintentionalBreakMiscueRequest(
                seekPlayer.runtime(), RefereeAuthorityOperation::FinalDeclare,
                "replay.06g.import.self_sign", finalPayload);
            expect(!const_cast<GameRuntime&>(seekPlayer.runtime()).
                    submitUnintentionalBreakMiscueDeclarationFinal(
                        replaySelfSign).ok &&
                seekPlayer.runtime().rulesReplayEffectStateHash() ==
                    importedEffect,
                "receipt import restores no live authenticator or player self-sign path");
            replay::Player player;
            expect(player.load(disk, live.physicsProfile(), executable,
                       content).ok,
                "V4 06G disk replay loads with the exact profile");
            while (!player.finished())
                expect(player.stepTick().ok,
                    "V4 06G disk replay advances through native collector");
            expect(player.finalHashMatches() &&
                player.runtime().
                    unintentionalBreakMiscueDeclarationState().phase ==
                    UnintentionalBreakMiscueDeclarationPhase::Consumed &&
                player.runtime().hasLastShotFacts() &&
                player.runtime().lastShotFacts().breakEvidenceOrigin ==
                    BreakEvidenceOrigin::AuthoritativeShotFactCollector &&
                player.runtime().breakFoulDecisionOffer() &&
                player.runtime().breakFoulDecisionOffer()->branch ==
                    BreakFoulBranch::NoRackContactMiscue,
                "V4 seek-to-final reproduces collector effect and all final hashes");

            PhysicsProfile changedProfile = live.physicsProfile();
            changedProfile.cue.maximumReliableOffsetRadius -= 0.01f;
            replay::Player profileMutation;
            expect(profileMutation.load(disk, changedProfile, executable,
                       content).errorCode == "replay_profile_hash_mismatch",
                "V4 rejects a changed physics profile before playback");

            auto expectRuntimeRejection = [&](replay::Document mutation,
                                               const char* expectedError,
                                               const char* message) {
                replay::Player rejected;
                const auto loaded = rejected.load(mutation,
                    live.physicsProfile(), executable, content);
                expect(loaded.ok, "self-consistent V4 mutation reaches runtime verification");
                replay::ValidationResult result{true, ""};
                while (!rejected.finished() && result.ok)
                    result = rejected.stepTick();
                if (result.ok || result.errorCode != expectedError)
                    std::cerr << "06G replay mutation expected="
                              << expectedError << " actual="
                              << result.errorCode << '\n';
                expect(!result.ok && result.errorCode == expectedError,
                    message);
            };
            auto mutation = disk;
            mutation.rulesReceipts[0].event.actorPlayerId = 1;
            mutation.rulesReceipts[0].receiptSha256 =
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expectRuntimeRejection(mutation,
                "replay_rules_commit_mismatch_06g",
                "V4 rejects a re-digested actor mutation");
            mutation = disk;
            mutation.rulesReceipts[0].event.declarationRackIdentity += ".x";
            mutation.rulesReceipts[0].receiptSha256 =
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expectRuntimeRejection(mutation,
                "replay_rules_commit_mismatch_06g",
                "V4 rejects a re-digested rack mutation");
            mutation = disk;
            mutation.rulesReceipts[0].event.parentEffectSha256 =
                std::string(64, '0');
            mutation.rulesReceipts[0].receiptSha256 =
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expectRuntimeRejection(mutation,
                "replay_rules_parent_mismatch",
                "V4 rejects a re-digested parent-effect mutation");
            mutation = disk;
            mutation.rulesReceipts[0].event.authoritySubjectId =
                "referee.tampered";
            mutation.rulesReceipts[0].receiptSha256 =
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expectRuntimeRejection(mutation,
                "replay_rules_commit_mismatch_06g",
                "V4 rejects a re-digested receipt payload mutation");
            mutation = disk;
            mutation.rulesReceipts[0].event.proposalDecisionId +=
                ".tampered";
            mutation.rulesReceipts[0].receiptSha256 =
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expectRuntimeRejection(mutation,
                "replay_rules_commit_mismatch_06g",
                "V4 rejects a re-digested proposal identity mutation");
            mutation = disk;
            ++mutation.rulesReceipts[0].event.previousRulesRevision;
            ++mutation.rulesReceipts[0].event.committedRulesRevision;
            mutation.rulesReceipts[0].receiptSha256 =
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expectRuntimeRejection(mutation,
                "replay_rules_parent_mismatch",
                "V4 rejects a re-digested parent revision mutation");
            mutation = disk;
            mutation.rulesReceipts[0].afterSemanticActionOrdinal =
                mutation.actions.size();
            mutation.rulesReceipts[0].receiptSha256 =
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expectRuntimeRejection(mutation,
                "replay_rules_parent_mismatch",
                "V4 rejects a declaration reordered after Shoot");
            mutation = disk;
            mutation.actions[0].first = std::nextafter(
                exactTip, std::numeric_limits<float>::infinity());
            expectRuntimeRejection(mutation,
                "replay_final_hash_mismatch",
                "V4 final effect hash binds the exact modeled tip input");

            // Component-only regression: two semantic miscue attempts can be
            // queued before the collector tick even though the product UI
            // does not expose this as normal gameplay.  The second attempt
            // must not replace the first shot's consumed declaration binding.
            GameRuntime twoAttempt;
            prepare(twoAttempt);
            replay::Recorder twoAttemptRecorder;
            expect(twoAttemptRecorder.begin(twoAttempt, executable,
                       content, 70).ok,
                "two-attempt V4 component replay starts cleanly");
            propose(twoAttempt, "replay.06g.two.proposal");
            finalize(twoAttempt, "replay.06g.two.final");
            expect(twoAttempt.setTipOffset(exactTip, 0.0f).ok &&
                twoAttempt.dispatch(shoot).ok,
                "first component miscue consumes the declaration");
            const std::string firstStrokeBinding = twoAttempt.
                unintentionalBreakMiscueDeclarationState().
                    consumedStrokeBindingSha256;
            const float secondTip = std::nextafter(
                exactTip, std::numeric_limits<float>::infinity());
            expect(twoAttempt.setTipOffset(secondTip, 0.0f).ok &&
                twoAttempt.dispatch(shoot).ok &&
                twoAttempt.cueImpactInput().tipOffsetRadius[0] !=
                    live.cueImpactInput().tipOffsetRadius[0] &&
                twoAttempt.unintentionalBreakMiscueDeclarationState().
                    consumedStrokeBindingSha256 == firstStrokeBinding &&
                twoAttempt.step(1).ok &&
                twoAttemptRecorder.finish(twoAttempt, 2).ok,
                "second component attempt cannot overwrite the consumed stroke binding");
            replay::Player twoAttemptPlayer;
            expect(twoAttemptPlayer.load(twoAttemptRecorder.document(),
                       twoAttempt.physicsProfile(), executable, content).ok,
                "two-attempt V4 component replay loads");
            while (!twoAttemptPlayer.finished())
                expect(twoAttemptPlayer.stepTick().ok,
                    "two-attempt V4 component replay advances");
            expect(twoAttemptPlayer.finalHashMatches() &&
                twoAttemptPlayer.runtime().
                    unintentionalBreakMiscueDeclarationState().
                        consumedStrokeBindingSha256 == firstStrokeBinding,
                "two-attempt live/replay preserves the first consumed binding");
            std::filesystem::remove(miscuePath, miscueIgnored);
        }
        billiardgl::replay::Recorder recorder;
        expect(recorder.begin(runtime, executable, content, 71).ok,
            "recorder binds stopped initial state/profile/content/seed");

        billiardgl::GameAction yaw;
        yaw.type = billiardgl::ActionType::SetAimYaw;
        yaw.first = 1.5707963f;
        expect(runtime.dispatch(yaw).ok && recorder.recordApplied(runtime,
            billiardgl::replay::SemanticActionType::SetAimYaw, yaw.first).ok,
            "applied aim action is recorded at authoritative tick");
        billiardgl::GameAction power;
        power.type = billiardgl::ActionType::SetShotPower;
        power.first = 72.0f;
        expect(runtime.dispatch(power).ok && recorder.recordApplied(runtime,
            billiardgl::replay::SemanticActionType::SetShotPower, power.first).ok,
            "applied power action is recorded in order");
        billiardgl::GameAction rejectedPower = power;
        rejectedPower.first = 201.0f;
        const std::size_t beforeRejected = recorder.document().actions.size();
        expect(!runtime.dispatch(rejectedPower).ok &&
            !recorder.recordApplied(runtime,
                billiardgl::replay::SemanticActionType::SetShotPower,
                rejectedPower.first).ok &&
            recorder.document().actions.size() == beforeRejected,
            "failed semantic action cannot be inserted through recorder API");
        billiardgl::GameAction shoot;
        shoot.type = billiardgl::ActionType::Shoot;
        expect(runtime.dispatch(shoot).ok && recorder.recordApplied(runtime,
            billiardgl::replay::SemanticActionType::Shoot).ok,
            "applied shoot action is recorded semantically");
        expect(!recorder.finish(runtime, 1).ok,
            "final tick must follow the last recorded action");
        expect(runtime.step(80).ok && recorder.finish(runtime, 1).ok,
            "finished recording stores final tick/shot count/state hash");

        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "billiardgl_recorded_replay_v1.json";
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(path.string() + ".tmp", ignored);
        expect(recorder.saveAtomic(path.string()).ok &&
            std::filesystem::exists(path) && !std::filesystem::exists(path.string() + ".tmp"),
            "Replay v1 saves through same-directory atomic replacement");

        std::ifstream input(path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        const auto decoded = billiardgl::json::parse(bytes);
        billiardgl::replay::Document document;
        expect(decoded.ok && billiardgl::replay::parse(decoded.value, document).ok,
            "recorded replay parses as Replay v1");
        {
            using namespace billiardgl;
            replay::ProductReplayContentRegistry registry;
            const auto official=replay::officialSingleRackReplayBinding(
                runtime.physicsProfile());
            replay::ProductReplayContentRegistry incompleteRegistry;
            auto emptyOfficial=official;
            emptyOfficial.initialStateSha256.clear();
            emptyOfficial.initialAuthorityStateSha256.clear();
            emptyOfficial.initialRulesRevision=0;
            emptyOfficial.initialRulesEffectSha256.clear();
            expect(!incompleteRegistry.addTrustedContent(
                emptyOfficial).ok,
                "single-rack binding cannot omit its initial authority");
            auto partialOfficial=official;
            partialOfficial.initialRulesEffectSha256.clear();
            expect(!incompleteRegistry.addTrustedContent(
                partialOfficial).ok,
                "single-rack binding cannot partially bind its initial authority");
            expect(registry.addTrainingAttempt(content,runtime.physicsProfile()).ok&&
                registry.addOfficialSingleRack(runtime.physicsProfile()).ok&&
                !registry.addOfficialSingleRack(runtime.physicsProfile()).ok&&
                registry.size()==2,
                "product replay registry rejects duplicate trusted content");
            const replay::TrustedReplayContentBinding* binding=nullptr;
            expect(registry.authorize(document,binding).ok&&binding!=nullptr&&
                binding->scope==
                    replay::ProductReplayContentScope::TrainingAttempt,
                "registry resolves a validated training v1 binding");
            replay::Document mutation=document;
            mutation.formatVersion=replay::kReplayRulesFormatVersion;
            expect(!registry.authorize(mutation,binding).ok,
                "training content cannot authorize a rules-changing v2 document");
            mutation=document;
            mutation.contentSha256=official.contentSha256;
            mutation.physicsProfileId=official.physicsProfileId;
            mutation.physicsProfileSha256=official.physicsProfileSha256;
            mutation.rulesMode=RulesMode::ChineseEightBallV1;
            GameRuntime officialRuntime;
            expect(officialRuntime.replaceStateForScenario(
                officialRuntime.state(),runtime.physicsProfile(),nullptr,false,
                PhysicsBoundaryMode::ProductionTable).ok&&
                officialRuntime.setRulesMode(
                    RulesMode::ChineseEightBallV1).ok,
                "official registry fixture profile install failed");
            rules::RerackFoundationParameters rackParameters;
            rackParameters.deterministicSeed=
                rules::kOfficialRackDefaultSeed;
            rackParameters.breakerPlayerId=0;
            const auto rack=officialRuntime.stageOfficialRerack(
                rackParameters);
            expect(rack.ok&&officialRuntime.commitRackFoundation(
                rack,"product.match.start.official-rack-v1").ok,
                "official registry fixture rack install failed");
            mutation.initialState=officialRuntime.state();
            mutation.initialRulesRevision=
                officialRuntime.rulesAuthorityRevision();
            mutation.initialRulesEffectSha256=
                officialRuntime.rulesReplayEffectStateHash();
            expect(registry.authorize(mutation,binding).ok&&binding!=nullptr&&
                binding->scope==
                    replay::ProductReplayContentScope::SingleRackMatch,
                "official rack content resolves only through its product-owned binding");
            replay::Document forgedOfficial=mutation;
            forgedOfficial.initialState.balls[0].position.x+=1.0;
            expect(!registry.authorize(forgedOfficial,binding).ok,
                "official content identity binds its canonical rack state");
            mutation.physicsProfileSha256=std::string(64,'0');
            expect(!registry.authorize(mutation,binding).ok&&binding==nullptr,
                "file-declared profile cannot override the trusted binding");
            mutation=document;
            mutation.contentSha256=sha256Hex("unknown-product-replay-content");
            expect(!registry.authorize(mutation,binding).ok&&binding==nullptr,
                "unknown product content remains fail-closed");
        }
        expect(document.finalSummary.authorityStateSchema ==
                billiardgl::replay::kReplayAuthorityStateSchema &&
            document.finalSummary.authorityStateSha256 ==
                billiardgl::replay::replayAuthorityStateSha256(runtime.state()),
            "new recorder writes the versioned D6 authority digest");
        {
            billiardgl::json::Value halfBinding = decoded.value;
            halfBinding["final_summary"].asObject().erase(
                "authority_state_sha256");
            billiardgl::replay::Document unchanged = document;
            const auto rejected = billiardgl::replay::parse(
                halfBinding, unchanged);
            expect(!rejected.ok &&
                rejected.errorCode == "replay_authority_hash_invalid" &&
                unchanged.finalSummary.authorityStateSha256 ==
                    document.finalSummary.authorityStateSha256,
                "wire parser rejects a half-present authority binding atomically");
            halfBinding = decoded.value;
            halfBinding["final_summary"]["authority_state_schema"] =
                billiardgl::json::Value("");
            halfBinding["final_summary"]["authority_state_sha256"] =
                billiardgl::json::Value("");
            expect(billiardgl::replay::parse(
                    halfBinding, unchanged).errorCode ==
                    "replay_authority_hash_invalid",
                "present-but-empty authority fields cannot masquerade as legacy absence");
        }
        expect(document.initialState.balls[0].trajectoryGeneration ==
                std::numeric_limits<std::uint64_t>::max() &&
            decoded.value.at("initial_scenario").asArray()[0]
                .at("trajectory_generation").asString() ==
                "18446744073709551615",
            "Replay v1 round-trips full-width trajectory generation exactly");

        billiardgl::json::Value legacyReplay = decoded.value;
        legacyReplay["final_summary"].asObject().erase("authority_state_schema");
        legacyReplay["final_summary"].asObject().erase("authority_state_sha256");
        for (billiardgl::json::Value& ball :
                legacyReplay["initial_scenario"].asArray())
            ball.asObject().erase("trajectory_generation");
        billiardgl::replay::Document legacyDocument;
        expect(billiardgl::replay::parse(legacyReplay, legacyDocument).ok &&
            legacyDocument.initialState.balls[0].trajectoryGeneration == 0 &&
            legacyDocument.finalSummary.authorityStateSchema.empty() &&
            legacyDocument.finalSummary.authorityStateSha256.empty(),
            "legacy Replay v1 documents retain generation-zero and base-hash compatibility");

        const std::array<billiardgl::json::Value, 6> invalidGenerations = {{
            billiardgl::json::Value(-1),
            billiardgl::json::Value(1.0),
            billiardgl::json::Value("01"),
            billiardgl::json::Value("-1"),
            billiardgl::json::Value("1.0"),
            billiardgl::json::Value("18446744073709551616")}};
        for (const billiardgl::json::Value& invalid : invalidGenerations) {
            billiardgl::json::Value mutation = decoded.value;
            mutation["initial_scenario"].asArray()[0]
                ["trajectory_generation"] = invalid;
            billiardgl::replay::Document rejected = document;
            const auto result = billiardgl::replay::parse(mutation, rejected);
            expect(!result.ok &&
                result.errorCode == "replay_trajectory_generation_invalid" &&
                rejected.initialState.balls[0].trajectoryGeneration ==
                    document.initialState.balls[0].trajectoryGeneration,
                "Replay v1 rejects malformed or overflowing trajectory generation atomically");
        }
        billiardgl::replay::Player player;
        {
            auto removed = decoded.value;
            removed["initial_scenario"].asArray()[1]["out_of_play"] =
                billiardgl::json::Value(true);
            removed["initial_scenario"].asArray()[1]["pocketed"] =
                billiardgl::json::Value(false);
            billiardgl::replay::Document parsed;
            expect(billiardgl::replay::parse(removed, parsed).ok &&
                parsed.initialState.balls[1].outOfPlay &&
                !parsed.initialState.balls[1].pocketed,
                "replay preserves off-table removal independently of pocket capture");
            expect(billiardgl::replay::serialize(parsed).at("initial_scenario")
                .asArray()[1].at("out_of_play").asBool(),
                "replay writer retains the explicit removal state");
            removed["initial_scenario"].asArray()[1]["pocketed"] =
                billiardgl::json::Value(true);
            expect(!billiardgl::replay::parse(removed, parsed).ok,
                "replay rejects conflicting removal states");
            removed["initial_scenario"].asArray()[1]["out_of_play"] =
                billiardgl::json::Value("true");
            expect(!billiardgl::replay::parse(removed, parsed).ok,
                "replay rejects non-boolean removal state");
        }
        expect(player.load(document, runtime.physicsProfile(), executable, content).ok,
            "recorded replay loads with exact bindings");
        billiardgl::replay::ReplayTransportModel v1Transport;
        expect(v1Transport.load(document,runtime.physicsProfile(),executable,
                content).ok&&
            v1Transport.snapshot().documentFormatVersion==
                billiardgl::replay::kReplayFormatVersion&&
            v1Transport.snapshot().rulesReceiptCount==0&&
            !v1Transport.snapshot().rulesEffectVerified&&
            !v1Transport.snapshot().rackTerminalActive,
            "transport preserves v1 with no fabricated rules verification or rack terminal");
        while (!player.finished()) expect(player.stepTick().ok,
            "recorded semantic action log re-executes");
        expect(player.finalHashMatches(),
            "recorded replay reproduces the captured final state hash");
        std::filesystem::remove(path, ignored);

        {
            billiardgl::GameRuntime bounded;
            billiardgl::GameAction aim;
            aim.type = billiardgl::ActionType::SetAimYaw;
            for (std::size_t index = 0;
                    index < billiardgl::GameRuntime::
                        kAppliedSemanticActionJournalCapacity + 73;
                    ++index) {
                aim.first = (index % 2 == 0) ? 0.1f : 0.2f;
                expect(bounded.dispatch(aim).ok,
                    "high-frequency aim input remains valid");
            }
            expect(bounded.appliedSemanticActions().size() ==
                billiardgl::GameRuntime::kAppliedSemanticActionJournalCapacity,
                "semantic action journal has an explicit hard capacity");
        }

        {
            billiardgl::GameRuntime captured;
            billiardgl::replay::Recorder highFrequency;
            expect(highFrequency.begin(captured, executable, content, 72).ok,
                "high-frequency recorder starts");
            billiardgl::GameAction aim;
            aim.type = billiardgl::ActionType::SetAimYaw;
            const std::size_t total = billiardgl::GameRuntime::
                kAppliedSemanticActionJournalCapacity * 2 + 17;
            for (std::size_t index = 0; index < total; ++index) {
                aim.first = (index % 2 == 0) ? 0.3f : 0.4f;
                expect(captured.dispatch(aim).ok,
                    "captured high-frequency aim input remains valid");
                if (index % 128 == 127)
                    expect(highFrequency.captureApplied(captured).ok,
                        "periodic consumer drains action journal without gaps");
            }
            expect(highFrequency.captureApplied(captured).ok &&
                highFrequency.document().actions.size() == total,
                "periodic capture preserves every semantic action beyond journal capacity");
            expect(captured.step(1).ok && highFrequency.finish(captured, 0).ok,
                "complete high-frequency action stream remains a valid replay");
        }

        {
            billiardgl::GameRuntime lagged;
            billiardgl::replay::Recorder laggedRecorder;
            expect(laggedRecorder.begin(lagged, executable, content, 73).ok,
                "lagged recorder starts");
            billiardgl::GameAction aim;
            aim.type = billiardgl::ActionType::SetAimYaw;
            for (std::size_t index = 0;
                    index < billiardgl::GameRuntime::
                        kAppliedSemanticActionJournalCapacity + 1;
                    ++index) {
                aim.first = (index % 2 == 0) ? 0.5f : 0.6f;
                expect(lagged.dispatch(aim).ok,
                    "lagged high-frequency aim input remains valid");
            }
            const auto overrun = laggedRecorder.captureApplied(lagged);
            expect(!overrun.ok &&
                overrun.errorCode == "replay_action_journal_overrun" &&
                laggedRecorder.document().actions.empty(),
                "lagged recorder fails explicitly instead of silently dropping actions");
        }

        {
            billiardgl::GameRuntime resetRuntime;
            billiardgl::replay::Recorder resetRecorder;
            expect(resetRecorder.begin(resetRuntime, executable, content, 74).ok,
                "reset-generation recorder starts");
            resetRuntime.reset();
            const auto reset = resetRecorder.captureApplied(resetRuntime);
            expect(!reset.ok &&
                reset.errorCode == "replay_action_journal_reset",
                "authority journal reset invalidates recording explicitly");
        }
        {
            using namespace billiardgl;
            using namespace billiardgl::rules;
            ReplayRefereeAuthenticator authenticator;
            GameRuntime live;
            expect(live.setRulesMode(RulesMode::ChineseEightBallV1).ok,
                "Replay v2 fixture enables Chinese rules");
            live.configureNonphysicalGeneralFoulAuthority(&authenticator);
            replay::Recorder v2Recorder;
            expect(v2Recorder.begin(live,executable,content,75).ok,
                "Replay v2 recorder begins before referee effects");
            const auto legacyEffectVersion=
                RuntimeRulesReplayEffectVersion::MobileAndPlacementV1;
            const auto choiceEffectVersion=
                RuntimeRulesReplayEffectVersion::SingleRackChoicesV2;
            expect(live.rulesReplayEffectStateHash(
                       RuntimeRulesReplayEffectVersion::
                           SingleRackDeclarationsV3) ==
                    live.rulesReplayEffectStateHash(
                       RuntimeRulesReplayEffectVersion::
                           SingleRackMiscueDeclarationV4),
                "V4 is byte-compatible with V3 before any 06G declaration");
            const std::string legacyInitialEffect=
                live.rulesReplayEffectStateHash(legacyEffectVersion);
            const std::string choiceInitialEffect=
                live.rulesReplayEffectStateHash(choiceEffectVersion);

            const std::string firstShot=
                "runtime.nonphysical_general_foul.shot."+
                std::to_string(live.rulesAuthorityRevision());
            const std::string preProposalEffect=
                live.rulesReplayEffectStateHash();
            const std::uint64_t preProposalProtocolRevision=
                live.nonphysicalGeneralFoulProtocolState().authorityRevision;
            auto firstProposal=refereeRequest(live,
                RefereeAuthorityOperation::Propose,"replay.mobile.first",
                "replay.mobile.first.p",canonicalNonphysicalGeneralFoulProposal(
                    NonphysicalGeneralFoulKind::
                        MobileDeviceDisturbanceGeneralFoul,0,firstShot));
            expect(live.submitNonphysicalGeneralFoulProposal(firstProposal).ok,
                "Replay v2 fixture opens first signed event");
            expect(!live.nonphysicalFoulExplanation(),
                "pending proposal has no presentation explanation");
            expect(live.rulesReplayEffectStateHash()==preProposalEffect&&
                live.nonphysicalGeneralFoulProtocolState().authorityRevision>
                    preProposalProtocolRevision,
                "replay effect digest excludes live permission protocol state without weakening it");
            auto firstFinal=refereeRequest(live,
                RefereeAuthorityOperation::FinalDeclare,
                "replay.mobile.first","replay.mobile.first.f",
                canonicalNonphysicalGeneralFoulFinal(
                    NonphysicalGeneralFoulKind::
                        MobileDeviceDisturbanceGeneralFoul,0,
                    "replay.mobile.first",firstShot));
            expect(live.submitNonphysicalGeneralFoulFinal(firstFinal).ok&&
                live.ballInHandPlacementOffer(),
                "Replay v2 fixture commits first foul and BIH");
            expect(live.nonphysicalFoulExplanation() &&
                live.nonphysicalFoulExplanation()->kind ==
                    NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul,
                "live first foul retains committed typed explanation");
            const std::string legacyFirstEffect=
                live.rulesReplayEffectStateHash(legacyEffectVersion);
            const std::string choiceFirstEffect=
                live.rulesReplayEffectStateHash(choiceEffectVersion);
            const auto placementOffer=*live.ballInHandPlacementOffer();
            BallInHandPlacementRequest placement;
            placement.rulesetId=kBallInHandRulesetId;
            placement.capabilityId=kFreeBallPlacementCapabilityId;
            placement.flowId=kFullTableBallInHandFlowId;
            placement.decisionId=placementOffer.decisionId;
            placement.expectedAuthorityStateRevision=
                placementOffer.authorityStateRevision;
            placement.idempotencyKey="replay.mobile.placement";
            placement.actorPlayerId=placementOffer.actorPlayerId;
            placement.actionId=kConfirmFullTablePlacementActionId;
            placement.position={live.state().balls[0].position.x,
                live.state().balls[0].position.z};
            expect(live.submitBallInHandPlacement(placement).ok,
                "Replay v2 fixture commits typed BIH placement");
            expect(live.nonphysicalFoulExplanation().has_value(),
                "BIH placement retains last committed explanation");
            const std::string legacyPlacementEffect=
                live.rulesReplayEffectStateHash(legacyEffectVersion);
            const std::string choicePlacementEffect=
                live.rulesReplayEffectStateHash(choiceEffectVersion);

            GameAction yaw;yaw.type=ActionType::SetAimYaw;
            yaw.first=1.57079632679f;
            GameAction power;power.type=ActionType::SetShotPower;
            power.first=200.0f;
            GameAction shoot;shoot.type=ActionType::Shoot;
            expect(live.dispatch(yaw).ok&&live.dispatch(power).ok&&
                live.dispatch(shoot).ok&&v2Recorder.captureApplied(live).ok,
                "Replay v2 fixture records post-placement semantic shot");
            for(int tick=0;tick<5000&&live.state().ballsMoving;++tick)
                expect(live.step(1).ok,"Replay v2 fixture shot settles");
            expect(!live.state().ballsMoving&&!live.breakFoulDecisionPending()&&
                !live.eightOnBreakDecisionPending(),
                "Replay v2 fixture reaches an unblocked post-shot rack");
            expect(!live.nonphysicalFoulExplanation(),
                "a new physical adjudication clears old nonphysical explanation");

            const std::string repeatShot=
                "runtime.nonphysical_general_foul.shot."+
                std::to_string(live.rulesAuthorityRevision());
            auto repeatProposal=refereeRequest(live,
                RefereeAuthorityOperation::Propose,"replay.mobile.repeat",
                "replay.mobile.repeat.p",canonicalNonphysicalGeneralFoulProposal(
                    NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
                    0,repeatShot));
            expect(live.submitNonphysicalGeneralFoulProposal(repeatProposal).ok,
                "Replay v2 fixture opens post-shot repeat");
            auto repeatFinal=refereeRequest(live,
                RefereeAuthorityOperation::FinalDeclare,
                "replay.mobile.repeat","replay.mobile.repeat.f",
                canonicalNonphysicalGeneralFoulFinal(
                    NonphysicalGeneralFoulKind::MobileDeviceRepeatLossOfRack,
                    0,"replay.mobile.repeat",repeatShot));
            const std::string legacyRepeatParentEffect=
                live.rulesReplayEffectStateHash(legacyEffectVersion);
            const std::string choiceRepeatParentEffect=
                live.rulesReplayEffectStateHash(choiceEffectVersion);
            expect(live.submitNonphysicalGeneralFoulFinal(repeatFinal).ok&&
                live.rulesRackTerminalState().active&&live.step(1).ok&&
                v2Recorder.finish(live,1).ok,
                "Replay v2 records post-shot repeat rack terminal");
            expect(!live.nonphysicalFoulExplanation(),
                "repeat terminal does not retain a first-foul explanation");
            const std::string legacyFinalEffect=
                live.rulesReplayEffectStateHash(legacyEffectVersion);
            const std::string choiceFinalEffect=
                live.rulesReplayEffectStateHash(choiceEffectVersion);
            const replay::Document v2=v2Recorder.document();
            expect(v2.formatVersion==replay::kReplayRulesFormatVersion&&
                v2.rulesReceiptSchema==
                    replay::kReplaySingleRackDeclarationReceiptSchema&&
                v2.rulesReceipts.size()==3&&
                !v2.finalSummary.rulesEffectStateSha256.empty(),
                "Replay v2 contains first-foul, placement, and repeat receipts");
            const std::filesystem::path v2Path=
                std::filesystem::temp_directory_path()/
                "billiardgl_recorded_replay_v2_rules.json";
            std::filesystem::remove(v2Path,ignored);
            std::filesystem::remove(v2Path.string()+".tmp",ignored);
            expect(v2Recorder.saveAtomic(v2Path.string()).ok&&
                std::filesystem::exists(v2Path)&&
                !std::filesystem::exists(v2Path.string()+".tmp"),
                "Replay v2 uses the atomic disk save path");
            std::ifstream v2Input(v2Path,std::ios::binary);
            const std::string v2DiskBytes(
                (std::istreambuf_iterator<char>(v2Input)),
                std::istreambuf_iterator<char>());
            const auto v2DiskJson=json::parse(v2DiskBytes);
            replay::Document v2DiskDocument;
            replay::ReplayTransportModel v2Transport;
            expect(v2DiskJson.ok&&
                replay::parse(v2DiskJson.value,v2DiskDocument).ok&&
                v2Transport.load(v2DiskDocument,live.physicsProfile(),
                    executable,content).ok,
                "Replay v2 disk document loads through transport");
            auto transportSnapshot=v2Transport.snapshot();
            expect(transportSnapshot.loaded&&!transportSnapshot.finished&&
                transportSnapshot.documentFormatVersion==
                    replay::kReplayRulesFormatVersion&&
                transportSnapshot.rulesReceiptCount==3&&
                !transportSnapshot.rulesEffectVerified&&
                !transportSnapshot.rackTerminalActive,
                "transport exposes v2 metadata without claiming pre-final verification");
            expect(v2Transport.seekShot(0).ok&&
                !v2Transport.snapshot().rackTerminalActive&&
                !v2Transport.snapshot().rulesEffectVerified,
                "transport seek reconstructs the pre-shot rules boundary");
            while(!v2Transport.snapshot().finished)
                expect(v2Transport.stepTick().ok,
                    "transport advances the disk-loaded v2 timeline");
            transportSnapshot=v2Transport.snapshot();
            expect(transportSnapshot.finished&&
                transportSnapshot.rulesEffectVerified&&
                transportSnapshot.rackTerminalActive&&
                transportSnapshot.rackTerminalReason==
                    RulesRackTerminalReason::MobileDeviceRepeatViolation&&
                transportSnapshot.rackLosingPlayer==0&&
                transportSnapshot.rackWinningPlayer==1&&
                !transportSnapshot.rackIdentity.empty()&&
                transportSnapshot.rackTerminalDecisionId==
                    "replay.mobile.repeat",
                "transport exposes verified rack loss separately from match completion");
            std::filesystem::remove(v2Path,ignored);
            const std::string v2Bytes=json::stringify(replay::serialize(v2));
            expect(v2Bytes.find("billiardgl_replay_v2")!=std::string::npos&&
                v2Bytes.find("credential")==std::string::npos&&
                v2Bytes.find("authentication")==std::string::npos&&
                v2Bytes.find("nonce")==std::string::npos&&
                v2Bytes.find("proof")==std::string::npos&&
                v2Bytes.find("secret")==std::string::npos,
                "Replay v2 wire receipt is versioned and secret-free");
            const auto v2Decoded=json::parse(v2Bytes);
            replay::Document v2Parsed;
            expect(v2Decoded.ok&&replay::parse(v2Decoded.value,v2Parsed).ok,
                "Replay v2 round-trips committed receipts");
            replay::Player v2Player;
            const auto v2Load=v2Player.load(v2Parsed,live.physicsProfile(),executable,
                    content);
            const auto v2Seek=v2Load.ok?v2Player.seekShot(0):v2Load;
            expect(v2Load.ok&&v2Seek.ok&&
                v2Player.runtime().mobileDeviceViolationHistory()[0]==
                    MobileDeviceViolationState::GeneralFoulCommitted&&
                v2Player.runtime().ballInHandPlacementPending()&&
                !v2Player.runtime().rulesRackTerminalState().active,
                "Replay v2 seek reconstructs first history and confirmed BIH before Shoot");
            expect(v2Player.runtime().nonphysicalFoulExplanation() &&
                v2Player.runtime().nonphysicalFoulExplanation()->kind ==
                    NonphysicalGeneralFoulKind::MobileDeviceDisturbanceGeneralFoul &&
                v2Player.runtime().nonphysicalFoulExplanation()->decisionId ==
                    v2.rulesReceipts[0].event.decisionId,
                "verified replay reconstructs explanation without live authority");
            while(!v2Player.finished())
                expect(v2Player.stepTick().ok,
                    "Replay v2 replays semantic and referee timeline");
            expect(v2Player.finalHashMatches()&&
                v2Player.runtime().rulesRackTerminalState().active&&
                v2Player.runtime().mobileDeviceViolationHistory()[0]==
                    MobileDeviceViolationState::RackLossCommitted,
                "Replay v2 final digest binds reconstructed rack terminal");
            expect(!v2Player.runtime().nonphysicalFoulExplanation(),
                "replay terminal clears first-foul explanation");

            auto legacyV2=v2;
            legacyV2.rulesReceiptSchema=replay::kReplayRulesReceiptSchema;
            legacyV2.initialRulesEffectSha256=legacyInitialEffect;
            legacyV2.rulesReceipts[0].event.parentEffectSha256=
                legacyInitialEffect;
            legacyV2.rulesReceipts[0].event.committedEffectSha256=
                legacyFirstEffect;
            legacyV2.rulesReceipts[1].event.parentEffectSha256=
                legacyFirstEffect;
            legacyV2.rulesReceipts[1].event.committedEffectSha256=
                legacyPlacementEffect;
            legacyV2.rulesReceipts[2].event.parentEffectSha256=
                legacyRepeatParentEffect;
            legacyV2.rulesReceipts[2].event.committedEffectSha256=
                legacyFinalEffect;
            for(auto& receipt:legacyV2.rulesReceipts)
                receipt.receiptSha256=replay::rulesReceiptSha256(receipt);
            legacyV2.finalSummary.rulesEffectStateSha256=legacyFinalEffect;
            const auto legacyWire=replay::serialize(legacyV2);
            replay::Document legacyParsed;
            replay::Player legacyPlayer;
            expect(replay::parse(legacyWire,legacyParsed).ok&&
                legacyParsed.rulesReceiptSchema==
                    replay::kReplayRulesReceiptSchema&&
                legacyPlayer.load(legacyParsed,live.physicsProfile(),
                    executable,content).ok,
                "legacy Replay v2 receipt schema remains readable");
            while(!legacyPlayer.finished()){
                const auto step=legacyPlayer.stepTick();
                expect(step.ok,"legacy Replay v2 effect chain still replays");
            }
            expect(legacyPlayer.finalHashMatches(),
                "legacy Replay v2 retains its original effect digest semantics");

            auto choiceV2=v2;
            choiceV2.rulesReceiptSchema=
                replay::kReplaySingleRackChoiceReceiptSchema;
            choiceV2.initialRulesEffectSha256=choiceInitialEffect;
            choiceV2.rulesReceipts[0].event.parentEffectSha256=
                choiceInitialEffect;
            choiceV2.rulesReceipts[0].event.committedEffectSha256=
                choiceFirstEffect;
            choiceV2.rulesReceipts[1].event.parentEffectSha256=
                choiceFirstEffect;
            choiceV2.rulesReceipts[1].event.committedEffectSha256=
                choicePlacementEffect;
            choiceV2.rulesReceipts[2].event.parentEffectSha256=
                choiceRepeatParentEffect;
            choiceV2.rulesReceipts[2].event.committedEffectSha256=
                choiceFinalEffect;
            for(auto& receipt:choiceV2.rulesReceipts)
                receipt.receiptSha256=replay::rulesReceiptSha256(receipt);
            choiceV2.finalSummary.rulesEffectStateSha256=choiceFinalEffect;
            const auto choiceV2Wire=replay::serialize(choiceV2);
            replay::Document choiceV2Parsed;
            replay::Player choiceV2Player;
            expect(replay::parse(choiceV2Wire,choiceV2Parsed).ok&&
                choiceV2Parsed.rulesReceiptSchema==
                    replay::kReplaySingleRackChoiceReceiptSchema&&
                choiceV2Player.load(choiceV2Parsed,live.physicsProfile(),
                    executable,content).ok,
                "previous comprehensive Replay v2 schema remains readable");
            while(!choiceV2Player.finished()){
                const auto step=choiceV2Player.stepTick();
                expect(step.ok,
                    "previous comprehensive Replay v2 still replays");
            }
            expect(choiceV2Player.finalHashMatches(),
                "previous comprehensive Replay v2 keeps its effect digest semantics");

            auto mutation=v2;
            mutation.rulesReceipts[0].event.foulKind =
                static_cast<NonphysicalGeneralFoulKind>(999);
            mutation.rulesReceipts[0].receiptSha256 =
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            replay::Player invalidKind;
            const auto invalidLoad = invalidKind.load(mutation, live.physicsProfile(),
                executable, content);
            expect(!invalidLoad.ok || !invalidKind.seekShot(0).ok,
                "unknown typed referee kind cannot create a replay explanation");
            expect(!invalidKind.runtime().nonphysicalFoulExplanation(),
                "rejected unknown kind leaves derived explanation absent");
            mutation=v2;
            mutation.rulesReceipts[0].event.offendingPlayer=1;
            expect(!replay::validate(mutation,live.physicsProfile(),executable,
                    content).ok,
                "Replay v2 rejects mutated receipt fields");
            mutation=v2;
            mutation.rulesReceipts[0].event.selectedBallId=4;
            mutation.rulesReceipts[0].receiptSha256=
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expect(!replay::validate(mutation,live.physicsProfile(),executable,
                    content).ok,
                "comprehensive receipt rejects an inapplicable selected-ball payload");
            auto missingSelected=replay::serialize(v2);
            missingSelected["referee_receipts"].asArray()[0].asObject().erase(
                "selected_ball_id");
            replay::Document missingSelectedDocument;
            expect(!replay::parse(missingSelected,missingSelectedDocument).ok,
                "comprehensive receipt parser rejects a missing typed field");
            mutation=v2;
            mutation.rulesReceiptSchema="unknown_rules_receipts";
            expect(!replay::validate(mutation,live.physicsProfile(),executable,
                    content).ok,
                "Replay v2 rejects unknown receipt schema");
            mutation=v2;
            mutation.formatVersion=replay::kReplayFormatVersion;
            expect(!replay::validate(mutation,live.physicsProfile(),executable,
                    content).ok,
                "Replay v1 cannot silently accept rules-changing receipts");
            mutation=v2;
            mutation.finalSummary.authorityStateSchema.clear();
            mutation.finalSummary.authorityStateSha256.clear();
            mutation.finalSummary.rulesEffectStateSha256=
                std::string(64,'0');
            replay::Player effectOnlyMutation;
            expect(replay::validate(mutation,live.physicsProfile(),executable,
                    content).ok&&
                effectOnlyMutation.load(mutation,live.physicsProfile(),executable,
                    content).ok,
                "Replay v2 permits the legacy authority digest to be absent");
            replay::ValidationResult effectOnlyStep{true,""};
            while(!effectOnlyMutation.finished()){
                effectOnlyStep=effectOnlyMutation.stepTick();
                if(!effectOnlyMutation.finished())
                    expect(effectOnlyStep.ok,
                        "Replay v2 effect-only mutation advances before the final boundary");
            }
            expect(!effectOnlyStep.ok&&
                effectOnlyStep.errorCode=="replay_final_hash_mismatch"&&
                !effectOnlyMutation.finalHashMatches(),
                "Replay v2 always verifies the canonical effect digest");
            mutation=v2;
            mutation.rulesReceipts[0].event.offendingPlayer=1;
            mutation.rulesReceipts[0].event.opponentPlayer=0;
            mutation.rulesReceipts[0].receiptSha256=
                replay::rulesReceiptSha256(mutation.rulesReceipts[0]);
            expect(replay::validate(mutation,live.physicsProfile(),executable,
                    content).ok,
                "self-consistent mutation reaches runtime effect verification");
            replay::Player atomicMutation;
            expect(atomicMutation.load(mutation,live.physicsProfile(),executable,
                    content).ok&&
                !atomicMutation.seekShot(0).ok&&
                atomicMutation.runtime().mobileDeviceViolationHistory()[0]==
                    MobileDeviceViolationState::None&&
                atomicMutation.runtime().mobileDeviceViolationHistory()[1]==
                    MobileDeviceViolationState::None&&
                !atomicMutation.runtime().rulesRackTerminalState().active,
                "runtime rejects a re-digested semantic mutation atomically");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
