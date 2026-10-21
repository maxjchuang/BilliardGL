#include "replay/replay_recorder.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace billiardgl::replay {
namespace {

ValidationResult success() { return {true, ""}; }
ValidationResult failure(const char* code) { return {false, code}; }

ValidationResult declarationTimelineRepresentable(const GameRuntime& runtime)
{
    using rules::DoubleHitCloseCasePhase;
    using rules::FrozenBallDeclarationPhase;
    using rules::FrozenCushionDeclarationPhase;
    using rules::PushReviewPhase;
    using rules::UnintentionalBreakMiscueDeclarationPhase;

    const auto miscuePhase =
        runtime.unintentionalBreakMiscueDeclarationState().phase;
    if (miscuePhase ==
            UnintentionalBreakMiscueDeclarationPhase::ProposalOpen)
        return failure("replay_rules_declaration_proposal_pending");
    if (miscuePhase ==
            UnintentionalBreakMiscueDeclarationPhase::Cancelled)
        return failure("replay_rules_declaration_cancel_unsupported");
    if (miscuePhase ==
            UnintentionalBreakMiscueDeclarationPhase::Invalidated)
        return failure("replay_rules_declaration_transition_unsupported");

    if (runtime.pushReviewState().phase == PushReviewPhase::ProposalOpen ||
        runtime.doubleHitCloseCaseState().phase ==
            DoubleHitCloseCasePhase::ProposalOpen ||
        runtime.frozenBallDeclarationState().phase ==
            FrozenBallDeclarationPhase::ProposalOpen ||
        runtime.frozenCushionDeclarationState().phase ==
            FrozenCushionDeclarationPhase::ProposalOpen)
        return failure("replay_rules_declaration_proposal_pending");
    if (runtime.pushReviewState().phase == PushReviewPhase::Cancelled ||
        runtime.doubleHitCloseCaseState().phase ==
            DoubleHitCloseCasePhase::Cancelled ||
        runtime.frozenBallDeclarationState().phase ==
            FrozenBallDeclarationPhase::Cancelled ||
        runtime.frozenCushionDeclarationState().phase ==
            FrozenCushionDeclarationPhase::Cancelled)
        return failure("replay_rules_declaration_cancel_unsupported");
    if ((!runtime.pushReviewState().committed &&
            runtime.pushReviewState().phase == PushReviewPhase::Invalidated) ||
        (!runtime.doubleHitCloseCaseState().committed &&
            runtime.doubleHitCloseCaseState().phase ==
                DoubleHitCloseCasePhase::Invalidated) ||
        (!runtime.frozenBallDeclarationState().committed &&
            runtime.frozenBallDeclarationState().phase ==
                FrozenBallDeclarationPhase::Invalidated) ||
        (!runtime.frozenCushionDeclarationState().committed &&
            runtime.frozenCushionDeclarationState().phase ==
                FrozenCushionDeclarationPhase::Invalidated))
        return failure("replay_rules_declaration_transition_unsupported");
    return {true, ""};
}

bool validHash(const std::string& value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
        [](unsigned char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
}

}  // namespace

ValidationResult Recorder::begin(const GameRuntime& runtime,
    const std::string& executableVersion, const std::string& contentSha256,
    std::uint32_t seed)
{
    terminalError_.clear();
    const ValidationResult declarations =
        declarationTimelineRepresentable(runtime);
    if (!declarations.ok) return failClosed(declarations.errorCode);
    if (runtime.state().ballsMoving) return failure("replay_recording_requires_stopped_state");
    if (runtime.state().gameOver)
        return failure("replay_recording_match_complete");
    if (runtime.rulesRackTerminalState().active ||
        runtime.mobileDeviceViolationHistory()[0] !=
            rules::MobileDeviceViolationState::None ||
        runtime.mobileDeviceViolationHistory()[1] !=
            rules::MobileDeviceViolationState::None)
        return failure("replay_recording_rules_state_unsupported");
    if(runtime.pushReviewState().committed||
       runtime.doubleHitCloseCaseState().committed||
       runtime.frozenBallDeclarationState().committed||
       runtime.frozenCushionDeclarationState().committed||
       runtime.unintentionalBreakMiscueDeclarationState().committed)
        return failure("replay_recording_rules_state_unsupported");
    if (runtime.rulesMode() == RulesMode::ChineseEightBallV1 &&
        runtime.matchState().phase != rules::MatchPhase::Break)
        return failure("replay_recording_rules_state_unsupported");
    if (runtime.boundaryMode() != PhysicsBoundaryMode::ProductionTable)
        return failure("replay_recording_boundary_mode_unsupported");
    if (runtime.state().nextPocketCaptureSequence != 1)
        return failure("replay_recording_initial_state_unsupported");
    for (const BallState& ball : runtime.state().balls) {
        if (ball.physicalDomain != PhysicalBallDomain::ClothSupported ||
            ball.pocketInteraction.phase != PocketInteractionPhase::Outside ||
            ball.pocketInteraction.pocketId != -1 ||
            ball.pocketInteraction.captureSequence != 0)
            return failure("replay_recording_initial_state_unsupported");
    }
    if (executableVersion.empty() || !validHash(contentSha256))
        return failure("replay_recording_binding_invalid");
    Document candidate;
    candidate.executableVersion = executableVersion;
    candidate.contentSha256 = contentSha256;
    candidate.physicsProfileId = runtime.physicsProfile().id;
    candidate.physicsProfileSha256 = physicsProfileSha256(runtime.physicsProfile());
    candidate.rulesMode = runtime.rulesMode();
    candidate.initialState = runtime.state();
    candidate.seed = seed;
    candidate.initialRulesRevision=runtime.rulesAuthorityRevision();
    candidate.initialRulesEffectSha256=
        runtime.rulesReplayEffectStateHash();
    document_ = candidate;
    profile_ = runtime.physicsProfile();
    active_ = true;
    finished_ = false;
    shotCount_ = 0;
    lastAppliedSequence_ = runtime.appliedSemanticActions().empty() ? 0 :
        runtime.appliedSemanticActions().back().sequence;
    initialAppliedSequence_ = lastAppliedSequence_;
    journalGeneration_ = runtime.appliedSemanticActionJournalGeneration();
    lastRulesReplaySequence_=runtime.rulesReplayEvents().empty()?0:
        runtime.rulesReplayEvents().back().sequence;
    rulesReplayJournalGeneration_=
        runtime.rulesReplayEventJournalGeneration();
    return success();
}

ValidationResult Recorder::recordApplied(const GameRuntime& runtime,
    SemanticActionType type, float first, float second)
{
    if (!terminalError_.empty()) return {false, terminalError_};
    if (!active_ || finished_) return failure("replay_recording_inactive");
    if (runtime.appliedSemanticActionJournalGeneration() != journalGeneration_)
        return failure("replay_action_journal_reset");
    if (!std::isfinite(first) || !std::isfinite(second))
        return failure("replay_action_nonfinite");
    const auto& applied = runtime.appliedSemanticActions();
    if (applied.empty()) return failure("replay_action_not_applied");
    const AppliedSemanticAction& authority = applied.back();
    AppliedSemanticActionType expected;
    switch (type) {
    case SemanticActionType::SetAimYaw:
        expected = AppliedSemanticActionType::SetAimYaw;
        break;
    case SemanticActionType::SetShotPower:
        expected = AppliedSemanticActionType::SetShotPower;
        break;
    case SemanticActionType::SetTipOffset:
        expected = AppliedSemanticActionType::SetTipOffset;
        break;
    case SemanticActionType::SetCalledPocket:
        expected = AppliedSemanticActionType::SetCalledPocket;
        break;
    case SemanticActionType::Shoot:
        expected = AppliedSemanticActionType::Shoot;
        break;
    }
    if (authority.sequence != lastAppliedSequence_ + 1 ||
        authority.type != expected || authority.first != first ||
        authority.second != second)
        return failure("replay_action_not_applied");
    document_.actions.push_back(
        {authority.tick, type, authority.first, authority.second});
    if (type == SemanticActionType::Shoot) ++shotCount_;
    lastAppliedSequence_ = authority.sequence;
    return success();
}

ValidationResult Recorder::captureApplied(const GameRuntime& runtime)
{
    if (!terminalError_.empty()) return {false, terminalError_};
    if (!active_ || finished_) return failure("replay_recording_inactive");
    const ValidationResult declarations =
        declarationTimelineRepresentable(runtime);
    if (!declarations.ok) return failClosed(declarations.errorCode);
    if (runtime.appliedSemanticActionJournalGeneration() != journalGeneration_)
        return failure("replay_action_journal_reset");
    if (runtime.oldestAppliedSemanticActionSequence() >
        lastAppliedSequence_ + 1)
        return failure("replay_action_journal_overrun");
    for (const AppliedSemanticAction& action :
            runtime.appliedSemanticActionsSince(lastAppliedSequence_)) {
        SemanticActionType type;
        switch (action.type) {
        case AppliedSemanticActionType::SetAimYaw:
            type = SemanticActionType::SetAimYaw;
            break;
        case AppliedSemanticActionType::SetShotPower:
            type = SemanticActionType::SetShotPower;
            break;
        case AppliedSemanticActionType::SetTipOffset:
            type = SemanticActionType::SetTipOffset;
            break;
        case AppliedSemanticActionType::SetCalledPocket:
            type = SemanticActionType::SetCalledPocket;
            break;
        case AppliedSemanticActionType::Shoot:
            type = SemanticActionType::Shoot;
            break;
        }
        document_.actions.push_back(
            {action.tick, type, action.first, action.second});
        if (type == SemanticActionType::Shoot) ++shotCount_;
        lastAppliedSequence_ = action.sequence;
    }
    if(runtime.rulesReplayEventJournalGeneration()!=
            rulesReplayJournalGeneration_)
        return failure("replay_rules_event_journal_reset");
    for(const RuntimeRulesReplayEvent& event:runtime.rulesReplayEvents()){
        if(event.sequence<=lastRulesReplaySequence_)continue;
        if(event.sequence!=lastRulesReplaySequence_+1||
           event.afterAppliedSemanticActionSequence<initialAppliedSequence_)
            return failure("replay_rules_event_journal_overrun");
        const std::uint64_t ordinal=
            event.afterAppliedSemanticActionSequence-initialAppliedSequence_;
        if(ordinal>document_.actions.size())
            return failure("replay_rules_event_order_invalid");
        RulesReceipt receipt;
        receipt.tick=event.tick;receipt.sequence=event.sequence;
        receipt.afterSemanticActionOrdinal=static_cast<std::size_t>(ordinal);
        receipt.event=event;receipt.receiptSha256=rulesReceiptSha256(receipt);
        document_.rulesReceipts.push_back(std::move(receipt));
        document_.formatVersion=kReplayRulesFormatVersion;
        if(event.type==RuntimeRulesReplayEventType::
                UnintentionalBreakMiscueDeclarationCommit)
            document_.rulesReceiptSchema=
                kReplaySingleRackMiscueDeclarationReceiptSchema;
        else if(document_.rulesReceiptSchema!=
                kReplaySingleRackMiscueDeclarationReceiptSchema)
            document_.rulesReceiptSchema=
                kReplaySingleRackDeclarationReceiptSchema;
        lastRulesReplaySequence_=event.sequence;
    }
    if(runtime.unintentionalBreakMiscueDeclarationState().committed){
        const auto& declaration =
            runtime.unintentionalBreakMiscueDeclarationState();
        const bool represented=std::any_of(document_.rulesReceipts.begin(),
            document_.rulesReceipts.end(),[&](const RulesReceipt& receipt){
                const RuntimeRulesReplayEvent& event = receipt.event;
                return event.type==RuntimeRulesReplayEventType::
                        UnintentionalBreakMiscueDeclarationCommit&&
                    event.declarationRackIdentity==
                        declaration.rackIdentity&&
                    event.proposalDecisionId==
                        declaration.proposalDecisionId&&
                    event.authoritySubjectId==
                        declaration.refereeSubjectId&&
                    event.actorPlayerId==
                        declaration.context.actorPlayerId&&
                    event.previousRulesRevision==
                        declaration.context.parentRevision;
            });
        if(!represented)
            return failClosed("replay_rules_declaration_receipt_missing");
    }
    return success();
}

ValidationResult Recorder::finish(const GameRuntime& runtime, int shotCount)
{
    if (!terminalError_.empty()) return {false, terminalError_};
    if (!active_ || finished_) return failure("replay_recording_inactive");
    const ValidationResult captured = captureApplied(runtime);
    if (!captured.ok) return captured;
    if (shotCount < 0) return failure("replay_final_shot_count_invalid");
    if (shotCount != shotCount_)
        return failure("replay_final_shot_count_mismatch");
    if (!document_.actions.empty() &&
        runtime.tick() <= document_.actions.back().tick)
        return failure("replay_final_tick_must_follow_actions");
    document_.finalSummary.tick = runtime.tick();
    document_.finalSummary.shotCount = shotCount;
    document_.finalSummary.stateSha256 = gameStateSha256(runtime.state());
    document_.finalSummary.authorityStateSchema =
        kReplayAuthorityStateSchema;
    document_.finalSummary.authorityStateSha256 =
        replayAuthorityStateSha256(runtime.state());
    if(document_.formatVersion==kReplayRulesFormatVersion){
        if(runtime.tick()==0)
            return failure("replay_rules_final_tick_invalid");
        document_.finalSummary.rulesEffectStateSha256=
            runtime.rulesReplayEffectStateHash();
    }
    const ValidationResult checked = validate(document_, profile_,
        document_.executableVersion, document_.contentSha256);
    if (!checked.ok) return checked;
    finished_ = true;
    active_ = false;
    return success();
}

ValidationResult Recorder::failClosed(const std::string& errorCode)
{
    terminalError_ = errorCode;
    active_ = false;
    finished_ = false;
    return {false, terminalError_};
}

ValidationResult Recorder::saveAtomic(const std::string& path) const
{
    if (!finished_) return failure("replay_recording_not_finished");
    const std::filesystem::path destination(path);
    const std::filesystem::path temporary = destination.string() + ".tmp";
    try {
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return failure("replay_save_open_failed");
            output << json::stringify(serialize(document_)) << '\n';
            output.flush();
            if (!output) {
                std::filesystem::remove(temporary);
                return failure("replay_save_write_failed");
            }
        }
        std::filesystem::rename(temporary, destination);
        return success();
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return failure("replay_save_commit_failed");
    }
}

}  // namespace billiardgl::replay
