#include "audio/audio_event_mapper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <tuple>

namespace billiardgl::audio {
namespace {

constexpr std::size_t kMappedKindCount = 8;

struct Aggregate {
    bool present = false;
    double strongestImpulseNs = 0.0;
    std::size_t count = 0;
    int primaryId = -1;
    int secondaryId = -1;
    std::uint64_t sequence = 0;
};

std::size_t mappedIndex(AudioCueKind kind)
{
    return static_cast<std::size_t>(kind);
}

double finiteOr(double value, double fallback)
{
    return std::isfinite(value) ? value : fallback;
}

AudioMapperConfig sanitized(AudioMapperConfig config)
{
    config.minimumAudibleImpulseNs = std::max(
        0.0, finiteOr(config.minimumAudibleImpulseNs, 0.002));
    config.maximumMappedImpulseNs = std::max(
        config.minimumAudibleImpulseNs + 0.000001,
        finiteOr(config.maximumMappedImpulseNs, 0.35));
    config.minimumContactGain = std::clamp(
        finiteOr(config.minimumContactGain, 0.12), 0.0, 1.0);
    config.maximumContactGain = std::clamp(
        finiteOr(config.maximumContactGain, 1.0),
        config.minimumContactGain, 1.0);
    config.minimumContactPitch = std::clamp(
        finiteOr(config.minimumContactPitch, 0.88), 0.25, 4.0);
    config.maximumContactPitch = std::clamp(
        finiteOr(config.maximumContactPitch, 1.12),
        config.minimumContactPitch, 4.0);
    config.fallbackEventImpulseNs = std::clamp(
        finiteOr(config.fallbackEventImpulseNs, 0.04),
        config.minimumAudibleImpulseNs, config.maximumMappedImpulseNs);
    config.maximumCuesPerTick = std::max<std::size_t>(
        1, std::min<std::size_t>(config.maximumCuesPerTick, kMappedKindCount));
    return config;
}

bool validImpulse(double impulse)
{
    return std::isfinite(impulse) && impulse >= 0.0;
}

std::int64_t contactTimeKey(double seconds)
{
    if (!std::isfinite(seconds)) return std::numeric_limits<std::int64_t>::min();
    constexpr double kLimit =
        static_cast<double>(std::numeric_limits<std::int64_t>::max()) /
        1000000000.0;
    if (seconds > kLimit || seconds < -kLimit)
        return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(std::llround(seconds * 1000000000.0));
}

bool isJaw(PocketBoundaryEventKind kind)
{
    return kind == PocketBoundaryEventKind::LeftJaw ||
        kind == PocketBoundaryEventKind::RightJaw;
}

void addAggregate(std::array<Aggregate, kMappedKindCount>& values,
    AudioCueKind kind, double impulse, int primaryId, int secondaryId,
    std::uint64_t sequence = 0)
{
    Aggregate& aggregate = values[mappedIndex(kind)];
    aggregate.present = true;
    ++aggregate.count;
    const auto candidateIds = std::make_tuple(primaryId, secondaryId);
    const auto currentIds = std::make_tuple(
        aggregate.primaryId, aggregate.secondaryId);
    if (aggregate.count == 1 || impulse > aggregate.strongestImpulseNs ||
        (impulse == aggregate.strongestImpulseNs && candidateIds < currentIds)) {
        aggregate.strongestImpulseNs = impulse;
        aggregate.primaryId = primaryId;
        aggregate.secondaryId = secondaryId;
        aggregate.sequence = sequence;
    } else if (sequence != 0 &&
        (aggregate.sequence == 0 || sequence < aggregate.sequence)) {
        aggregate.sequence = sequence;
    }
}

AudioCueKind runtimeKind(const std::string& name, bool& known)
{
    known = true;
    if (name == "ball_collision") return AudioCueKind::BallBall;
    if (name == "rail_collision") return AudioCueKind::Rail;
    if (name == "ball_pocketed" || name == "cue_ball_pocketed" ||
        name == "eight_ball_pocketed") return AudioCueKind::PocketDrop;
    if (name == "cue_impact" || name == "shot_started" ||
        name == "shot_taken") return AudioCueKind::CueImpact;
    if (name == "foul" || name == "rules_foul") return AudioCueKind::Foul;
    if (name == "game_over" || name == "match_finished" ||
        name == "match_complete" || name == "rules_game_over")
        return AudioCueKind::MatchFinish;
    if (name.rfind("ui_", 0) == 0) return AudioCueKind::UiFeedback;
    known = false;
    return AudioCueKind::UiFeedback;
}

AudioBus busFor(AudioCueKind kind)
{
    if (kind == AudioCueKind::Foul || kind == AudioCueKind::UiFeedback ||
        kind == AudioCueKind::MatchFinish)
        return AudioBus::Ui;
    return AudioBus::Effects;
}

double jawImpulse(const PhysicsStepTelemetry& telemetry,
    const PhysicsContactRecord& jaw)
{
    double strongest = 0.0;
    const std::int64_t wantedTime = contactTimeKey(jaw.timeOfImpactSeconds);
    for (const PhysicsContactRecord& candidate : telemetry.contacts) {
        if (candidate.kind != PhysicsContactKind::Rail ||
            candidate.firstBall != jaw.firstBall ||
            contactTimeKey(candidate.timeOfImpactSeconds) != wantedTime ||
            !validImpulse(candidate.normalImpulseNs)) continue;
        strongest = std::max(strongest, candidate.normalImpulseNs);
    }
    return strongest;
}

bool isRailConsumedByJaw(const PhysicsStepTelemetry& telemetry,
    const PhysicsContactRecord& rail)
{
    const std::int64_t wantedTime = contactTimeKey(rail.timeOfImpactSeconds);
    for (const PhysicsContactRecord& candidate : telemetry.contacts) {
        if (candidate.kind == PhysicsContactKind::Pocket &&
            isJaw(candidate.pocketBoundaryEvent) &&
            candidate.firstBall == rail.firstBall &&
            contactTimeKey(candidate.timeOfImpactSeconds) == wantedTime)
            return true;
    }
    return false;
}

}  // namespace

AudioEventMapper::AudioEventMapper(AudioMapperConfig config)
    : config_(sanitized(config))
{
}

std::vector<AudioCue> AudioEventMapper::mapTick(std::uint64_t tick,
    const PhysicsStepTelemetry& telemetry,
    const std::vector<RuntimeEvent>& events) const
{
    std::array<Aggregate, kMappedKindCount> aggregates{};
    if (telemetry.stepStatus == PhysicsStepStatus::Succeeded) {
        for (const PhysicsContactRecord& contact : telemetry.contacts) {
            if (contact.kind == PhysicsContactKind::Pocket &&
                isJaw(contact.pocketBoundaryEvent)) {
                const double impulse = jawImpulse(telemetry, contact);
                addAggregate(aggregates, AudioCueKind::PocketJaw,
                    validImpulse(impulse) ? impulse : 0.0,
                    contact.firstBall, contact.pocketId);
                continue;
            }
            if (contact.kind == PhysicsContactKind::Pocket &&
                contact.pocketBoundaryEvent == PocketBoundaryEventKind::Capture) {
                addAggregate(aggregates, AudioCueKind::PocketDrop,
                    config_.fallbackEventImpulseNs,
                    contact.firstBall, contact.pocketId);
                continue;
            }
            if (!validImpulse(contact.normalImpulseNs) ||
                contact.normalImpulseNs < config_.minimumAudibleImpulseNs)
                continue;
            if (contact.kind == PhysicsContactKind::BallBall) {
                const int first = std::min(contact.firstBall, contact.secondBall);
                const int second = std::max(contact.firstBall, contact.secondBall);
                addAggregate(aggregates, AudioCueKind::BallBall,
                    contact.normalImpulseNs, first, second);
            } else if (contact.kind == PhysicsContactKind::Rail &&
                !isRailConsumedByJaw(telemetry, contact)) {
                addAggregate(aggregates, AudioCueKind::Rail,
                    contact.normalImpulseNs, contact.firstBall, -1);
            }
        }
    }

    std::vector<RuntimeEvent> sortedEvents;
    sortedEvents.reserve(events.size());
    for (const RuntimeEvent& event : events) {
        if (event.tick == tick) sortedEvents.push_back(event);
    }
    std::sort(sortedEvents.begin(), sortedEvents.end(),
        [](const RuntimeEvent& first, const RuntimeEvent& second) {
            if (first.sequence != second.sequence)
                return first.sequence < second.sequence;
            return first.name < second.name;
        });
    for (const RuntimeEvent& event : sortedEvents) {
        bool known = false;
        const AudioCueKind kind = runtimeKind(event.name, known);
        if (!known) continue;
        if (telemetry.stepStatus == PhysicsStepStatus::Failed &&
            (kind == AudioCueKind::BallBall || kind == AudioCueKind::Rail ||
                kind == AudioCueKind::PocketDrop))
            continue;
        Aggregate& aggregate = aggregates[mappedIndex(kind)];
        // Physics facts have better identity and magnitude. Runtime events are
        // a fallback, not a second sound for the same fact.
        if (aggregate.present && kind != AudioCueKind::Foul &&
            kind != AudioCueKind::UiFeedback) continue;
        addAggregate(aggregates, kind, config_.fallbackEventImpulseNs,
            -1, -1, event.sequence);
    }

    std::vector<AudioCue> cues;
    cues.reserve(std::min(config_.maximumCuesPerTick, kMappedKindCount));
    for (std::size_t index = 0; index < kMappedKindCount &&
            cues.size() < config_.maximumCuesPerTick; ++index) {
        const Aggregate& aggregate = aggregates[index];
        if (!aggregate.present) continue;
        const AudioCueKind kind = static_cast<AudioCueKind>(index);
        const double impulse = std::clamp(aggregate.strongestImpulseNs,
            config_.minimumAudibleImpulseNs, config_.maximumMappedImpulseNs);
        const double normalized = std::sqrt(std::clamp(
            (impulse - config_.minimumAudibleImpulseNs) /
                (config_.maximumMappedImpulseNs -
                    config_.minimumAudibleImpulseNs),
            0.0, 1.0));
        AudioCue cue;
        cue.tick = tick;
        cue.kind = kind;
        cue.bus = busFor(kind);
        cue.gain = config_.minimumContactGain + normalized *
            (config_.maximumContactGain - config_.minimumContactGain);
        cue.pitch = config_.minimumContactPitch + normalized *
            (config_.maximumContactPitch - config_.minimumContactPitch);
        cue.sourceImpulseNs = impulse;
        cue.aggregateCount = aggregate.count;
        cue.primaryId = aggregate.primaryId;
        cue.secondaryId = aggregate.secondaryId;
        cue.sourceEventSequence = aggregate.sequence;
        cues.push_back(cue);
    }
    return cues;
}

const char* audioBusName(AudioBus bus) noexcept
{
    switch (bus) {
    case AudioBus::Music: return "music";
    case AudioBus::Ambience: return "ambience";
    case AudioBus::Effects: return "effects";
    case AudioBus::Ui: return "ui";
    }
    return "unknown";
}

const char* audioCueKindName(AudioCueKind kind) noexcept
{
    switch (kind) {
    case AudioCueKind::BallBall: return "ball_ball";
    case AudioCueKind::Rail: return "rail";
    case AudioCueKind::PocketJaw: return "pocket_jaw";
    case AudioCueKind::PocketDrop: return "pocket_drop";
    case AudioCueKind::CueImpact: return "cue_impact";
    case AudioCueKind::Foul: return "foul";
    case AudioCueKind::UiFeedback: return "ui_feedback";
    case AudioCueKind::MatchFinish: return "match_finish";
    case AudioCueKind::MusicLoop: return "music_loop";
    case AudioCueKind::VenueAmbience: return "venue_ambience";
    }
    return "unknown";
}

}  // namespace billiardgl::audio
