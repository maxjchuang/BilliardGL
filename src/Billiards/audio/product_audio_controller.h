#pragma once

#include "audio/audio_event_mapper.h"
#include "audio/audio_mixer_model.h"

#include <cstdint>
#include <vector>

namespace billiardgl::audio {

struct ProductAudioStep {
    // The owner increments generation whenever its RuntimeEvent sequence is
    // reset. Reusing a numeric tick in a new generation is valid.
    std::uint64_t generation = 0;
    std::uint64_t tick = 0;
    PhysicsStepTelemetry telemetry;
    // May be the whole retained journal; controller sequence filtering makes
    // repeat submissions idempotent.
    std::vector<RuntimeEvent> runtimeEvents;
};

struct ProductAudioResult {
    bool ok = true;
    std::string errorCode;
    bool duplicateOrOutOfOrder = false;
    std::size_t mappedCueCount = 0;
    std::size_t mixedCueCount = 0;
    AudioConsumeResult consumer;
};

struct ProductAudioSnapshot {
    bool hasGeneration = false;
    std::uint64_t generation = 0;
    bool hasProcessedTick = false;
    std::uint64_t lastProcessedTick = 0;
    std::uint64_t lastEventSequence = 0;
    std::uint64_t processedTickCount = 0;
    std::uint64_t duplicateTickCount = 0;
    std::uint64_t mappedCueCount = 0;
    std::uint64_t queuedCueCount = 0;
    std::uint64_t suppressedCueCount = 0;
    AudioConsumerStatus consumer;
};

// Pure orchestration layer. It reads supplied facts, never mutates GameRuntime,
// and treats consumer errors as explicit non-throwing product status.
class ProductAudioController {
public:
    ProductAudioController(AudioCueConsumer& consumer,
        const settings::ProductSettings& settings,
        AudioMapperConfig mapperConfig = {});

    ProductAudioResult applySettings(
        const settings::ProductSettings& settings);
    // Explicit product-owner opt-in. Existing shipping/fake-consumer flows do
    // not start loops merely by constructing or updating settings.
    ProductAudioResult startPersistentLoops(const settings::ProductSettings& settings);
    ProductAudioResult consumeStep(const ProductAudioStep& step,
        const settings::ProductSettings& settings);
    std::vector<ProductAudioResult> consumeSteps(
        const std::vector<ProductAudioStep>& steps,
        const settings::ProductSettings& settings);
    ProductAudioSnapshot snapshot() const;

private:
    void beginGeneration(std::uint64_t generation);

    AudioCueConsumer& consumer_;
    AudioEventMapper mapper_;
    AudioMixerModel mixer_;
    bool persistentLoopsEnabled_ = false;
    bool hasGeneration_ = false;
    std::uint64_t generation_ = 0;
    bool hasProcessedTick_ = false;
    std::uint64_t lastProcessedTick_ = 0;
    std::uint64_t lastEventSequence_ = 0;
    std::uint64_t processedTickCount_ = 0;
    std::uint64_t duplicateTickCount_ = 0;
    std::uint64_t mappedCueCount_ = 0;
    std::uint64_t queuedCueCount_ = 0;
    std::uint64_t suppressedCueCount_ = 0;
};

}  // namespace billiardgl::audio
