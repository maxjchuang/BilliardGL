#pragma once

#include "audio/audio_event_mapper.h"

namespace billiardgl::audio {

enum class AudioAssetStatus {
    DistinctAsset,
    CandidateReviewAsset,
    DegradedAssetAlias,
    Unavailable,
};

enum class AudioAssetPack { ShippingLegacy, CandidateV2Review, CandidateV3ContactReview };

struct AudioAssetBinding {
    AudioCueKind kind = AudioCueKind::BallBall;
    const char* fileName = "";
    const char* aliasGroup = "";
    AudioAssetStatus status = AudioAssetStatus::Unavailable;
    bool loop = false;
    bool pitchSupported = false;
    const char* masterSha256 = "";
};

// Honest policy for the four repository WAV files. Any semantic categories
// sharing one file are marked degraded; SDL_mixer pitch is never claimed.
AudioAssetBinding resolveAudioAsset(AudioCueKind kind,
    AudioAssetPack pack = AudioAssetPack::ShippingLegacy) noexcept;
const char* audioAssetPackName(AudioAssetPack pack) noexcept;
const char* audioAssetStatusName(AudioAssetStatus status) noexcept;

}  // namespace billiardgl::audio
