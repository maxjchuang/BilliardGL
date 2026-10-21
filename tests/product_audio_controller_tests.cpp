#include "audio/audio_asset_policy.h"
#include "audio/product_audio_controller.h"
#include "deterministic_hash.h"
#include "game_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::audio;

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

bool close(double first, double second, double epsilon = 1e-12)
{
    return std::fabs(first - second) <= epsilon;
}

struct BusUpdate {
    AudioBus bus = AudioBus::Effects;
    AudioBusState state;
};

class FakeAudioConsumer final : public AudioCueConsumer {
public:
    AudioConsumeResult updateBus(
        AudioBus bus, const AudioBusState& state) override
    {
        busUpdates.push_back({bus, state});
        return {};
    }

    AudioConsumeResult consume(
        const std::vector<MixedAudioCue>& cues) override
    {
        batches.push_back(cues);
        if (failState == AudioConsumerState::DeviceFailed ||
            failState == AudioConsumerState::AssetFailed) {
            status_.state = failState;
            status_.code = failCode;
            return {false, failCode, 0, 0, 0, 0};
        }
        AudioConsumeResult result;
        for (const MixedAudioCue& cue : cues) {
            if (cue.suppressed) ++result.suppressedCount;
            else ++result.queuedCount;
        }
        status_.state = AudioConsumerState::Ready;
        status_.queuedCueCount += result.queuedCount;
        status_.suppressedCueCount += result.suppressedCount;
        return result;
    }

    AudioConsumerStatus status() const override { return status_; }

    std::vector<BusUpdate> busUpdates;
    std::vector<std::vector<MixedAudioCue>> batches;
    AudioConsumerState failState = AudioConsumerState::Dormant;
    std::string failCode;

private:
    AudioConsumerStatus status_;
};

PhysicsContactRecord contact(PhysicsContactKind kind, double impulse)
{
    PhysicsContactRecord value;
    value.kind = kind;
    value.firstBall = 0;
    value.secondBall = kind == PhysicsContactKind::BallBall ? 1 : -1;
    value.normalImpulseNs = impulse;
    value.velocityImpulseApplied = true;
    return value;
}

ProductAudioStep step(std::uint64_t generation, std::uint64_t tick,
    PhysicsStepStatus status, std::vector<RuntimeEvent> events,
    PhysicsContactKind kind = PhysicsContactKind::BallBall)
{
    ProductAudioStep value;
    value.generation = generation;
    value.tick = tick;
    value.telemetry.stepStatus = status;
    value.telemetry.contacts.push_back(contact(kind, 0.2));
    value.runtimeEvents = std::move(events);
    return value;
}

const MixedAudioCue* mixedKind(const std::vector<MixedAudioCue>& cues,
    AudioCueKind kind)
{
    const auto found = std::find_if(cues.begin(), cues.end(),
        [kind](const MixedAudioCue& cue) { return cue.cue.kind == kind; });
    return found == cues.end() ? nullptr : &*found;
}

