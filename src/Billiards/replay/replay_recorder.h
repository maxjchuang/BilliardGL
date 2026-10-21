#pragma once

#include "replay/replay_v1.h"

#include <cstdint>
#include <string>

namespace billiardgl::replay {

class Recorder {
public:
    ValidationResult begin(const GameRuntime& runtime,
        const std::string& executableVersion,
        const std::string& contentSha256, std::uint32_t seed);
    ValidationResult recordApplied(const GameRuntime& runtime,
        SemanticActionType type, float first = 0.0f, float second = 0.0f);
    ValidationResult captureApplied(const GameRuntime& runtime);
    ValidationResult finish(const GameRuntime& runtime, int shotCount);
    ValidationResult saveAtomic(const std::string& path) const;

    bool active() const noexcept { return active_; }
    bool finished() const noexcept { return finished_; }
    int shotCount() const noexcept { return shotCount_; }
    const Document& document() const noexcept { return document_; }

private:
    ValidationResult failClosed(const std::string& errorCode);

    Document document_;
    PhysicsProfile profile_;
    bool active_ = false;
    bool finished_ = false;
    std::string terminalError_;
    std::uint64_t lastAppliedSequence_ = 0;
    std::uint64_t initialAppliedSequence_ = 0;
    std::uint64_t journalGeneration_ = 0;
    std::uint64_t lastRulesReplaySequence_ = 0;
    std::uint64_t rulesReplayJournalGeneration_ = 0;
    int shotCount_ = 0;
};

}  // namespace billiardgl::replay
