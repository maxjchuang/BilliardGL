#include "audio/audio_asset_policy.h"

namespace billiardgl::audio {

AudioAssetBinding resolveAudioAsset(AudioCueKind kind, AudioAssetPack pack) noexcept
{
    if (pack == AudioAssetPack::CandidateV3ContactReview) {
        switch (kind) {
        case AudioCueKind::CueImpact: return {kind,"cue_impact.wav","cue_impact",AudioAssetStatus::CandidateReviewAsset,false,false,"d2e6c5c2b2d1400b367bbda58807c6a583a7480853e799a28672caf232ef0dc9"};
        case AudioCueKind::BallBall: return {kind,"ball_ball.wav","ball_ball",AudioAssetStatus::CandidateReviewAsset,false,false,"4bef273976f2bc2a495436a5ca55c61cd070bbf2b08cdf951daaabe2b7549fe3"};
        case AudioCueKind::Rail: return {kind,"rail.wav","rail",AudioAssetStatus::CandidateReviewAsset,false,false,"ca15f8d489ec37cbc977e3373bef9c87952231cfb9c23ef418db762c8e4a5b18"};
        case AudioCueKind::PocketJaw: return {kind,"pocket_jaw.wav","pocket_jaw",AudioAssetStatus::CandidateReviewAsset,false,false,"7b4c4d66af578301013517eae5e53b5fa37fb416b336a74e02a71cb9972fdfff"};
        case AudioCueKind::PocketDrop: return {kind,"pocket_drop.wav","pocket_drop",AudioAssetStatus::CandidateReviewAsset,false,false,"6f6d9aeb7a72aeee8084354c4840069fd6eb08309e294720124ab184dc9dc0f2"};
        default: return resolveAudioAsset(kind, AudioAssetPack::CandidateV2Review);
        }
    }
    if (pack == AudioAssetPack::CandidateV2Review) {
        // Frozen, technically measured masters. Distinct files are not a
        // listening/rights approval and never replace shipping by default.
        switch (kind) {
        case AudioCueKind::CueImpact: return {kind,"cue_impact.wav","cue_impact",AudioAssetStatus::CandidateReviewAsset,false,false,"c3dcff3eaa703626b873500792f71242f5396bccd3e8fe99f7e11b3fb67f94ea"};
        case AudioCueKind::BallBall: return {kind,"ball_ball.wav","ball_ball",AudioAssetStatus::CandidateReviewAsset,false,false,"47e9a9e9d545279d231d06cf0a588c0b62cc491453552e07f93f37dd6885217b"};
        case AudioCueKind::Rail: return {kind,"rail.wav","rail",AudioAssetStatus::CandidateReviewAsset,false,false,"b68d9939f24ba00f07ad912354a301457cfec88e3ac1da90296881b07488462d"};
        case AudioCueKind::PocketJaw: return {kind,"pocket_jaw.wav","pocket_jaw",AudioAssetStatus::CandidateReviewAsset,false,false,"3117670921b3aa88948f43966a9fc931002d09b0e60860d8110221f6b29a959a"};
        case AudioCueKind::PocketDrop: return {kind,"pocket_drop.wav","pocket_drop",AudioAssetStatus::CandidateReviewAsset,false,false,"2eebc456895d90a7805f6211968a3f0c157438d0dae32f6d2d38ef2466b3f2fa"};
        case AudioCueKind::UiFeedback: return {kind,"ui_feedback.wav","ui_feedback",AudioAssetStatus::CandidateReviewAsset,false,false,"a31c7aae67dda2330a8c071b725494a0d39f3ce1bdc31b6c269f8f5d1bb45853"};
        case AudioCueKind::Foul: return {kind,"foul.wav","foul",AudioAssetStatus::CandidateReviewAsset,false,false,"e31a9b9e5da2538a0ed8f2eb492a70559d98a67d68344af282c7d86de88c4f66"};
        case AudioCueKind::MatchFinish: return {kind,"match_finish.wav","match_finish",AudioAssetStatus::CandidateReviewAsset,false,false,"56bb9aa33c9ec5ffbeee5ea3c903c36ece005d3fe449472f2b5b9e04be7a331a"};
        case AudioCueKind::MusicLoop: return {kind,"music_loop.wav","music_loop",AudioAssetStatus::CandidateReviewAsset,true,false,"ecca09c6e0fd855f5b796c1076912a1c53b790787489bcfb2a42fd5d2576dad1"};
        case AudioCueKind::VenueAmbience: return {kind,"venue_ambience.wav","venue_ambience",AudioAssetStatus::CandidateReviewAsset,true,false,"f74afb8c2c43149f479d31455d63ee97e03df2e2289697797bc7e5611210192d"};
        }
    }
    switch (kind) {
    case AudioCueKind::CueImpact:
    case AudioCueKind::BallBall:
    case AudioCueKind::Rail:
    case AudioCueKind::PocketJaw:
        return {kind, "hit.wav", "impact_hit_alias",
            AudioAssetStatus::DegradedAssetAlias, false, false};
    case AudioCueKind::PocketDrop:
        return {kind, "ballin.wav", "pocket_drop",
            AudioAssetStatus::DistinctAsset, false, false};
    case AudioCueKind::UiFeedback:
    case AudioCueKind::Foul:
    case AudioCueKind::MatchFinish:
        return {kind, "GameOver.wav", "notification_gameover_alias",
            AudioAssetStatus::DegradedAssetAlias, false, false};
    case AudioCueKind::MusicLoop:
    case AudioCueKind::VenueAmbience:
        return {kind, "background.wav", "background_loop_alias",
            AudioAssetStatus::DegradedAssetAlias, true, false};
    }
    return {kind, "", "unavailable", AudioAssetStatus::Unavailable,
        false, false};
}

const char* audioAssetPackName(AudioAssetPack pack) noexcept
{
    if (pack == AudioAssetPack::CandidateV3ContactReview)
        return "phase5_v3_contact_review_not_content_accepted";
    return pack == AudioAssetPack::CandidateV2Review ?
        "phase5_v2_review_not_content_accepted" : "shipping_legacy_alias";
}

const char* audioAssetStatusName(AudioAssetStatus status) noexcept
{
    switch (status) {
    case AudioAssetStatus::DistinctAsset: return "distinct_asset";
    case AudioAssetStatus::CandidateReviewAsset: return "candidate_review_not_content_accepted";
    case AudioAssetStatus::DegradedAssetAlias:
        return "degraded_asset_alias";
    case AudioAssetStatus::Unavailable: return "unavailable";
    }
    return "unavailable";
}

}  // namespace billiardgl::audio
