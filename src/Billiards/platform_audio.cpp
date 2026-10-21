#include "platform_audio.h"
#include <chrono>

#include "audio/audio_asset_policy.h"
#include "resource_path.h"
#include "deterministic_hash.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <unordered_map>

namespace billiardgl::audio {
namespace {

constexpr int kChannelCount = 32;
constexpr std::size_t kCueKindCount = 10;

std::size_t busIndex(AudioBus bus)
{
    return static_cast<std::size_t>(bus);
}

std::size_t cueIndex(AudioCueKind kind)
{
    return static_cast<std::size_t>(kind);
}

int mixerVolume(double gain)
{
    if (!std::isfinite(gain)) return 0;
    return static_cast<int>(std::lround(
        std::clamp(gain, 0.0, 1.0) * MIX_MAX_VOLUME));
}

}  // namespace

struct SdlMixerAudioCueConsumer::Impl {
    explicit Impl(SdlAudioConsumerOptions value) : options(std::move(value))
    {
        status.assetPackId = audioAssetPackName(options.pack);
        status.candidateReview = options.pack == AudioAssetPack::CandidateV2Review ||
            options.pack == AudioAssetPack::CandidateV3ContactReview;
        options.loopFadeInMilliseconds = std::clamp(options.loopFadeInMilliseconds, 0, 5000);
    }
    ~Impl()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (status.deviceInitialized) Mix_HaltChannel(-1);
        for (auto& entry : chunks)
            if (entry.second != nullptr) Mix_FreeChunk(entry.second);
        chunks.clear();
        if (status.deviceInitialized) {
            Mix_CloseAudio();
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
    }

    bool ensureReady(AudioConsumeResult& result)
    {
        if (status.candidateReview && !std::filesystem::path(options.candidateRoot).is_absolute()) {
            status.state = AudioConsumerState::AssetFailed;
            status.code = "audio_candidate_absolute_root_required";
            result.ok = false;
            result.code = status.code;
            return false;
        }
        if (status.state == AudioConsumerState::DeviceFailed) {
            result.ok = false;
            result.code = status.code;
            return false;
        }
        if (status.deviceInitialized) return true;
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            status.state = AudioConsumerState::DeviceFailed;
            status.code = "audio_device_sdl_init_failed";
            result.ok = false;
            result.code = status.code;
            return false;
        }
        if (Mix_OpenAudio(status.candidateReview ? 48000 : 44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            status.state = AudioConsumerState::DeviceFailed;
            status.code = "audio_device_open_failed";
            result.ok = false;
            result.code = status.code;
            return false;
        }
        Mix_AllocateChannels(kChannelCount);
        channelBus.fill(-1);
        channelBaseGain.fill(0.0);
        loopChannels.fill(-1);
        status.deviceInitialized = true;
        status.state = AudioConsumerState::Ready;
        status.code.clear();
        return true;
    }

    Mix_Chunk* chunk(const AudioAssetBinding& binding,
        AudioConsumeResult& result)
    {
        const auto found = chunks.find(binding.fileName);
        if (found != chunks.end()) {
            if (found->second == nullptr) {
                result.ok = false;
                result.code = assetErrors.at(binding.fileName);
            }
            return found->second;
        }
        const std::string path = status.candidateReview ?
            (std::filesystem::path(options.candidateRoot) / binding.fileName).string() :
            audioPath(binding.fileName);
        if (status.candidateReview) {
            std::ifstream master(path, std::ios::binary);
            std::string masterError;
            if (!master) {
                masterError = std::string("audio_candidate_master_missing:") + path;
            } else {
                const std::string bytes((std::istreambuf_iterator<char>(master)), {});
                if (sha256Hex(bytes) != binding.masterSha256)
                    masterError = std::string("audio_candidate_master_sha_mismatch:") + path;
            }
            if (!masterError.empty()) {
                status.code = masterError;
                status.state = AudioConsumerState::AssetFailed;
                result.ok = false;
                if (result.code.empty()) result.code = status.code;
                chunks.emplace(binding.fileName, nullptr);
                assetErrors.emplace(binding.fileName, masterError);
                status.cachedAssetCount = chunks.size();
                return nullptr; // No shipping alias or alternate-root fallback.
            }
        }
        Mix_Chunk* loaded = Mix_LoadWAV(path.c_str());
        chunks.emplace(binding.fileName, loaded);
        status.cachedAssetCount = chunks.size();
        if (loaded == nullptr) {
            status.state = AudioConsumerState::AssetFailed;
            status.code = std::string("audio_asset_load_failed:") +
                binding.fileName;
            assetErrors.emplace(binding.fileName, status.code);
            result.ok = false;
            if (result.code.empty()) result.code = status.code;
        }
        return loaded;
    }

