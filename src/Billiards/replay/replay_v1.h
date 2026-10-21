#pragma once

#include "automation_json.h"
#include "deterministic_hash.h"
#include "game_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::replay {

constexpr int kReplayFormatVersion = 1;
constexpr int kReplayRulesFormatVersion = 2;
inline constexpr const char* kReplayAuthorityStateSchema = "d6_authority_v1";
inline constexpr const char* kReplayRulesReceiptSchema =
    "wpa_ii_17_committed_receipts_v1";
inline constexpr const char* kReplaySingleRackChoiceReceiptSchema =
    "wpa_heyball_single_rack_choices_v2";
inline constexpr const char* kReplaySingleRackDeclarationReceiptSchema =
    "wpa_heyball_single_rack_declarations_v3";
inline constexpr const char* kReplaySingleRackMiscueDeclarationReceiptSchema =
    "wpa_heyball_single_rack_miscue_declarations_v4";

enum class SemanticActionType {
    SetAimYaw,
    SetShotPower,
    SetTipOffset,
    SetCalledPocket,
    Shoot
};

struct SemanticAction {
    std::uint64_t tick = 0;
    SemanticActionType type = SemanticActionType::SetAimYaw;
    float first = 0.0f;
    float second = 0.0f;
};

struct RulesReceipt {
    std::uint64_t tick = 0;
    std::uint64_t sequence = 0;
    std::size_t afterSemanticActionOrdinal = 0;
    RuntimeRulesReplayEvent event;
    std::string receiptSha256;
};

struct FinalSummary {
    std::uint64_t tick = 0;
    int shotCount = 0;
    std::string stateSha256;
    // Optional Replay-local supplement. Legacy Replay v1 documents retain
    // their original state_sha256 meaning; new recorders bind D6 authority
    // state that is intentionally absent from the frozen global state hash.
    std::string authorityStateSchema;
    std::string authorityStateSha256;
    std::string rulesEffectStateSha256;
};

struct Document {
    int formatVersion = kReplayFormatVersion;
    std::string executableVersion;
    int contentVersion = 1;
    std::string contentSha256;
    std::string physicsProfileId;
    std::string physicsProfileSha256;
    RulesMode rulesMode = RulesMode::Legacy;
    GameState initialState;
    std::uint32_t seed = 0;
    std::uint64_t initialRulesRevision = 0;
    std::string initialRulesEffectSha256;
    std::vector<SemanticAction> actions;
    std::string rulesReceiptSchema;
    std::vector<RulesReceipt> rulesReceipts;
    FinalSummary finalSummary;
};

struct ValidationResult {
    bool ok = false;
    std::string errorCode;
};

std::string physicsProfileSha256(const PhysicsProfile& profile);
std::string replayAuthorityStateSha256(const GameState& state);
std::string rulesReceiptSha256(const RulesReceipt& receipt);
ValidationResult validate(const Document& document,
    const PhysicsProfile& currentProfile,
    const std::string& executableVersion,
    const std::string& contentSha256);
json::Value serialize(const Document& document);
ValidationResult parse(const json::Value& value, Document& document);

class Player {
public:
    ValidationResult load(const Document& document,
        const PhysicsProfile& currentProfile,
        const std::string& executableVersion,
        const std::string& contentSha256);
    void setPaused(bool paused) { paused_ = paused; }
    bool paused() const { return paused_; }
    ValidationResult setPlaybackRate(double rate);
    double playbackRate() const { return playbackRate_; }
    ValidationResult advanceScheduledFrame();
    ValidationResult stepTick();
    ValidationResult seekShot(std::size_t shotOrdinal);
    bool finished() const;
    std::string finalStateSha256() const;
    bool finalHashMatches() const;
    int documentFormatVersion() const noexcept
    {
        return loaded_ ? document_.formatVersion : 0;
    }
    std::size_t rulesReceiptCount() const noexcept
    {
        return loaded_ ? document_.rulesReceipts.size() : 0;
    }
    bool rulesEffectVerified() const
    {
        return loaded_ && document_.formatVersion == kReplayRulesFormatVersion &&
            finished() && finalHashMatches();
    }
    const GameRuntime& runtime() const { return runtime_; }

private:
    ValidationResult resetToInitial();
    ValidationResult applyAction(const SemanticAction& action);
    ValidationResult applyActionsAtCurrentTick(std::size_t stopBefore =
        static_cast<std::size_t>(-1));

    Document document_;
    PhysicsProfile profile_;
    GameRuntime runtime_;
    std::size_t nextAction_ = 0;
    std::size_t nextRulesReceipt_ = 0;
    int shotCount_ = 0;
    bool loaded_ = false;
    bool paused_ = true;
    double playbackRate_ = 1.0;
    double schedulingAccumulator_ = 0.0;
    ValidationResult applyTimelineAtCurrentTick(std::size_t stopBefore =
        static_cast<std::size_t>(-1));
};

}  // namespace billiardgl::replay
