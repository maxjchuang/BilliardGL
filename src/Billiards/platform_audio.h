#pragma once

#include "audio/audio_mixer_model.h"
#include "audio/audio_asset_policy.h"

#include <memory>

namespace billiardgl::audio {

struct AudioPreparationResult {
    AudioConsumeResult result;
    double deviceMilliseconds = 0.0;
    double assetMilliseconds = 0.0;
};

struct SdlAudioConsumerOptions {
    AudioAssetPack pack = AudioAssetPack::ShippingLegacy;
    // Explicit review-only master directory; never searched or silently aliased.
    std::string candidateRoot;
    int loopFadeInMilliseconds = 120;
};

// Interactive product opt-in only. Legacy/default consumers remain unchanged.
SdlAudioConsumerOptions candidateAudioReviewOptionsFromEnvironment();

// SDL_mixer implementation used only by platform builds. Construction and bus
// updates do not open a device. Product startup calls prepare() before play;
// compatibility callers retain lazy initialization on first audible consume.
class SdlMixerAudioCueConsumer final : public AudioCueConsumer {
public:
    explicit SdlMixerAudioCueConsumer(SdlAudioConsumerOptions options = {});
    ~SdlMixerAudioCueConsumer() override;

    SdlMixerAudioCueConsumer(const SdlMixerAudioCueConsumer&) = delete;
    SdlMixerAudioCueConsumer& operator=(
        const SdlMixerAudioCueConsumer&) = delete;

    AudioConsumeResult updateBus(
        AudioBus bus, const AudioBusState& state) override;
    AudioConsumeResult consume(
        const std::vector<MixedAudioCue>& cues) override;
    AudioConsumerStatus status() const override;
    // Initializes and caches all mapped assets without queuing any sound.
    AudioPreparationResult prepare();
    // Explicit audition/shutdown transition; does not touch runtime authority.
    std::size_t fadeOutLoops(int milliseconds = 120);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace billiardgl::audio

namespace billiardgl {

// Compatibility wrappers retained for the legacy application.
void playBackgroundLoop();
void playHit();
void playBallIn();
void playGameOver();
audio::AudioConsumerStatus legacyAudioStatus();

}  // namespace billiardgl
