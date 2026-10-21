#include "platform_audio.h"
#include "audio/product_audio_controller.h"
#include "deterministic_hash.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>

using namespace billiardgl;
using namespace billiardgl::audio;

namespace {
void expect(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
const std::string kRoot = std::string(BILLIARDGL_SOURCE_ROOT) + "/assets/audio/phase5-v2-technical-correction";
const std::array<AudioCueKind, 10> kKinds{{AudioCueKind::CueImpact, AudioCueKind::BallBall,
    AudioCueKind::Rail, AudioCueKind::PocketJaw, AudioCueKind::PocketDrop,
    AudioCueKind::UiFeedback, AudioCueKind::Foul, AudioCueKind::MatchFinish,
    AudioCueKind::MusicLoop, AudioCueKind::VenueAmbience}};

SdlAudioConsumerOptions candidateOptions(const std::string& root = kRoot)
{
    SdlAudioConsumerOptions options;
    options.pack = AudioAssetPack::CandidateV2Review;
    options.candidateRoot = root;
    return options;
}

AudioCue cue(AudioCueKind kind, double gain = .5)
{
    AudioCue value;
    value.kind = kind;
    value.bus = kind == AudioCueKind::MusicLoop ? AudioBus::Music :
        kind == AudioCueKind::VenueAmbience ? AudioBus::Ambience :
        kind == AudioCueKind::UiFeedback || kind == AudioCueKind::Foul ||
        kind == AudioCueKind::MatchFinish ? AudioBus::Ui : AudioBus::Effects;
    value.gain = gain;
    value.loop = kind == AudioCueKind::MusicLoop || kind == AudioCueKind::VenueAmbience;
    return value;
}

void verifyPolicy()
{
    expect(std::string(resolveAudioAsset(AudioCueKind::Rail).fileName) == "hit.wav",
        "shipping policy must not silently promote candidate");
    std::set<std::string> files, hashes;
    for (const auto kind : kKinds) {
        const auto binding = resolveAudioAsset(kind, AudioAssetPack::CandidateV2Review);
        expect(binding.status == AudioAssetStatus::CandidateReviewAsset && !binding.pitchSupported,
            "candidate remains unaccepted and unsupported pitch remains honest");
        files.insert(binding.fileName); hashes.insert(binding.masterSha256);
    }
    expect(files.size() == 10 && hashes.size() == 10, "ten one-to-one frozen candidate masters");
    unsetenv("BILLIARDGL_AUDIO_REVIEW_V2_ROOT");
    unsetenv("BILLIARDGL_AUDIO_REVIEW_V3_ROOT");
    expect(candidateAudioReviewOptionsFromEnvironment().pack == AudioAssetPack::ShippingLegacy,
        "default product selection remains shipping");
    setenv("BILLIARDGL_AUDIO_REVIEW_V2_ROOT", kRoot.c_str(), 1);
    expect(candidateAudioReviewOptionsFromEnvironment().pack == AudioAssetPack::CandidateV2Review,
        "explicit product review selection reaches candidate options");
    unsetenv("BILLIARDGL_AUDIO_REVIEW_V2_ROOT");
    setenv("BILLIARDGL_AUDIO_REVIEW_V3_ROOT", "/absolute/v3-review", 1);
    expect(std::string(audioAssetPackName(candidateAudioReviewOptionsFromEnvironment().pack)) ==
        "phase5_v3_contact_review_not_content_accepted",
        "explicit v3 review selection must not silently select the old timbre or shipping aliases");
    unsetenv("BILLIARDGL_AUDIO_REVIEW_V3_ROOT");
}

void verifySdlConsumer()
{
    SdlMixerAudioCueConsumer consumer(candidateOptions());
    expect(!consumer.status().deviceInitialized, "construction remains device-free");
    auto settings = settings::ProductSettings::defaults();
    settings.musicVolume = .2; settings.ambienceVolume = .3; settings.effectsVolume = .4;
    ProductAudioController controller(consumer, settings);
    expect(!consumer.status().deviceInitialized, "bus settings remain device-free before preparation");
    const auto prepared = consumer.prepare();
    expect(prepared.result.ok && consumer.status().cachedAssetCount == 10 &&
        consumer.status().candidateReview, "actual SDL decodes all ten SHA-bound PCM24 masters");
    int rate = 0, channels = 0; Uint16 format = 0;
    expect(Mix_QuerySpec(&rate, &format, &channels) && rate == 48000 && channels == 2,
        "candidate SDL device requests 48k stereo runtime format");
    expect(controller.startPersistentLoops(settings).ok && Mix_Playing(-1) == 2,
        "controller starts two independent loop channels");
    expect(Mix_GetChunk(0) != Mix_GetChunk(1) && Mix_FadingChannel(0) == MIX_FADING_IN &&
        Mix_FadingChannel(1) == MIX_FADING_IN, "separate decoded loops have actual fade-in envelopes");
    SDL_Delay(170);
    controller.applySettings(settings);
    expect(Mix_Volume(0, -1) == 26 && Mix_Volume(1, -1) == 38,
        "independent music and ambience gains survive fade completion");
    settings.ambienceMuted = true;
    controller.applySettings(settings);
    expect(Mix_Volume(0, -1) == 26 && Mix_Volume(1, -1) == 0,
        "ambience can be turned off without muting independent music content");
    settings.ambienceMuted = false; settings.musicMuted = true;
    controller.applySettings(settings);
    expect(Mix_Volume(0, -1) == 0 && Mix_Volume(1, -1) == 38,
        "music mute leaves ambience audible");

    GameRuntime authority;
    const auto before = gameStateSha256(authority.state());
    int lowVolume = 0;
    for (int i = 0; i < 2; ++i) {
        ProductAudioStep step;
        step.generation = 1; step.tick = static_cast<std::uint64_t>(i + 1);
        PhysicsContactRecord contact;
        contact.kind = PhysicsContactKind::BallBall;
        contact.firstBall = 0; contact.secondBall = 1;
        contact.normalImpulseNs = i ? .3 : .01;
        contact.velocityImpulseApplied = true;
        step.telemetry.contacts.push_back(contact);
        const auto result = controller.consumeStep(step, settings);
        expect(result.ok && result.mappedCueCount == 1 && result.consumer.degradedAliasCount == 0,
            "physical contact facts reach distinct candidate through controller and SDL");
        const int volume = Mix_Volume(2, -1);
        if (!i) lowVolume = volume;
        else expect(volume > lowVolume && volume <= 51, "actual channel gain follows impulse and effects bus");
        const auto count = consumer.status().queuedCueCount;
        expect(controller.consumeStep(step, settings).duplicateOrOutOfOrder &&
            consumer.status().queuedCueCount == count, "same tick does not replay collision or persistent loops");
        Mix_HaltChannel(2);
    }
    expect(gameStateSha256(authority.state()) == before, "SDL candidate consumption cannot mutate physics authority");
    settings.musicMuted = false;
    controller.applySettings(settings);
    expect(consumer.fadeOutLoops(120) == 2 && Mix_FadingChannel(0) == MIX_FADING_OUT &&
        Mix_FadingChannel(1) == MIX_FADING_OUT, "actual SDL fade-out reaches both loop channels");
    SDL_Delay(170);
    expect(Mix_Playing(-1) == 0, "loops stop at the end of the bounded fade");
    settings.musicMuted = true; settings.ambienceMuted = true;
    controller.startPersistentLoops(settings);
    expect(Mix_Playing(-1) == 0, "initially muted persistent buses queue no sound");
    settings.musicMuted = false;
    controller.applySettings(settings);
    expect(Mix_Playing(-1) == 1 && Mix_FadingChannel(0) == MIX_FADING_IN,
        "unmuting an initially muted bus starts its previously unqueued loop");
    settings.musicVolume = .8;
    controller.applySettings(settings);
    SDL_Delay(170);
    controller.applySettings(settings);
    expect(Mix_Playing(-1) == 1 && Mix_Volume(0, -1) == 102,
        "gain change during fade is not overwritten by the SDL envelope");
    consumer.fadeOutLoops(120); SDL_Delay(170);
    expect(controller.snapshot().queuedCueCount == consumer.status().queuedCueCount,
        "controller queue accounting includes explicitly enabled loop starts exactly once");
    std::cout << "candidate_policy=10_distinct_unaccepted sdl_decode=10/10 gain=PASS bus_mute=PASS"
        << " fade=PASS duplicate_tick=PASS authority_hash=unchanged driver="
        << SDL_GetCurrentAudioDriver() << " format=" << format << '\n';
}

void verifyMissingAndWrongMaster()
{
    {
        SdlMixerAudioCueConsumer consumer(candidateOptions(""));
        const auto result = consumer.prepare();
        expect(!result.result.ok && result.result.code == "audio_candidate_absolute_root_required" &&
            !consumer.status().deviceInitialized, "missing explicit candidate root fails before device initialization");
    }
    const auto temporary = std::filesystem::temp_directory_path() /
        ("billiardgl-audio-candidate-" + std::to_string(SDL_GetTicks64()));
    expect(!std::filesystem::exists(temporary), "isolated audio-negative temporary directory");
    std::filesystem::create_directory(temporary);
    {
        SdlMixerAudioCueConsumer consumer(candidateOptions(temporary.string()));
        const auto result = consumer.prepare();
        expect(!result.result.ok && result.result.code.find("audio_candidate_master_missing:") == 0 &&
            consumer.status().state == AudioConsumerState::AssetFailed,
            "missing candidate reports exact master and does not fall back to shipping aliases");
    }
    { std::ofstream(temporary / "cue_impact.wav", std::ios::binary) << "not the frozen master"; }
    {
        SdlMixerAudioCueConsumer consumer(candidateOptions(temporary.string()));
        AudioMixerModel mixer;
        const auto result = consumer.consume(mixer.mix({cue(AudioCueKind::CueImpact)}));
        expect(!result.ok && result.code.find("audio_candidate_master_sha_mismatch:") == 0 &&
            result.queuedCount == 0, "tampered master is rejected before SDL decode or playback");
    }
    std::filesystem::remove_all(temporary);
    std::cout << "candidate_missing_root=PASS missing_master=PASS wrong_master_SHA=PASS no_alias_fallback=PASS\n";
}

struct Recording { std::mutex mutex; std::vector<unsigned char> pcm; };
void record(void* context, Uint8* stream, int length)
{
    auto& recording = *static_cast<Recording*>(context);
    std::lock_guard<std::mutex> lock(recording.mutex);
    recording.pcm.insert(recording.pcm.end(), stream, stream + length);
}

void saveRecording(const std::string& path, const Recording& recording)
{
    // C11 exclusive creation: an existing file or a competing creator is never
    // truncated. In particular a mis-specified master/evidence path is safe.
    std::unique_ptr<std::FILE,decltype(&std::fclose)> output(std::fopen(path.c_str(),"wbx"),&std::fclose);
    expect(output != nullptr,"audio_review_output_requires_new_file");
    bool complete = true;
    const auto write = [&](const char* bytes,std::size_t size) {
        complete &= std::fwrite(bytes,1,size,output.get()) == size;
    };
    const auto u16 = [&](std::uint16_t x) { const char bytes[]{static_cast<char>(x),static_cast<char>(x>>8)}; write(bytes,2); };
    const auto u32 = [&](std::uint32_t x) { const char bytes[]{static_cast<char>(x),static_cast<char>(x>>8),static_cast<char>(x>>16),static_cast<char>(x>>24)}; write(bytes,4); };
    write("RIFF",4); u32(static_cast<std::uint32_t>(recording.pcm.size()+36));
    write("WAVEfmt ",8); u32(16); u16(1); u16(2); u32(48000); u32(192000); u16(4); u16(16);
    write("data",4); u32(static_cast<std::uint32_t>(recording.pcm.size()));
    write(reinterpret_cast<const char*>(recording.pcm.data()),recording.pcm.size());
    expect(complete && std::fflush(output.get())==0 && !recording.pcm.empty(),
        "actual SDL postmix PCM preview is saved");
}

void requireNewRecordingDestination(const std::string& path)
{
    expect(!path.empty(),"audio_review_output_path_required");
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path,error);
    expect((!error || error==std::errc::no_such_file_or_directory) &&
        !std::filesystem::exists(status),"audio_review_output_requires_new_file");
    const auto target = std::filesystem::weakly_canonical(path);
    for (const auto& root : {kRoot,std::string(BILLIARDGL_SOURCE_ROOT)+"/assets/audio/phase5-v3-contact-review"}) {
        const auto relative = target.lexically_relative(std::filesystem::weakly_canonical(root));
        expect(!relative.empty() && *relative.begin()=="..","audio_review_output_inside_frozen_candidate_root");
    }
}

void compareContactV3(const std::string& capture)
{
    Recording recording;
    std::size_t queuedCount = 0;
    std::string driver;
    const auto position = [&]() { std::lock_guard<std::mutex> lock(recording.mutex);
        return static_cast<double>(recording.pcm.size()) / 192000.0; };
    const std::array<AudioCueKind,5> contacts{{AudioCueKind::CueImpact,AudioCueKind::BallBall,
        AudioCueKind::Rail,AudioCueKind::PocketJaw,AudioCueKind::PocketDrop}};
    for (const auto kind : contacts) {
        for (const bool revised : {false,true}) {
            auto options = candidateOptions();
            if (revised) {
                options.pack = AudioAssetPack::CandidateV3ContactReview;
                options.candidateRoot = std::string(BILLIARDGL_SOURCE_ROOT) + "/assets/audio/phase5-v3-contact-review";
            }
            SdlMixerAudioCueConsumer consumer(options);
            const auto prepared = consumer.prepare();
            expect(prepared.result.ok && consumer.status().cachedAssetCount == 10,
                "comparison loads all ten exact SHA-bound masters for the selected review pack");
            driver = SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "unavailable";
            int rate = 0, channels = 0; Uint16 format = 0;
            expect(Mix_QuerySpec(&rate,&format,&channels) && rate==48000 && channels==2 && format==AUDIO_S16LSB,
                "comparison binds actual PCM16 stereo 48k output");
            auto settings = settings::ProductSettings::defaults();
            settings.effectsVolume = .7;
            ProductAudioController controller(consumer,settings);
            AudioMixerModel mixer(settings);
            Mix_SetPostMix(record,&recording);
            const double start = position();
            const auto queued = consumer.consume(mixer.mix({cue(kind,1.0)}));
            expect(queued.ok && queued.queuedCount==1 && queued.degradedAliasCount==0,
                "comparison queues one actual non-aliased contact cue");
            queuedCount += queued.queuedCount;
            while (Mix_Playing(-1)) SDL_Delay(10);
            SDL_Delay(160);
            Mix_SetPostMix(nullptr,nullptr);
            const auto binding = resolveAudioAsset(kind,options.pack);
            std::cout << "comparison cue=" << audioCueKindName(kind) << " variant=" << (revised?"v3":"v2")
                << " start_seconds=" << start << " end_seconds=" << position()
                << " master_sha=" << binding.masterSha256 << " decoded_master_count=10 queued=1\n";
        }
    }
    expect(queuedCount==10,"comparison must keep five complete old/new pairs");
    saveRecording(capture,recording);
    std::cout << "comparison_actual_queued_cues=" << queuedCount << " pairs=5 driver="
        << driver << " content_acceptance=NOT_CLAIMED\n";
}

void reviewPlayback(const std::string& capture)
{
    SdlMixerAudioCueConsumer consumer(candidateOptions());
    expect(consumer.prepare().result.ok, "review playback loads exact candidate masters");
    int rate = 0, channels = 0; Uint16 format = 0;
    expect(Mix_QuerySpec(&rate, &format, &channels) && rate == 48000 && channels == 2 &&
        format == AUDIO_S16LSB, "review WAV binds actual 48k stereo signed little-endian PCM16 output");
    Recording recording;
    if (!capture.empty()) Mix_SetPostMix(record, &recording);
    auto settings = settings::ProductSettings::defaults();
    settings.effectsVolume = .7; settings.uiVolume = .7;
    ProductAudioController controller(consumer, settings);
    AudioMixerModel mixer(settings);
    for (const auto kind : kKinds) {
        if (kind == AudioCueKind::MusicLoop || kind == AudioCueKind::VenueAmbience) continue;
        std::cout << "review_one_shot=" << audioCueKindName(kind) << '\n';
        const auto queued = consumer.consume(mixer.mix({cue(kind, 1.0)}));
        expect(queued.ok && queued.queuedCount == 1 && queued.degradedAliasCount == 0,
            "each labelled review cue actually queues one distinct SDL channel");
        while (Mix_Playing(-1)) SDL_Delay(20);
        SDL_Delay(180);
    }
    settings.musicVolume = .5; settings.ambienceMuted = true;
    const auto music = controller.startPersistentLoops(settings);
    expect(music.ok && music.consumer.queuedCount == 1, "music-only review loop actually queues");
    std::cout << "review_loop=music_only excerpt_seconds=4\n";
    SDL_Delay(4000);
    settings.musicMuted = true; settings.ambienceMuted = false; settings.ambienceVolume = .5;
    const auto ambience = controller.applySettings(settings);
    expect(ambience.ok && ambience.consumer.queuedCount == 1, "ambience-only review loop actually queues");
    std::cout << "review_loop=ambience_only excerpt_seconds=4\n";
    SDL_Delay(4000);
    consumer.fadeOutLoops(120); SDL_Delay(170);
    Mix_SetPostMix(nullptr, nullptr);
    expect(consumer.status().queuedCueCount == 10, "review sequence actually queues all ten semantic assets once");
    std::cout << "review_actual_queued_cues=10 distinct_semantic_assets=10 content_acceptance=NOT_CLAIMED\n";
    if (!capture.empty()) {
        saveRecording(capture,recording);
        std::cout << "review_capture=" << capture << " pcm_bytes=" << recording.pcm.size()
            << " driver=" << SDL_GetCurrentAudioDriver() << " content_acceptance=NOT_CLAIMED\n";
    }
}
} // namespace

int main(int argc, char** argv)
{
    try {
        const bool audition = argc == 2 && std::string(argv[1]) == "--audition";
        const bool capture = argc == 3 && std::string(argv[1]) == "--capture";
        const bool compare = argc == 3 && std::string(argv[1]) == "--compare-v3";
        if (capture || compare) requireNewRecordingDestination(argv[2]);
        if (!audition) setenv("SDL_AUDIODRIVER", "dummy", 1);
        verifyPolicy();
        if (!audition) { verifySdlConsumer(); verifyMissingAndWrongMaster(); }
        if (audition || capture) reviewPlayback(capture ? argv[2] : "");
        if (compare) compareContactV3(argv[2]);
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
