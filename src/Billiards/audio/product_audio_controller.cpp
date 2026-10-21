#include "audio/product_audio_controller.h"

#include <algorithm>

namespace billiardgl::audio {
namespace {

const AudioBus kBuses[] = {AudioBus::Music, AudioBus::Ambience,
    AudioBus::Effects, AudioBus::Ui};

void merge(AudioConsumeResult& target, const AudioConsumeResult& source)
{
    if (!source.ok && target.ok) {
        target.ok = false;
        target.code = source.code;
    }
    target.queuedCount += source.queuedCount;
    target.suppressedCount += source.suppressedCount;
    target.degradedAliasCount += source.degradedAliasCount;
    target.ignoredPitchCount += source.ignoredPitchCount;
}

}  // namespace

ProductAudioController::ProductAudioController(AudioCueConsumer& consumer,
    const settings::ProductSettings& settings, AudioMapperConfig mapperConfig)
    : consumer_(consumer), mapper_(mapperConfig), mixer_(settings)
{
    (void)applySettings(settings);
}

ProductAudioResult ProductAudioController::applySettings(
    const settings::ProductSettings& settings)
{
    mixer_.applySettings(settings);
    ProductAudioResult result;
    for (AudioBus bus : kBuses)
        merge(result.consumer, consumer_.updateBus(bus, mixer_.bus(bus)));
    if (persistentLoopsEnabled_) {
        const auto mixed = mixer_.mix(mixer_.persistentCues(lastProcessedTick_));
        merge(result.consumer, consumer_.consume(mixed));
        result.mixedCueCount = mixed.size();
        queuedCueCount_ += result.consumer.queuedCount;
        suppressedCueCount_ += result.consumer.suppressedCount;
    }
    result.ok = result.consumer.ok;
    result.errorCode = result.consumer.code;
    return result;
}

void ProductAudioController::beginGeneration(std::uint64_t generation)
{
    hasGeneration_ = true;
    generation_ = generation;
    hasProcessedTick_ = false;
    lastProcessedTick_ = 0;
    lastEventSequence_ = 0;
}

ProductAudioResult ProductAudioController::startPersistentLoops(
    const settings::ProductSettings& settings)
{
    persistentLoopsEnabled_ = true;
    return applySettings(settings);
}

ProductAudioResult ProductAudioController::consumeStep(
    const ProductAudioStep& step,
    const settings::ProductSettings& settings)
{
    ProductAudioResult result = applySettings(settings);
    if (!hasGeneration_)
        beginGeneration(step.generation);
    else if (step.generation < generation_) {
        ++duplicateTickCount_;
        result.duplicateOrOutOfOrder = true;
        return result;
    } else if (step.generation > generation_) {
        beginGeneration(step.generation);
    }
    if (hasProcessedTick_ && step.tick <= lastProcessedTick_) {
        ++duplicateTickCount_;
        result.duplicateOrOutOfOrder = true;
        return result;
    }

    std::vector<RuntimeEvent> freshEvents;
    std::uint64_t newestConsumedSequence = lastEventSequence_;
    for (const RuntimeEvent& event : step.runtimeEvents) {
        if (event.sequence <= lastEventSequence_ || event.tick > step.tick)
            continue;
        newestConsumedSequence = std::max(newestConsumedSequence, event.sequence);
        if (event.tick == step.tick) freshEvents.push_back(event);
    }
    const std::vector<AudioCue> cues = mapper_.mapTick(
        step.tick, step.telemetry, freshEvents);
    const std::vector<MixedAudioCue> mixed = mixer_.mix(cues);
    result.mappedCueCount = cues.size();
    result.mixedCueCount = mixed.size();
    if (!mixed.empty()) {
        const AudioConsumeResult consumed = consumer_.consume(mixed);
        queuedCueCount_ += consumed.queuedCount;
        suppressedCueCount_ += consumed.suppressedCount;
        merge(result.consumer, consumed);
        result.ok = result.consumer.ok;
        if (!result.ok && result.errorCode.empty())
            result.errorCode = result.consumer.code;
    }

    hasProcessedTick_ = true;
    lastProcessedTick_ = step.tick;
    lastEventSequence_ = newestConsumedSequence;
    ++processedTickCount_;
    mappedCueCount_ += cues.size();
    return result;
}

std::vector<ProductAudioResult> ProductAudioController::consumeSteps(
    const std::vector<ProductAudioStep>& steps,
    const settings::ProductSettings& settings)
{
    std::vector<ProductAudioResult> results;
    results.reserve(steps.size());
    for (const ProductAudioStep& step : steps)
        results.push_back(consumeStep(step, settings));
    return results;
}

ProductAudioSnapshot ProductAudioController::snapshot() const
{
    ProductAudioSnapshot result;
    result.hasGeneration = hasGeneration_;
    result.generation = generation_;
    result.hasProcessedTick = hasProcessedTick_;
    result.lastProcessedTick = lastProcessedTick_;
    result.lastEventSequence = lastEventSequence_;
    result.processedTickCount = processedTickCount_;
    result.duplicateTickCount = duplicateTickCount_;
    result.mappedCueCount = mappedCueCount_;
    result.queuedCueCount = queuedCueCount_;
    result.suppressedCueCount = suppressedCueCount_;
    result.consumer = consumer_.status();
    return result;
}

}  // namespace billiardgl::audio