    void setChannelVolume(int channel)
    {
        if (channel < 0 || channel >= kChannelCount) return;
        // SDL_mixer owns channel volume while fading. The candidate loop
        // target is set before FadeIn; writing here would cancel its envelope.
        if (status.candidateReview && Mix_FadingChannel(channel) != MIX_NO_FADING) return;
        const int index = channelBus[static_cast<std::size_t>(channel)];
        if (index < 0 || index >= 4) return;
        const AudioBusState& bus = buses[static_cast<std::size_t>(index)];
        const double gain = bus.muted ? 0.0 :
            channelBaseGain[static_cast<std::size_t>(channel)] * bus.volume;
        Mix_Volume(channel, mixerVolume(gain));
    }

    void refreshBus(AudioBus bus)
    {
        const int wanted = static_cast<int>(busIndex(bus));
        for (int channel = 0; channel < kChannelCount; ++channel) {
            if (channelBus[static_cast<std::size_t>(channel)] != wanted)
                continue;
            if (!Mix_Playing(channel)) {
                channelBus[static_cast<std::size_t>(channel)] = -1;
                channelBaseGain[static_cast<std::size_t>(channel)] = 0.0;
                continue;
            }
            setChannelVolume(channel);
        }
    }

    mutable std::mutex mutex;
    std::array<AudioBusState, 4> buses{};
    std::array<int, kChannelCount> channelBus{};
    std::array<double, kChannelCount> channelBaseGain{};
    std::array<bool, kChannelCount> channelLoop{};
    std::array<int, kCueKindCount> loopChannels{};
    std::unordered_map<std::string, Mix_Chunk*> chunks;
    std::unordered_map<std::string, std::string> assetErrors;
    AudioConsumerStatus status;
    SdlAudioConsumerOptions options;
};

SdlAudioConsumerOptions candidateAudioReviewOptionsFromEnvironment()
{
    SdlAudioConsumerOptions options;
    if (const char* root = std::getenv("BILLIARDGL_AUDIO_REVIEW_V3_ROOT")) {
        options.pack = AudioAssetPack::CandidateV3ContactReview;
        options.candidateRoot = root;
    } else if (const char* root = std::getenv("BILLIARDGL_AUDIO_REVIEW_V2_ROOT")) {
        options.pack = AudioAssetPack::CandidateV2Review;
        options.candidateRoot = root;
    }
    return options;
}

SdlMixerAudioCueConsumer::SdlMixerAudioCueConsumer(SdlAudioConsumerOptions options)
    : impl_(std::make_unique<Impl>(std::move(options)))
{
    impl_->channelBus.fill(-1);
    impl_->loopChannels.fill(-1);
}

SdlMixerAudioCueConsumer::~SdlMixerAudioCueConsumer() = default;

AudioPreparationResult SdlMixerAudioCueConsumer::prepare()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    AudioPreparationResult prepared;
    const auto start = std::chrono::steady_clock::now();
    const bool ready = impl_->ensureReady(prepared.result);
    const auto deviceReady = std::chrono::steady_clock::now();
    prepared.deviceMilliseconds = std::chrono::duration<double, std::milli>(
        deviceReady - start).count();
    if (!ready) return prepared;
    for (const AudioCueKind kind : {AudioCueKind::BallBall, AudioCueKind::Rail,
            AudioCueKind::PocketJaw, AudioCueKind::PocketDrop,
            AudioCueKind::CueImpact, AudioCueKind::Foul,
            AudioCueKind::UiFeedback, AudioCueKind::MatchFinish,
            AudioCueKind::MusicLoop, AudioCueKind::VenueAmbience}) {
        const auto binding = resolveAudioAsset(kind, impl_->options.pack);
        if (binding.status != AudioAssetStatus::Unavailable)
            (void)impl_->chunk(binding, prepared.result);
    }
    prepared.assetMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - deviceReady).count();
    if (prepared.result.ok && impl_->status.candidateReview)
        impl_->status.code = "candidate_review_not_content_accepted";
    return prepared;
}

