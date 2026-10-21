#include "deterministic_hash.h"
#include "render/modern/modern_product_session.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl;
using namespace billiardgl::audio;
using namespace billiardgl::render;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

struct BusUpdate {
    AudioBus bus = AudioBus::Effects;
    AudioBusState state;
};

class FakeConsumer final : public AudioCueConsumer {
public:
    AudioConsumeResult updateBus(
        AudioBus bus, const AudioBusState& state) override
    {
        buses.push_back({bus, state});
        return {};
    }

    AudioConsumeResult consume(
        const std::vector<MixedAudioCue>& cues) override
    {
        batches.push_back(cues);
        if (fail) {
            status_.state = AudioConsumerState::DeviceFailed;
            status_.code = "audio_test_device_failed";
            return {false, status_.code, 0, 0, 0, 0};
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

    AudioBusState latest(AudioBus bus) const
    {
        const auto found = std::find_if(buses.rbegin(), buses.rend(),
            [bus](const BusUpdate& update) { return update.bus == bus; });
        return found == buses.rend() ? AudioBusState{} : found->state;
    }

    std::vector<BusUpdate> buses;
    std::vector<std::vector<MixedAudioCue>> batches;
    bool fail = false;

private:
    AudioConsumerStatus status_;
};

PlatformInputEvent key(const std::string& control)
{
    PlatformInputEvent event;
    event.kind = PlatformInputKind::KeyDown;
    event.control = control;
    return event;
}

void configureCollision(GameRuntime& runtime)
{
    BallState cue = runtime.state().balls[0];
    cue.pocketed = false;
    cue.position.x = -5.0f;
    cue.position.z = 0.0f;
    cue.velocity.x = 20.0f;
    cue.velocity.y = 0.0f;
    cue.velocity.z = 0.0f;
    BallState object = runtime.state().balls[1];
    object.pocketed = false;
    object.position.x = 5.0f;
    object.position.z = 0.0f;
    object.velocity = Point3{};
    expect(runtime.setBall(0, cue).ok && runtime.setBall(1, object).ok,
        "collision fixture must configure authority through public APIs");
}

bool stepUntilAudio(GameRuntime& runtime, ModernProductSession& session,
    FakeConsumer& consumer, int maximumTicks = 20)
{
    const std::size_t before = consumer.batches.size();
    for (int tick = 0; tick < maximumTicks; ++tick) {
        expect(runtime.step(1).ok, "collision fixture physics step failed");
        const std::string authorityHash = gameStateSha256(runtime.state());
        session.consumeAuthoritySteps();
        expect(gameStateSha256(runtime.state()) == authorityHash,
            "audio consumption must not mutate authority state");
        if (consumer.batches.size() > before) return true;
    }
    return false;
}

void testRuntimeTelemetryAndGeneration()
{
    GameRuntime runtime;
    const std::uint64_t initialGeneration = runtime.eventJournalGeneration();
    expect(runtime.step(2).ok && runtime.recentPhysicsSteps().size() == 2 &&
        runtime.latestPhysicsStep().tick == 2 &&
        runtime.latestPhysicsStep().telemetry.stepStatus ==
            PhysicsStepStatus::Succeeded,
        "runtime exposes successful per-tick physics telemetry without tracing");
    runtime.reset();
    expect(runtime.eventJournalGeneration() == initialGeneration + 1 &&
        runtime.tick() == 0 && runtime.events().empty() &&
        runtime.recentPhysicsSteps().empty(),
        "reset increments event generation before tick/sequence reuse");

    GameState scenario = runtime.state();
    const std::uint64_t resetGeneration = runtime.eventJournalGeneration();
    expect(runtime.replaceStateForScenario(scenario).ok &&
        runtime.eventJournalGeneration() == resetGeneration + 1,
        "scenario replacement increments event generation before reuse");

    GameRuntime failed;
    failed.mutableState().balls[0].position.x =
        std::numeric_limits<float>::quiet_NaN();
    expect(!failed.step(1).ok && failed.tick() == 0 &&
        failed.hasRecentPhysicsStep() &&
        failed.latestPhysicsStep().tick == 1 &&
        failed.latestPhysicsStep().telemetry.stepStatus ==
            PhysicsStepStatus::Failed,
        "failed step telemetry is retained while the authority tick stays put");
}

void testSessionAudioAuthorityConsumption()
{
    const auto nonce = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    ModernProductSessionOptions options;
    options.settingsPath = (std::filesystem::temp_directory_path() /
        ("billiardgl-audio-session-" + std::to_string(nonce) + ".json"))
        .string();
    GameRuntime runtime;
    ai::AiPlayerController ai;
    FakeConsumer consumer;
    ModernProductSession session(runtime, ai, options, &consumer);
    expect(session.snapshot().audioEnabled &&
        !session.snapshot().audio.consumer.deviceInitialized,
        "injected consumer starts without opening a platform device");

    expect(session.handlePlatformInput(key("F10")).ok,
        "settings opens through normalized product input");
    for (int category = 0; category < 3; ++category)
        expect(session.handlePlatformInput(key("ArrowRight")).ok,
            "audio category navigation succeeds");
    expect(session.handlePlatformInput(key("-")).ok &&
        std::fabs(consumer.latest(AudioBus::Music).volume - 0.65) < 1e-12,
        "previewed audio volume reaches the consumer immediately without a tick");
    for (int item = 0; item < 4; ++item)
        expect(session.handlePlatformInput(key("ArrowDown")).ok,
            "audio mute focus navigation succeeds");
    expect(session.handlePlatformInput(key("Space")).ok &&
        consumer.latest(AudioBus::Music).muted,
        "previewed mute reaches the consumer immediately");
    expect(session.handlePlatformInput(key("Escape")).ok &&
        std::fabs(consumer.latest(AudioBus::Music).volume - 0.7) < 1e-12 &&
        !consumer.latest(AudioBus::Music).muted,
        "settings cancel immediately restores saved bus state");

    const std::uint64_t firstGeneration = runtime.eventJournalGeneration();
    configureCollision(runtime);
    expect(stepUntilAudio(runtime, session, consumer),
        "real runtime collision telemetry reaches the fake consumer");
    expect(!runtime.events().empty() &&
        std::any_of(runtime.events().begin(), runtime.events().end(),
            [](const RuntimeEvent& event) {
                return event.name == "ball_collision";
            }) &&
        std::any_of(consumer.batches.back().begin(),
            consumer.batches.back().end(), [](const MixedAudioCue& cue) {
                return cue.cue.kind == AudioCueKind::BallBall &&
                    cue.cue.aggregateCount >= 1;
            }),
        "runtime event journal and successful contact telemetry map to one cue batch");
    const auto afterFirst = session.snapshot();
    const std::size_t firstBatchCount = consumer.batches.size();
    session.consumeAuthoritySteps();
    expect(consumer.batches.size() == firstBatchCount &&
        session.snapshot().audio.processedTickCount ==
            afterFirst.audio.processedTickCount,
        "re-pumping the same authority tick cannot replay a cue");

    const std::uint64_t beforeMulti =
        session.snapshot().audioConsumedAuthorityTicks;
    expect(runtime.step(3).ok, "multi-step authority update succeeds");
    session.consumeAuthoritySteps();
    expect(session.snapshot().audioConsumedAuthorityTicks == beforeMulti + 3,
        "one pump consumes every retained successful tick from a multi-step call");
    const std::uint64_t processedAfterMulti =
        session.snapshot().audio.processedTickCount;
    session.consumeAuthoritySteps();
    expect(session.snapshot().audio.processedTickCount == processedAfterMulti,
        "multi-step records are never consumed twice");

    runtime.reset();
    expect(runtime.eventJournalGeneration() == firstGeneration + 1,
        "reset advances the explicit audio/event generation");
    configureCollision(runtime);
    expect(stepUntilAudio(runtime, session, consumer) &&
        session.snapshot().audio.generation == runtime.eventJournalGeneration(),
        "new generation safely reuses numeric ticks without suppressing new cues");

    GameRuntime pristine;
    const std::uint64_t beforeScenario = runtime.eventJournalGeneration();
    expect(runtime.replaceStateForScenario(pristine.state()).ok &&
        runtime.eventJournalGeneration() == beforeScenario + 1,
        "new scenario advances generation before tick/event sequence reuse");
    configureCollision(runtime);
    consumer.fail = true;
    const std::string hashBeforeFailurePump = gameStateSha256(runtime.state());
    expect(stepUntilAudio(runtime, session, consumer),
        "failing consumer is still invoked by real telemetry");
    const auto failure = session.snapshot();
    expect(!failure.audioError.empty() && failure.audioFailureCount > 0 &&
        failure.audio.consumer.state == AudioConsumerState::DeviceFailed &&
        gameStateSha256(runtime.state()) != hashBeforeFailurePump,
        "device failure is app-visible while physics continues normally");
    const std::string hashAfterStep = gameStateSha256(runtime.state());
    session.consumeAuthoritySteps();
    expect(gameStateSha256(runtime.state()) == hashAfterStep,
        "failed audio delivery cannot alter or replay authority state");
}

}  // namespace

int main()
{
    try {
        testRuntimeTelemetryAndGeneration();
        testSessionAudioAuthorityConsumption();
        std::cout << "modern-product-audio-session-e2e: pass\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
