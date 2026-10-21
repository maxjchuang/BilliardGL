#include "audio/audio_event_mapper.h"
#include "audio/audio_mixer_model.h"
#include "automation_json.h"
#include "deterministic_hash.h"
#include "game_runtime.h"
#include "product_settings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using billiardgl::PhysicsContactKind;
using billiardgl::PhysicsContactRecord;
using billiardgl::PhysicsStepTelemetry;
using billiardgl::PocketBoundaryEventKind;
using billiardgl::RuntimeEvent;
using billiardgl::audio::AudioBus;
using billiardgl::audio::AudioCue;
using billiardgl::audio::AudioCueKind;
using billiardgl::audio::AudioDeliveryStatus;
using billiardgl::audio::AudioEventMapper;
using billiardgl::audio::AudioMixerModel;

void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

bool close(double first, double second, double epsilon = 1e-12)
{
    return std::fabs(first - second) <= epsilon;
}

const AudioCue* findCue(const std::vector<AudioCue>& cues, AudioCueKind kind)
{
    const auto found = std::find_if(cues.begin(), cues.end(),
        [kind](const AudioCue& cue) { return cue.kind == kind; });
    return found == cues.end() ? nullptr : &*found;
}

PhysicsContactRecord contact(PhysicsContactKind kind, int first, int second,
    double impulse, double time = 0.0)
{
    PhysicsContactRecord value;
    value.kind = kind;
    value.firstBall = first;
    value.secondBall = second;
    value.normalImpulseNs = impulse;
    value.velocityImpulseApplied = impulse > 0.0 && std::isfinite(impulse);
    value.timeOfImpactSeconds = time;
    return value;
}

bool sameCue(const AudioCue& first, const AudioCue& second)
{
    return first.tick == second.tick && first.kind == second.kind &&
        first.bus == second.bus && close(first.gain, second.gain) &&
        close(first.pitch, second.pitch) &&
        close(first.sourceImpulseNs, second.sourceImpulseNs) &&
        first.aggregateCount == second.aggregateCount &&
        first.primaryId == second.primaryId &&
        first.secondaryId == second.secondaryId &&
        first.sourceEventSequence == second.sourceEventSequence &&
        first.delivery == second.delivery;
}

void testMappingAndAggregation()
{
    PhysicsStepTelemetry telemetry;
    telemetry.contacts.push_back(contact(
        PhysicsContactKind::BallBall, 5, 2, 0.08, 0.001));
    telemetry.contacts.push_back(contact(
        PhysicsContactKind::BallBall, 2, 5, 0.20, 0.001));
    telemetry.contacts.push_back(contact(
        PhysicsContactKind::Rail, 4, -1, 0.10, 0.002));
    telemetry.contacts.push_back(contact(
        PhysicsContactKind::Rail, 7, -1, 0.16, 0.003));
    PhysicsContactRecord jaw = contact(
        PhysicsContactKind::Pocket, 7, -1, 0.0, 0.003);
    jaw.pocketId = 3;
    jaw.pocketBoundaryEvent = PocketBoundaryEventKind::LeftJaw;
    telemetry.contacts.push_back(jaw);
    PhysicsContactRecord drop = contact(
        PhysicsContactKind::Pocket, 9, -1, 0.0, 0.004);
    drop.pocketId = 1;
    drop.pocketBoundaryEvent = PocketBoundaryEventKind::Capture;
    telemetry.contacts.push_back(drop);

    const std::vector<RuntimeEvent> events{
        {7, 42, "ui_confirm"}, {3, 42, "cue_impact"},
        {8, 42, "foul"}, {9, 42, "ball_pocketed"},
        {1, 41, "ball_collision"}, {10, 42, "unknown_event"}};
    const AudioEventMapper mapper;
    const std::vector<AudioCue> cues = mapper.mapTick(42, telemetry, events);

    expect(cues.size() == 7, "all shipping cue categories map once per tick");
    const AudioCue* balls = findCue(cues, AudioCueKind::BallBall);
    const AudioCue* rail = findCue(cues, AudioCueKind::Rail);
    const AudioCue* pocketJaw = findCue(cues, AudioCueKind::PocketJaw);
    const AudioCue* pocketDrop = findCue(cues, AudioCueKind::PocketDrop);
    const AudioCue* cue = findCue(cues, AudioCueKind::CueImpact);
    const AudioCue* foul = findCue(cues, AudioCueKind::Foul);
    const AudioCue* ui = findCue(cues, AudioCueKind::UiFeedback);
    expect(balls != nullptr && balls->aggregateCount == 2 &&
        close(balls->sourceImpulseNs, 0.20) && balls->primaryId == 2 &&
        balls->secondaryId == 5,
        "ball contacts normalize identity and aggregate using strongest impulse");
    expect(rail != nullptr && rail->aggregateCount == 1 && rail->primaryId == 4,
        "jaw-matched rail fact is not emitted as a second rail sound");
    expect(pocketJaw != nullptr && close(pocketJaw->sourceImpulseNs, 0.16) &&
        pocketJaw->primaryId == 7 && pocketJaw->secondaryId == 3,
        "pocket jaw consumes the matching physical rail impulse");
    expect(pocketDrop != nullptr && pocketDrop->aggregateCount == 1,
        "physical pocket drop suppresses duplicate runtime fallback");
    expect(cue != nullptr && cue->bus == AudioBus::Effects && foul != nullptr &&
        foul->bus == AudioBus::Ui && ui != nullptr && ui->bus == AudioBus::Ui,
        "cue impact, foul, and UI feedback remain distinguishable and bus-routed");
    for (const AudioCue& item : cues) {
        expect(item.gain >= 0.0 && item.gain <= 1.0 &&
            item.pitch >= mapper.config().minimumContactPitch &&
            item.pitch <= mapper.config().maximumContactPitch &&
            item.delivery == AudioDeliveryStatus::CueOnly,
            "mapped parameters are bounded and never claim playback");
    }
}

