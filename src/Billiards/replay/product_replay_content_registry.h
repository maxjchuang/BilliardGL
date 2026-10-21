#pragma once

#include "replay/replay_v1.h"

#include <cstddef>
#include <map>
#include <string>

namespace billiardgl::replay {

inline constexpr const char* kOfficialSingleRackReplayRulesetId =
    "wpa_heyball_2025_08_16_v1";

enum class ProductReplayContentScope {
    TrainingAttempt,
    SingleRackMatch,
};

struct TrustedReplayContentBinding {
    std::string contentSha256;
    std::string physicsProfileId;
    std::string physicsProfileSha256;
    PhysicsProfile physicsProfile;
    std::string initialStateSha256;
    std::string initialAuthorityStateSha256;
    std::uint64_t initialRulesRevision = 0;
    std::string initialRulesEffectSha256;
    ProductReplayContentScope scope =
        ProductReplayContentScope::TrainingAttempt;
    int minimumFormatVersion = kReplayFormatVersion;
    int maximumFormatVersion = kReplayFormatVersion;
    bool allowsLegacyRules = true;
    bool allowsChineseEightBallRules = true;
};

struct ProductReplayContentRegistryResult {
    bool ok = false;
    std::string errorCode;
};

TrustedReplayContentBinding officialSingleRackReplayBinding(
    const PhysicsProfile& profile);

class ProductReplayContentRegistry {
public:
    ProductReplayContentRegistryResult addTrustedContent(
        TrustedReplayContentBinding binding);
    ProductReplayContentRegistryResult addTrainingAttempt(
        const std::string& contentSha256,const PhysicsProfile& profile);
    ProductReplayContentRegistryResult addOfficialSingleRack(
        const PhysicsProfile& profile);
    ProductReplayContentRegistryResult authorize(const Document& document,
        const TrustedReplayContentBinding*& binding) const;
    std::size_t size() const noexcept { return bindings_.size(); }

private:
    std::map<std::string,TrustedReplayContentBinding> bindings_;
};

}  // namespace billiardgl::replay