AudioConsumeResult SdlMixerAudioCueConsumer::updateBus(
    AudioBus bus, const AudioBusState& state)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    AudioBusState safe;
    safe.volume = std::isfinite(state.volume) ?
        std::clamp(state.volume, 0.0, 1.0) : 0.0;
    safe.muted = state.muted;
    const auto previous = impl_->buses[busIndex(bus)];
    if (impl_->status.deviceInitialized && impl_->status.candidateReview &&
            (previous.muted != safe.muted || previous.volume != safe.volume)) {
        // A change during the brief fade must not be overwritten by SDL's
        // envelope. Retire that loop; the enabled controller requeues it with
        // the current gain, or keeps it off when its bus is muted.
        for (int channel = 0; channel < kChannelCount; ++channel)
            if (impl_->channelBus[channel] == static_cast<int>(busIndex(bus)) &&
                    impl_->channelLoop[channel] && Mix_FadingChannel(channel) != MIX_NO_FADING)
                Mix_HaltChannel(channel);
    }
    impl_->buses[busIndex(bus)] = safe;
    if (impl_->status.deviceInitialized) impl_->refreshBus(bus);
    return {};
}

AudioConsumeResult SdlMixerAudioCueConsumer::consume(
    const std::vector<MixedAudioCue>& cues)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    AudioConsumeResult result;
    const bool audible = std::any_of(cues.begin(), cues.end(),
        [](const MixedAudioCue& cue) { return !cue.suppressed; });
    for (const MixedAudioCue& cue : cues) {
        if (cue.suppressed) {
            ++result.suppressedCount;
            ++impl_->status.suppressedCueCount;
        }
    }
    if (!audible || !impl_->ensureReady(result)) return result;

    for (const MixedAudioCue& mixed : cues) {
        if (mixed.suppressed) continue;
        const AudioAssetBinding binding = resolveAudioAsset(mixed.cue.kind, impl_->options.pack);
        if (binding.status == AudioAssetStatus::Unavailable ||
            binding.fileName[0] == '\0') {
            result.ok = false;
            if (result.code.empty()) result.code = "audio_asset_unavailable";
            impl_->status.state = AudioConsumerState::AssetFailed;
            impl_->status.code = result.code;
            continue;
        }
        Mix_Chunk* sample = impl_->chunk(binding, result);
        if (sample == nullptr) continue;

        int channel = -1;
        bool newlyQueued = true;
        if (binding.loop) {
            int& existing = impl_->loopChannels[cueIndex(mixed.cue.kind)];
            if (existing >= 0 && Mix_Playing(existing) &&
                    impl_->channelLoop[existing] && Mix_GetChunk(existing) == sample &&
                    impl_->channelBus[existing] == static_cast<int>(busIndex(mixed.cue.bus))) {
                channel = existing;
                newlyQueued = false;
            }
            else {
                if (impl_->status.candidateReview && impl_->options.loopFadeInMilliseconds > 0) {
                    int available = -1;
                    for (int candidate = 0; candidate < kChannelCount; ++candidate)
                        if (!Mix_Playing(candidate)) { available = candidate; break; }
                    if (available >= 0) {
                        impl_->channelBus[available] = static_cast<int>(busIndex(mixed.cue.bus));
                        impl_->channelBaseGain[available] = std::isfinite(mixed.cue.gain) ?
                            std::clamp(mixed.cue.gain, 0.0, 1.0) : 0.0;
                        impl_->setChannelVolume(available);
                        existing = Mix_FadeInChannel(available, sample, -1,
                            impl_->options.loopFadeInMilliseconds);
                    } else existing = -1;
                } else existing = Mix_PlayChannel(-1, sample, -1);
                channel = existing;
            }
        } else {
            channel = Mix_PlayChannel(-1, sample, 0);
        }
        if (channel < 0 || channel >= kChannelCount) {
            result.ok = false;
            if (result.code.empty()) result.code = "audio_channel_unavailable";
            continue;
        }
        impl_->channelBus[static_cast<std::size_t>(channel)] =
            static_cast<int>(busIndex(mixed.cue.bus));
        impl_->channelLoop[static_cast<std::size_t>(channel)] = binding.loop;
        impl_->channelBaseGain[static_cast<std::size_t>(channel)] =
            std::isfinite(mixed.cue.gain) ?
                std::clamp(mixed.cue.gain, 0.0, 1.0) : 0.0;
        impl_->setChannelVolume(channel);
        if (newlyQueued) {
            ++result.queuedCount;
            ++impl_->status.queuedCueCount;
        }
        if (binding.status == AudioAssetStatus::DegradedAssetAlias) {
            ++result.degradedAliasCount;
            ++impl_->status.degradedAliasCount;
        }
        if (!binding.pitchSupported &&
            (!std::isfinite(mixed.cue.pitch) ||
                std::fabs(mixed.cue.pitch - 1.0) > 0.000001)) {
            ++result.ignoredPitchCount;
            ++impl_->status.ignoredPitchCount;
        }
    }
    if (impl_->status.state != AudioConsumerState::AssetFailed &&
        impl_->status.state != AudioConsumerState::DeviceFailed) {
        if (impl_->status.degradedAliasCount > 0) {
            impl_->status.state = AudioConsumerState::Degraded;
            impl_->status.code = "degraded_asset_alias";
        } else {
            impl_->status.state = AudioConsumerState::Ready;
            impl_->status.code = impl_->status.candidateReview ?
                "candidate_review_not_content_accepted" : "";
        }
    }
    return result;
}