void testTimelineDedupResetAndMultiStep()
{
    FakeAudioConsumer consumer;
    auto settings = settings::ProductSettings::defaults();
    ProductAudioController controller(consumer, settings);
    GameRuntime authority;
    const std::string authorityHash = gameStateSha256(authority.state());

    const ProductAudioStep first = step(7, 1,
        PhysicsStepStatus::Succeeded, {{1, 1, "cue_impact"}});
    const ProductAudioResult delivered = controller.consumeStep(first, settings);
    expect(delivered.ok && delivered.mappedCueCount == 2 &&
        delivered.consumer.queuedCount == 2 && consumer.batches.size() == 1,
        "one tick maps, mixes, and consumes contact plus runtime cue once");
    const ProductAudioResult duplicate = controller.consumeStep(first, settings);
    expect(duplicate.ok && duplicate.duplicateOrOutOfOrder &&
        consumer.batches.size() == 1,
        "same generation and tick cannot replay already consumed cues");

    const ProductAudioStep failed = step(7, 2, PhysicsStepStatus::Failed,
        {{1, 1, "cue_impact"}, {2, 2, "rail_collision"}},
        PhysicsContactKind::Rail);
    const ProductAudioResult failedResult =
        controller.consumeStep(failed, settings);
    expect(failedResult.ok && failedResult.mappedCueCount == 0 &&
        consumer.batches.size() == 1,
        "failed step publishes no contact fallback and never replays prior tick");
    ProductAudioStep repaired = failed;
    repaired.telemetry.stepStatus = PhysicsStepStatus::Succeeded;
    expect(controller.consumeStep(repaired, settings).duplicateOrOutOfOrder &&
        consumer.batches.size() == 1,
        "retrying a consumed failed numeric tick remains idempotent");

    const std::vector<RuntimeEvent> retained{
        {1, 1, "cue_impact"}, {2, 2, "rail_collision"},
        {3, 3, "ui_confirm"}, {4, 4, "rules_foul"}};
    const std::vector<ProductAudioResult> batch = controller.consumeSteps({
        step(7, 3, PhysicsStepStatus::Succeeded, retained,
            PhysicsContactKind::Rail),
        step(7, 4, PhysicsStepStatus::Succeeded, retained,
            PhysicsContactKind::BallBall)}, settings);
    expect(batch.size() == 2 && batch[0].mappedCueCount == 2 &&
        batch[1].mappedCueCount == 2 && consumer.batches.size() == 3,
        "multi-step batch consumes each tick and its new event sequence once");
    const std::size_t afterBatch = consumer.batches.size();
    const auto repeatedBatch = controller.consumeSteps({
        step(7, 3, PhysicsStepStatus::Succeeded, retained),
        step(7, 4, PhysicsStepStatus::Succeeded, retained)}, settings);
    expect(repeatedBatch[0].duplicateOrOutOfOrder &&
        repeatedBatch[1].duplicateOrOutOfOrder &&
        consumer.batches.size() == afterBatch,
        "resubmitting a multi-step batch cannot replay any tick");

    const ProductAudioResult reset = controller.consumeStep(step(8, 1,
        PhysicsStepStatus::Succeeded, {{1, 1, "rules_game_over"}}), settings);
    const ProductAudioSnapshot snapshot = controller.snapshot();
    expect(reset.ok && reset.mappedCueCount == 2 &&
        mixedKind(consumer.batches.back(), AudioCueKind::MatchFinish) != nullptr &&
        snapshot.generation == 8 && snapshot.lastProcessedTick == 1 &&
        snapshot.lastEventSequence == 1 &&
        gameStateSha256(authority.state()) == authorityHash,
        "new generation safely reuses tick and sequence without authority mutation");
    const std::size_t afterReset = consumer.batches.size();
    expect(controller.consumeStep(first, settings).duplicateOrOutOfOrder &&
        consumer.batches.size() == afterReset,
        "a delayed older generation cannot reset cursors or replay audio");
}

void testImmediateBusVolumeAndMute()
{
    FakeAudioConsumer consumer;
    auto settings = settings::ProductSettings::defaults();
    settings.effectsVolume = 0.4;
    settings.uiVolume = 0.7;
    settings.uiMuted = true;
    ProductAudioController controller(consumer, settings);
    const ProductAudioResult first = controller.consumeStep(step(1, 1,
        PhysicsStepStatus::Succeeded, {{1, 1, "ui_confirm"}}), settings);
    const MixedAudioCue* effect = mixedKind(
        consumer.batches.back(), AudioCueKind::BallBall);
    const MixedAudioCue* ui = mixedKind(
        consumer.batches.back(), AudioCueKind::UiFeedback);
    expect(first.ok && effect != nullptr && ui != nullptr &&
        !effect->suppressed && close(effect->outputGain,
            effect->cue.gain * 0.4) && ui->suppressed,
        "effects volume and UI mute are applied before the same tick is consumed");

    settings.effectsMuted = true;
    settings.uiMuted = false;
    settings.uiVolume = 0.25;
    const ProductAudioResult second = controller.consumeStep(step(1, 2,
        PhysicsStepStatus::Succeeded,
        {{1, 1, "ui_confirm"}, {2, 2, "ui_cancel"}}), settings);
    effect = mixedKind(consumer.batches.back(), AudioCueKind::BallBall);
    ui = mixedKind(consumer.batches.back(), AudioCueKind::UiFeedback);
    expect(second.ok && effect->suppressed && !ui->suppressed &&
        close(ui->outputGain, ui->cue.gain * 0.25) &&
        consumer.busUpdates.size() >= 12,
        "changed bus volume and mute state take effect on the immediate next tick");
}

