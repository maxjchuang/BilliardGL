#pragma once

#include "audio/audio_event_mapper.h"
#include "product_settings.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::audio {

struct AudioBusState {
    double volume = 1.0;
    bool muted = false;
};

struct MixedAudioCue {
    AudioCue cue;
    double busVolume = 1.0;
    double outputGain = 0.0;
    bool suppressed = false;
    AudioDeliveryStatus delivery = AudioDeliveryStatus::CueOnly;
};

enum class AudioConsumerState {
    Dormant,
    Ready,
    Degraded,
    DeviceFailed,
    AssetFailed,
};

struct AudioConsumerStatus {
    AudioConsumerState state = AudioConsumerState::Dormant;
    std::string code;
    bool deviceInitialized = false;
    std::size_t cachedAssetCount = 0;
    std::uint64_t queuedCueCount = 0;
    std::uint64_t suppressedCueCount = 0;
    std::uint64_t degradedAliasCount = 0;
    std::uint64_t ignoredPitchCount = 0;
    std::string assetPackId = "shipping_legacy_alias";
    bool candidateReview = false;
};

struct AudioConsumeResult {
    bool ok = true;
    std::string code;
    std::size_t queuedCount = 0;
    std::size_t suppressedCount = 0;
    std::size_t degradedAliasCount = 0;
    std::size_t ignoredPitchCount = 0;
};

// Platform implementations may consume these immutable requests later. The
// model itself never opens an audio device and never calls this interface.
class AudioCueConsumer {
public:
    virtual ~AudioCueConsumer() = default;
    virtual AudioConsumeResult updateBus(
        AudioBus bus, const AudioBusState& state) = 0;
    virtual AudioConsumeResult consume(
        const std::vector<MixedAudioCue>& cues) = 0;
    virtual AudioConsumerStatus status() const = 0;
};

const char* audioConsumerStateName(AudioConsumerState state) noexcept;

class AudioMixerModel {
public:
    AudioMixerModel();
    explicit AudioMixerModel(const settings::ProductSettings& settings);

    // Takes effect for the next mix call; no restart or device interaction.
    void applySettings(const settings::ProductSettings& settings) noexcept;
    const AudioBusState& bus(AudioBus bus) const noexcept;

    std::vector<MixedAudioCue> mix(
        const std::vector<AudioCue>& cues) const;
    std::vector<AudioCue> persistentCues(std::uint64_t tick) const;

private:
    std::array<AudioBusState, 4> buses_{};
};

}  // namespace billiardgl::audio