std::size_t SdlMixerAudioCueConsumer::fadeOutLoops(int milliseconds)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->status.deviceInitialized) return 0;
    std::size_t count = 0;
    for (int& channel : impl_->loopChannels) {
        if (channel >= 0 && Mix_Playing(channel)) {
            count += static_cast<std::size_t>(Mix_FadeOutChannel(channel,
                std::clamp(milliseconds, 0, 5000)) != 0);
            channel = -1;
        }
    }
    return count;
}

AudioConsumerStatus SdlMixerAudioCueConsumer::status() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->status;
}

}  // namespace billiardgl::audio

namespace billiardgl {
namespace {

audio::SdlMixerAudioCueConsumer& legacyConsumer()
{
    static audio::SdlMixerAudioCueConsumer consumer;
    return consumer;
}

void playLegacyCue(audio::AudioCueKind kind, audio::AudioBus bus,
    bool loop = false)
{
    audio::AudioCue cue;
    cue.kind = kind;
    cue.bus = bus;
    cue.gain = 1.0;
    cue.pitch = 1.0;
    cue.aggregateCount = 1;
    cue.loop = loop;
    audio::MixedAudioCue mixed;
    mixed.cue = cue;
    mixed.outputGain = 1.0;
    (void)legacyConsumer().consume({mixed});
}

}  // namespace

void playBackgroundLoop()
{
    playLegacyCue(audio::AudioCueKind::MusicLoop,
        audio::AudioBus::Music, true);
}

void playHit()
{
    playLegacyCue(audio::AudioCueKind::BallBall, audio::AudioBus::Effects);
}

void playBallIn()
{
    playLegacyCue(audio::AudioCueKind::PocketDrop,
        audio::AudioBus::Effects);
}

void playGameOver()
{
    playLegacyCue(audio::AudioCueKind::MatchFinish, audio::AudioBus::Ui);
}

audio::AudioConsumerStatus legacyAudioStatus()
{
    return legacyConsumer().status();
}

}  // namespace billiardgl
