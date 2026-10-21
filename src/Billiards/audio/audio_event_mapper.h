#pragma once

#include "game_runtime.h"
#include "physics_telemetry.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace billiardgl::audio {

enum class AudioBus {
    Music,
    Ambience,
    Effects,
    Ui,
};

enum class AudioCueKind {
    BallBall,
    Rail,
    PocketJaw,
    PocketDrop,
    CueImpact,
    Foul,
    UiFeedback,
    MatchFinish,
    MusicLoop,
    VenueAmbience,
};

// The foundation produces requests only. A platform backend must explicitly
// consume a cue before any code may claim that audio was queued or played.
enum class AudioDeliveryStatus {
    CueOnly,
};

struct AudioCue {
    std::uint64_t tick = 0;
    AudioCueKind kind = AudioCueKind::BallBall;
    AudioBus bus = AudioBus::Effects;
    double gain = 0.0;
    double pitch = 1.0;
    double sourceImpulseNs = 0.0;
    std::size_t aggregateCount = 0;
    int primaryId = -1;
    int secondaryId = -1;
    std::uint64_t sourceEventSequence = 0;
    bool loop = false;
    AudioDeliveryStatus delivery = AudioDeliveryStatus::CueOnly;
};

struct AudioMapperConfig {
    double minimumAudibleImpulseNs = 0.002;
    double maximumMappedImpulseNs = 0.35;
    double minimumContactGain = 0.12;
    double maximumContactGain = 1.0;
    double minimumContactPitch = 0.88;
    double maximumContactPitch = 1.12;
    double fallbackEventImpulseNs = 0.04;
    std::size_t maximumCuesPerTick = 8;
};

class AudioEventMapper {
public:
    explicit AudioEventMapper(AudioMapperConfig config = {});

    // Read-only transformation. Events from other ticks are ignored. A failed
    // physics step never publishes contact audio.
    std::vector<AudioCue> mapTick(std::uint64_t tick,
        const PhysicsStepTelemetry& telemetry,
        const std::vector<RuntimeEvent>& events) const;

    const AudioMapperConfig& config() const noexcept { return config_; }

private:
    AudioMapperConfig config_;
};

const char* audioBusName(AudioBus bus) noexcept;
const char* audioCueKindName(AudioCueKind kind) noexcept;

}  // namespace billiardgl::audio