void testConsumerErrorsAreExplicitAndNonThrowing()
{
    auto settings = settings::ProductSettings::defaults();
    FakeAudioConsumer deviceFailure;
    deviceFailure.failState = AudioConsumerState::DeviceFailed;
    deviceFailure.failCode = "audio_device_open_failed";
    ProductAudioController deviceController(deviceFailure, settings);
    const ProductAudioResult device = deviceController.consumeStep(step(1, 1,
        PhysicsStepStatus::Succeeded, {}), settings);
    expect(!device.ok && device.errorCode == "audio_device_open_failed" &&
        deviceController.snapshot().consumer.state ==
            AudioConsumerState::DeviceFailed,
        "device failure is explicit product status without an exception");
    const std::size_t failedAttempts = deviceFailure.batches.size();
    expect(deviceController.consumeStep(step(1, 1,
            PhysicsStepStatus::Succeeded, {}), settings)
            .duplicateOrOutOfOrder &&
        deviceFailure.batches.size() == failedAttempts,
        "consumer failure still advances dedup cursor and cannot replay a tick");

    FakeAudioConsumer assetFailure;
    assetFailure.failState = AudioConsumerState::AssetFailed;
    assetFailure.failCode = "audio_asset_load_failed:hit.wav";
    ProductAudioController assetController(assetFailure, settings);
    const ProductAudioResult asset = assetController.consumeStep(step(1, 1,
        PhysicsStepStatus::Succeeded, {}), settings);
    expect(!asset.ok && asset.errorCode == assetFailure.failCode &&
        assetController.snapshot().processedTickCount == 1,
        "asset failure remains observable while the product timeline progresses");
}

void testFourWavAssetPolicyIsHonest()
{
    for (AudioCueKind kind : {AudioCueKind::CueImpact,
            AudioCueKind::BallBall, AudioCueKind::Rail,
            AudioCueKind::PocketJaw}) {
        const AudioAssetBinding binding = resolveAudioAsset(kind);
        expect(std::string(binding.fileName) == "hit.wav" &&
            binding.status == AudioAssetStatus::DegradedAssetAlias &&
            !binding.pitchSupported,
            "cue, ball, rail, and jaw share hit.wav with degraded status");
    }
    const AudioAssetBinding drop =
        resolveAudioAsset(AudioCueKind::PocketDrop);
    expect(std::string(drop.fileName) == "ballin.wav" &&
        drop.status == AudioAssetStatus::DistinctAsset &&
        !drop.pitchSupported,
        "pocket drop is the only distinct transient asset");
    for (AudioCueKind kind : {AudioCueKind::UiFeedback,
            AudioCueKind::Foul, AudioCueKind::MatchFinish}) {
        const AudioAssetBinding binding = resolveAudioAsset(kind);
        expect(std::string(binding.fileName) == "GameOver.wav" &&
            binding.status == AudioAssetStatus::DegradedAssetAlias &&
            !binding.pitchSupported,
            "UI, foul, and finish share GameOver.wav with degraded status");
    }
    expect(std::string(audioAssetStatusName(
            AudioAssetStatus::DegradedAssetAlias)) ==
            "degraded_asset_alias",
        "degraded alias status uses the required stable identifier");
}

}  // namespace

int main()
{
    try {
        testTimelineDedupResetAndMultiStep();
        testImmediateBusVolumeAndMute();
        testConsumerErrorsAreExplicitAndNonThrowing();
        testFourWavAssetPolicyIsHonest();
        std::cout << "Product audio controller and fake consumer checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