void testDeterminismAndStormLimit()
{
    PhysicsStepTelemetry telemetry;
    for (int index = 0; index < 500; ++index) {
        telemetry.contacts.push_back(contact(PhysicsContactKind::BallBall,
            index % 16, (index + 1) % 16, 0.01 + index * 0.0001));
    }
    const std::vector<RuntimeEvent> events{{2, 77, "ball_collision"}};
    const AudioEventMapper mapper;
    const std::vector<AudioCue> first = mapper.mapTick(77, telemetry, events);
    std::reverse(telemetry.contacts.begin(), telemetry.contacts.end());
    const std::vector<AudioCue> second = mapper.mapTick(77, telemetry, events);
    expect(first.size() == 1 && second.size() == 1 &&
        first.front().aggregateCount == 500 &&
        sameCue(first.front(), second.front()),
        "same-tick contact storm collapses to one deterministic bounded cue");
}

void testExtremeAndNonfiniteInputs()
{
    PhysicsStepTelemetry telemetry;
    telemetry.contacts.push_back(contact(PhysicsContactKind::BallBall,
        0, 1, std::numeric_limits<double>::quiet_NaN()));
    telemetry.contacts.push_back(contact(PhysicsContactKind::Rail,
        2, -1, std::numeric_limits<double>::infinity()));
    telemetry.contacts.push_back(contact(PhysicsContactKind::BallBall,
        3, 4, -100.0));
    telemetry.contacts.push_back(contact(PhysicsContactKind::Rail,
        5, -1, std::numeric_limits<double>::max()));
    const AudioEventMapper mapper;
    const std::vector<AudioCue> cues = mapper.mapTick(3, telemetry, {});
    expect(cues.size() == 1 && cues.front().kind == AudioCueKind::Rail &&
        close(cues.front().gain, mapper.config().maximumContactGain) &&
        close(cues.front().pitch, mapper.config().maximumContactPitch),
        "nonfinite and negative impulses are ignored while finite extremes clamp");

    telemetry.stepStatus = billiardgl::PhysicsStepStatus::Failed;
    expect(mapper.mapTick(3, telemetry,
        {{1, 3, "ball_collision"}, {2, 3, "rail_collision"},
            {3, 3, "ball_pocketed"}}).empty(),
        "failed physics telemetry cannot publish contact audio or collision fallbacks");
}

