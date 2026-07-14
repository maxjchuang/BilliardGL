#include "contact_island.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace billiardgl {
namespace {

typedef std::tuple<int, int, int, int> StableKey;

StableKey stableKey(const ContinuousContactCandidate& candidate)
{
    return StableKey(static_cast<int>(candidate.kind), candidate.firstBall,
        candidate.secondBall, candidate.featureId);
}

int findRoot(std::map<int, int>& parent, int value)
{
    std::map<int, int>::iterator found = parent.find(value);
    if (found == parent.end()) {
        parent[value] = value;
        return value;
    }
    if (found->second != value) found->second = findRoot(parent, found->second);
    return found->second;
}

void unite(std::map<int, int>& parent, int first, int second)
{
    int firstRoot = findRoot(parent, first);
    int secondRoot = findRoot(parent, second);
    if (firstRoot == secondRoot) return;
    if (firstRoot < secondRoot) parent[secondRoot] = firstRoot;
    else parent[firstRoot] = secondRoot;
}

}  // namespace

ContactIslandBuildResult buildEarliestContactIslands(
    const std::vector<ContinuousContactCandidate>& candidates,
    double toiToleranceSeconds, int maximumIslandSize)
{
    ContactIslandBuildResult result;
    if (!std::isfinite(toiToleranceSeconds) || toiToleranceSeconds < 0.0 ||
        maximumIslandSize < 1) {
        result.limitExceeded = true;
        return result;
    }
    std::vector<ContinuousContactCandidate> sorted;
    for (const ContinuousContactCandidate& candidate : candidates) {
        if (candidate.valid && std::isfinite(candidate.timeOfImpactSeconds) &&
            candidate.timeOfImpactSeconds >= 0.0) sorted.push_back(candidate);
    }
    if (sorted.empty()) return result;
    std::sort(sorted.begin(), sorted.end(), continuousContactLess);
    const double earliest = sorted.front().timeOfImpactSeconds;
    sorted.erase(std::remove_if(sorted.begin(), sorted.end(),
        [earliest, toiToleranceSeconds](const ContinuousContactCandidate& value) {
            return value.timeOfImpactSeconds > earliest + toiToleranceSeconds;
        }), sorted.end());

    std::set<StableKey> seen;
    std::vector<ContinuousContactCandidate> unique;
    for (const ContinuousContactCandidate& candidate : sorted) {
        if (seen.insert(stableKey(candidate)).second) unique.push_back(candidate);
        else ++result.duplicateCandidatesRemoved;
    }

    std::map<int, int> parent;
    for (const ContinuousContactCandidate& candidate : unique) {
        findRoot(parent, candidate.firstBall);
        if (candidate.secondBall >= 0) {
            findRoot(parent, candidate.secondBall);
            unite(parent, candidate.firstBall, candidate.secondBall);
        }
    }
    std::map<int, ContactIsland> grouped;
    for (const ContinuousContactCandidate& candidate : unique) {
        const int root = findRoot(parent, candidate.firstBall);
        ContactIsland& island = grouped[root];
        island.timeOfImpactSeconds = earliest;
        island.contacts.push_back(candidate);
    }
    for (const std::pair<const int, ContactIsland>& entry : grouped) {
        ContactIsland island = entry.second;
        std::set<int> balls;
        for (const ContinuousContactCandidate& contact : island.contacts) {
            balls.insert(contact.firstBall);
            if (contact.secondBall >= 0) balls.insert(contact.secondBall);
        }
        island.ballIndices.assign(balls.begin(), balls.end());
        std::sort(island.contacts.begin(), island.contacts.end(), continuousContactLess);
        island.limitExceeded = static_cast<int>(island.ballIndices.size()) > maximumIslandSize;
        result.limitExceeded = result.limitExceeded || island.limitExceeded;
        result.islands.push_back(island);
    }
    std::sort(result.islands.begin(), result.islands.end(),
        [](const ContactIsland& first, const ContactIsland& second) {
            return first.ballIndices < second.ballIndices;
        });
    for (std::size_t index = 0; index < result.islands.size(); ++index) {
        result.islands[index].islandId = static_cast<int>(index);
    }
    return result;
}

}  // namespace billiardgl
