#include "contact_island.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>

namespace {
void expect(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

billiardgl::ContinuousContactCandidate pair(int first, int second, double toi)
{
    billiardgl::ContinuousContactCandidate value;
    value.valid = true;
    value.kind = billiardgl::ContinuousContactKind::BallBall;
    value.firstBall = std::min(first, second);
    value.secondBall = std::max(first, second);
    value.timeOfImpactSeconds = toi;
    return value;
}
}

int main()
{
    using namespace billiardgl;
    std::vector<ContinuousContactCandidate> candidates = {
        pair(0, 1, 0.01), pair(1, 2, 0.01000005), pair(5, 6, 0.01),
        pair(8, 9, 0.02)};
    ContactIslandBuildResult result = buildEarliestContactIslands(
        candidates, 0.0000001, 16);
    expect(result.islands.size() == 2,
        "equal-TOI graph should produce two disjoint islands");
    expect(result.islands[0].ballIndices == std::vector<int>({0, 1, 2}) &&
        result.islands[1].ballIndices == std::vector<int>({5, 6}),
        "chain connectivity and canonical ordering should be stable");

    candidates.push_back(pair(0, 1, 0.01000002));
    result = buildEarliestContactIslands(candidates, 0.0000001, 16);
    expect(result.duplicateCandidatesRemoved == 1 &&
        result.islands[0].contacts.size() == 2,
        "duplicate canonical contacts should be removed exactly once");

    std::mt19937 random(17);
    std::shuffle(candidates.begin(), candidates.end(), random);
    const ContactIslandBuildResult shuffled = buildEarliestContactIslands(
        candidates, 0.0000001, 16);
    expect(shuffled.islands.size() == result.islands.size() &&
        shuffled.islands[0].ballIndices == result.islands[0].ballIndices &&
        shuffled.islands[0].contacts.size() == result.islands[0].contacts.size(),
        "candidate generation order must not change island output");

    const ContactIslandBuildResult limited = buildEarliestContactIslands(
        {pair(0, 1, 0.0), pair(1, 2, 0.0)}, 0.0, 2);
    expect(limited.limitExceeded && limited.islands[0].limitExceeded &&
        limited.islands[0].ballIndices.size() == 3,
        "oversized islands must remain visible and report a hard limit");

    ContinuousContactCandidate left = boundaryContactCandidate(
        4, 1, 0.0, Point3{1.0f, 0.0f, 0.0f},
        PocketBoundaryEventKind::LeftJaw);
    ContinuousContactCandidate right = boundaryContactCandidate(
        4, 2, 0.0, Point3{-1.0f, 0.0f, 0.0f},
        PocketBoundaryEventKind::RightJaw);
    const ContactIslandBuildResult dualJaw = buildEarliestContactIslands(
        {right, left}, 0.0, 16);
    expect(dualJaw.islands.size() == 1 && dualJaw.islands[0].contacts.size() == 2,
        "simultaneous dual-jaw features should share the ball island");

    const ContactIslandBuildResult invalid = buildEarliestContactIslands(
        candidates, -1.0, 16);
    expect(invalid.limitExceeded && invalid.islands.empty(),
        "invalid solver controls should fail before grouping");
    return 0;
}
