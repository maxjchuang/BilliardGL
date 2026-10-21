#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>

namespace billiardgl::rules {

constexpr double kBreakHeadStringQuantizationGridWorldUnits = 0.000001;
constexpr const char* kBreakHeadStringReminderTextKey =
    "rules.chinese8.reminder.break_head_string_more_than_half";
constexpr const char* kBreakHeadStringFoulTextKey =
    "rules.chinese8.foul.break_head_string_more_than_half";

enum class BreakHeadStringOffenseOutcome {
    Compliant,
    Reminder,
    Foul,
};

struct BreakHeadStringReleaseSample {
    std::uint64_t shotIdentity = 0;
    int actorPlayerId = -1;
    double frozenCueBallCenterWorldXCm = 0.0;
    double frozenCueBallCenterWorldZCm = 0.0;
    double headStringWorldZCm = 0.0;
};

struct BreakHeadStringOffenseReceipt {
    std::uint64_t shotIdentity = 0;
    int actorPlayerId = -1;
    double frozenCueBallCenterWorldXCm = 0.0;
    double frozenCueBallCenterWorldZCm = 0.0;
    double headStringWorldZCm = 0.0;
    double quantizedSignedCrossingCm = 0.0;
    BreakHeadStringOffenseOutcome outcome =
        BreakHeadStringOffenseOutcome::Compliant;
    std::uint32_t playerOffenseCount = 0;
    std::string textKey;
};

inline bool operator==(const BreakHeadStringOffenseReceipt& left,
                       const BreakHeadStringOffenseReceipt& right) noexcept
{
    return left.shotIdentity == right.shotIdentity &&
        left.actorPlayerId == right.actorPlayerId &&
        left.frozenCueBallCenterWorldXCm ==
            right.frozenCueBallCenterWorldXCm &&
        left.frozenCueBallCenterWorldZCm ==
            right.frozenCueBallCenterWorldZCm &&
        left.headStringWorldZCm == right.headStringWorldZCm &&
        left.quantizedSignedCrossingCm ==
            right.quantizedSignedCrossingCm &&
        left.outcome == right.outcome &&
        left.playerOffenseCount == right.playerOffenseCount &&
        left.textKey == right.textKey;
}

struct BreakHeadStringOffenseResult {
    bool ok = false;
    std::string errorCode;
    BreakHeadStringOffenseReceipt receipt;
};

class BreakHeadStringOffenseAuthority {
public:
    BreakHeadStringOffenseResult commit(
        const BreakHeadStringReleaseSample& sample)
    {
        auto rejected = [](const char* code) {
            BreakHeadStringOffenseResult result;
            result.errorCode = code;
            return result;
        };
        if (sample.shotIdentity == 0)
            return rejected("rules_break_head_string_identity_invalid");
        if (sample.actorPlayerId < 0 || sample.actorPlayerId > 1)
            return rejected("rules_break_head_string_actor_invalid");
        if (!std::isfinite(sample.frozenCueBallCenterWorldXCm) ||
            !std::isfinite(sample.frozenCueBallCenterWorldZCm) ||
            !std::isfinite(sample.headStringWorldZCm))
            return rejected("rules_break_head_string_position_nonfinite");

        BreakHeadStringOffenseReceipt canonical;
        canonical.shotIdentity = sample.shotIdentity;
        canonical.actorPlayerId = sample.actorPlayerId;
        canonical.frozenCueBallCenterWorldXCm =
            quantize(sample.frozenCueBallCenterWorldXCm);
        canonical.frozenCueBallCenterWorldZCm =
            quantize(sample.frozenCueBallCenterWorldZCm);
        canonical.headStringWorldZCm =
            quantize(sample.headStringWorldZCm);
        canonical.quantizedSignedCrossingCm = quantize(
            canonical.frozenCueBallCenterWorldZCm -
            canonical.headStringWorldZCm);

        const auto existing = receipts_.find(sample.shotIdentity);
        if (existing != receipts_.end()) {
            if (!sameFrozenSample(existing->second, canonical))
                return rejected("rules_break_head_string_identity_conflict");
            BreakHeadStringOffenseResult result;
            result.ok = true;
            result.receipt = existing->second;
            return result;
        }

        const bool offense = canonical.quantizedSignedCrossingCm > 0.0;
        std::uint32_t nextCount = offense ?
            offenseCounts_[static_cast<std::size_t>(sample.actorPlayerId)] + 1 :
            offenseCounts_[static_cast<std::size_t>(sample.actorPlayerId)];
        canonical.playerOffenseCount = nextCount;
        if (!offense) {
            canonical.outcome = BreakHeadStringOffenseOutcome::Compliant;
        } else if (nextCount == 1) {
            canonical.outcome = BreakHeadStringOffenseOutcome::Reminder;
            canonical.textKey = kBreakHeadStringReminderTextKey;
        } else {
            canonical.outcome = BreakHeadStringOffenseOutcome::Foul;
            canonical.textKey = kBreakHeadStringFoulTextKey;
        }

        if (offense)
            offenseCounts_[static_cast<std::size_t>(sample.actorPlayerId)] =
                nextCount;
        receipts_.emplace(sample.shotIdentity, canonical);
        lastReceipt_ = canonical;
        BreakHeadStringOffenseResult result;
        result.ok = true;
        result.receipt = canonical;
        return result;
    }

