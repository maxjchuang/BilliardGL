#include "audio/audio_mixer_model.h"

#include <algorithm>
#include <cmath>

namespace billiardgl::audio {
namespace {

std::size_t busIndex(AudioBus bus)
{
    return static_cast<std::size_t>(bus);
}

double safeVolume(double value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

AudioCue loopCue(std::uint64_t tick, AudioCueKind kind, AudioBus bus)
{
    AudioCue cue;
    cue.tick = tick;
    cue.kind = kind;
    cue.bus = bus;
    cue.gain = 1.0;
    cue.pitch = 1.0;
    cue.aggregateCount = 1;
    cue.loop = true;
    return cue;
}

}  // namespace

const char* audioConsumerStateName(AudioConsumerState state) noexcept
{
    switch (state) {
    case AudioConsumerState::Dormant: return "dormant";
    case AudioConsumerState::Ready: return "ready";
    case AudioConsumerState::Degraded: return "degraded";
    case AudioConsumerState::DeviceFailed: return "device_failed";
    case AudioConsumerState::AssetFailed: return "asset_failed";
    }
    return "unknown";
}

AudioMixerModel::AudioMixerModel()
{
    applySettings(settings::ProductSettings{});
}

AudioMixerModel::AudioMixerModel(const settings::ProductSettings& settings)
{
    applySettings(settings);
}

void AudioMixerModel::applySettings(
    const settings::ProductSettings& settings) noexcept
{
    buses_[busIndex(AudioBus::Music)] = {
        safeVolume(settings.musicVolume), settings.musicMuted};
    buses_[busIndex(AudioBus::Ambience)] = {
        safeVolume(settings.ambienceVolume), settings.ambienceMuted};
    buses_[busIndex(AudioBus::Effects)] = {
        safeVolume(settings.effectsVolume), settings.effectsMuted};
    buses_[busIndex(AudioBus::Ui)] = {
        safeVolume(settings.uiVolume), settings.uiMuted};
}

const AudioBusState& AudioMixerModel::bus(AudioBus bus) const noexcept
{
    return buses_[busIndex(bus)];
}

std::vector<MixedAudioCue> AudioMixerModel::mix(
    const std::vector<AudioCue>& cues) const
{
    std::vector<MixedAudioCue> mixed;
    mixed.reserve(cues.size());
    for (const AudioCue& cue : cues) {
        const AudioBusState& state = bus(cue.bus);
        MixedAudioCue output;
        output.cue = cue;
        output.busVolume = state.volume;
        const double cueGain = std::isfinite(cue.gain) ?
            std::clamp(cue.gain, 0.0, 1.0) : 0.0;
        output.outputGain = state.muted ? 0.0 : cueGain * state.volume;
        output.suppressed = output.outputGain <= 0.0;
        mixed.push_back(output);
    }
    return mixed;
}

std::vector<AudioCue> AudioMixerModel::persistentCues(std::uint64_t tick) const
{
    return {
        loopCue(tick, AudioCueKind::MusicLoop, AudioBus::Music),
        loopCue(tick, AudioCueKind::VenueAmbience, AudioBus::Ambience),
    };
}

}  // namespace billiardgl::audio