void testMixerAndSettingsCompatibility()
{
    auto settings = billiardgl::settings::ProductSettings::defaults();
    settings.musicVolume = 0.2;
    settings.ambienceVolume = 0.3;
    settings.effectsVolume = 0.4;
    settings.uiVolume = 0.5;
    settings.ambienceMuted = true;
    settings.uiMuted = true;
    AudioMixerModel mixer(settings);
    std::vector<AudioCue> cues = mixer.persistentCues(91);
    AudioCue effect;
    effect.tick = 91;
    effect.kind = AudioCueKind::BallBall;
    effect.bus = AudioBus::Effects;
    effect.gain = 0.75;
    cues.push_back(effect);
    AudioCue ui = effect;
    ui.kind = AudioCueKind::UiFeedback;
    ui.bus = AudioBus::Ui;
    cues.push_back(ui);
    const auto first = mixer.mix(cues);
    expect(first.size() == 4 && close(first[0].outputGain, 0.2) &&
        first[1].suppressed && close(first[2].outputGain, 0.3) &&
        first[3].suppressed,
        "music, venue ambience, effects, and UI use independent buses and mutes");

    settings.effectsVolume = 0.1;
    settings.effectsMuted = true;
    settings.uiMuted = false;
    settings.uiVolume = 0.25;
    mixer.applySettings(settings);
    const auto second = mixer.mix(cues);
    expect(second[2].suppressed && close(second[2].outputGain, 0.0) &&
        !second[3].suppressed && close(second[3].outputGain, 0.1875),
        "changed settings apply on the immediately following mix call");
    for (const auto& item : second) {
        expect(item.delivery == AudioDeliveryStatus::CueOnly &&
            item.cue.delivery == AudioDeliveryStatus::CueOnly,
            "headless mixer returns cue data without pretending to play it");
    }

    billiardgl::settings::ProductSettings roundTripped;
    expect(billiardgl::settings::parse(
            billiardgl::settings::serialize(settings), roundTripped).ok &&
        close(roundTripped.musicVolume, settings.musicVolume) &&
        close(roundTripped.ambienceVolume, settings.ambienceVolume) &&
        close(roundTripped.effectsVolume, settings.effectsVolume) &&
        close(roundTripped.uiVolume, settings.uiVolume) &&
        roundTripped.musicMuted == settings.musicMuted &&
        roundTripped.ambienceMuted == settings.ambienceMuted &&
        roundTripped.effectsMuted == settings.effectsMuted &&
        roundTripped.uiMuted == settings.uiMuted,
        "all four bus volumes and mute flags round-trip through product settings");

    billiardgl::json::Value legacy =
        billiardgl::settings::serialize(
            billiardgl::settings::ProductSettings::defaults());
    auto& audio = legacy["audio"].asObject();
    audio.erase("ui");
    audio.erase("music_muted");
    audio.erase("ambience_muted");
    audio.erase("effects_muted");
    audio.erase("ui_muted");
    billiardgl::settings::ProductSettings parsed;
    expect(billiardgl::settings::parse(legacy, parsed).ok &&
        close(parsed.uiVolume, 1.0) && !parsed.musicMuted &&
        !parsed.ambienceMuted && !parsed.effectsMuted && !parsed.uiMuted,
        "older v1 settings load with safe defaults for new audio controls");

    auto invalid = settings;
    invalid.uiVolume = std::numeric_limits<double>::quiet_NaN();
    expect(billiardgl::settings::validate(invalid).errorCode == "E_AUDIO_VOLUME",
        "product settings reject nonfinite UI bus volume");
    mixer.applySettings(invalid);
    expect(close(mixer.bus(AudioBus::Ui).volume, 0.0),
        "mixer defensively silences an unvalidated nonfinite bus volume");
}

void testAuthorityIsolation()
{
    billiardgl::GameRuntime runtime;
    const std::string beforeHash = billiardgl::gameStateSha256(runtime.state());
    const std::uint64_t beforeTick = runtime.tick();
    const std::size_t beforeEvents = runtime.events().size();
    PhysicsStepTelemetry telemetry;
    telemetry.contacts.push_back(contact(
        PhysicsContactKind::BallBall, 0, 1, 0.2));
    const AudioEventMapper mapper;
    const std::vector<AudioCue> cues = mapper.mapTick(
        runtime.tick(), telemetry, runtime.events());
    AudioMixerModel mixer;
    const auto mixed = mixer.mix(cues);
    expect(!mixed.empty() && beforeHash ==
            billiardgl::gameStateSha256(runtime.state()) &&
        beforeTick == runtime.tick() && beforeEvents == runtime.events().size(),
        "read-only audio mapping and mixing preserve authority hash, tick, and events");
}

}  // namespace

int main()
{
    try {
        testMappingAndAggregation();
        testDeterminismAndStormLimit();
        testExtremeAndNonfiniteInputs();
        testMixerAndSettingsCompatibility();
        testAuthorityIsolation();
        std::cout << "Product audio mapping and headless mixer checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