    std::uint32_t offenseCount(int player) const noexcept
    {
        return player >= 0 && player < 2 ?
            offenseCounts_[static_cast<std::size_t>(player)] : 0;
    }

    const std::optional<BreakHeadStringOffenseReceipt>&
    lastReceipt() const noexcept
    {
        return lastReceipt_;
    }

    void resetMatch()
    {
        offenseCounts_ = {{0, 0}};
        receipts_.clear();
        lastReceipt_.reset();
    }

    std::string stateDigest() const
    {
        std::ostringstream value;
        value.imbue(std::locale::classic());
        value << std::setprecision(std::numeric_limits<double>::max_digits10)
              << offenseCounts_[0] << '|' << offenseCounts_[1]
              << '|' << receipts_.size();
        for (const auto& item : receipts_) {
            const BreakHeadStringOffenseReceipt& receipt = item.second;
            value << '|' << item.first << ':' << receipt.actorPlayerId
                  << ':' << receipt.frozenCueBallCenterWorldXCm
                  << ':' << receipt.frozenCueBallCenterWorldZCm
                  << ':' << receipt.headStringWorldZCm
                  << ':' << receipt.quantizedSignedCrossingCm
                  << ':' << static_cast<int>(receipt.outcome)
                  << ':' << receipt.playerOffenseCount
                  << ':' << receipt.textKey.size() << ':' << receipt.textKey;
        }
        return value.str();
    }

private:
    static double quantize(double value) noexcept
    {
        return std::round(
            value / kBreakHeadStringQuantizationGridWorldUnits) *
            kBreakHeadStringQuantizationGridWorldUnits;
    }

    static bool sameFrozenSample(
        const BreakHeadStringOffenseReceipt& left,
        const BreakHeadStringOffenseReceipt& right) noexcept
    {
        return left.shotIdentity == right.shotIdentity &&
            left.actorPlayerId == right.actorPlayerId &&
            left.frozenCueBallCenterWorldXCm ==
                right.frozenCueBallCenterWorldXCm &&
            left.frozenCueBallCenterWorldZCm ==
                right.frozenCueBallCenterWorldZCm &&
            left.headStringWorldZCm == right.headStringWorldZCm;
    }

    std::array<std::uint32_t, 2> offenseCounts_{{0, 0}};
    std::map<std::uint64_t, BreakHeadStringOffenseReceipt> receipts_;
    std::optional<BreakHeadStringOffenseReceipt> lastReceipt_;
};

}  // namespace billiardgl::rules
