#include "render/modern/environment_lighting.h"
#include "render/modern/camera_projection.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool near(float first, float second, float tolerance = 1.0e-6f)
{
    return std::abs(first - second) <= tolerance;
}

bool near(const std::array<float, 3>& first,
          const std::array<float, 3>& second,
          float tolerance = 1.0e-6f)
{
    for (std::size_t channel = 0; channel < first.size(); ++channel)
        if (!near(first[channel], second[channel], tolerance)) return false;
    return true;
}

std::array<float, 3> numericalDiffuseIrradianceOverPi(
    const std::array<float, 3>& normal,
    const std::array<float, 3>& upper,
    const std::array<float, 3>& lower)
{
    const std::array<float, 3> reference = std::abs(normal[1]) < 0.9f
        ? std::array<float, 3>{{0.0f, 1.0f, 0.0f}}
        : std::array<float, 3>{{1.0f, 0.0f, 0.0f}};
    const auto normalize = [](std::array<float, 3> value) {
        const float length = std::sqrt(value[0] * value[0]
            + value[1] * value[1] + value[2] * value[2]);
        for (auto& component : value) component /= length;
        return value;
    };
    const auto cross = [](const std::array<float, 3>& first,
                          const std::array<float, 3>& second) {
        return std::array<float, 3>{{
            first[1] * second[2] - first[2] * second[1],
            first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]}};
    };
    const auto tangent = normalize(cross(reference, normal));
    const auto bitangent = cross(normal, tangent);
    constexpr int thetaSamples = 256;
    constexpr int phiSamples = 512;
    constexpr double pi = 3.14159265358979323846;
    const double dTheta = 0.5 * pi / thetaSamples;
    const double dPhi = 2.0 * pi / phiSamples;
    std::array<double, 3> integral{};
    for (int thetaIndex = 0; thetaIndex < thetaSamples; ++thetaIndex) {
        const double theta = (thetaIndex + 0.5) * dTheta;
        const double sinTheta = std::sin(theta);
        const double cosTheta = std::cos(theta);
        for (int phiIndex = 0; phiIndex < phiSamples; ++phiIndex) {
            const double phi = (phiIndex + 0.5) * dPhi;
            const double localX = sinTheta * std::cos(phi);
            const double localY = sinTheta * std::sin(phi);
            const std::array<double, 3> direction{{
                tangent[0] * localX + bitangent[0] * localY
                    + normal[0] * cosTheta,
                tangent[1] * localX + bitangent[1] * localY
                    + normal[1] * cosTheta,
                tangent[2] * localX + bitangent[2] * localY
                    + normal[2] * cosTheta}};
            const double upperWeight = 0.5 + 0.5 * direction[1];
            const double weight = cosTheta * sinTheta * dTheta * dPhi
                / pi;
            for (std::size_t channel = 0; channel < integral.size(); ++channel) {
                const double radiance = lower[channel] * (1.0 - upperWeight)
                    + upper[channel] * upperWeight;
                integral[channel] += radiance * weight;
            }
        }
    }
    return {{static_cast<float>(integral[0]),
             static_cast<float>(integral[1]),
             static_cast<float>(integral[2])}};
}

} // namespace

int main()
{
    using namespace billiardgl::render;
    LightingRigDefinition lighting;
    lighting.environmentUpper = {0.8f, 0.7f, 0.6f};
    lighting.environmentLower = {0.1f, 0.2f, 0.3f};
    lighting.keyFluxLumens = 6200.0f;
    lighting.keyTemperatureKelvin = 4000.0f;
    lighting.keyWidthMetres = 0.61f;
    lighting.keyDepthMetres = 1.81f;
    const std::array<std::array<float, 3>, 6> faceAxes{{
        {{1.0f, 0.0f, 0.0f}}, {{-1.0f, 0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}}, {{0.0f, -1.0f, 0.0f}},
        {{0.0f, 0.0f, 1.0f}}, {{0.0f, 0.0f, -1.0f}},
    }};
    for (int face = 0; face < 6; ++face)
        if (environmentCubeDirection(face, 0.0f, 0.0f)
            != faceAxes[static_cast<std::size_t>(face)]) return EXIT_FAILURE;
    int sharedEdges = 0;
    for (int first = 0; first < 6; ++first) for (int second = first + 1; second < 6; ++second)
        for (int a = 0; a < 4; ++a) for (int b = 0; b < 4; ++b) {
            const auto edge = [](int face, int side, float t) {
                return environmentCubeDirection(face, side < 2 ? (side == 0 ? -1.0f : 1.0f) : t,
                    side >= 2 ? (side == 2 ? -1.0f : 1.0f) : t);
            };
            const auto middleA = edge(first, a, 0.0f);
            const auto middleB = edge(second, b, 0.0f);
            if (middleA != middleB) continue;
            ++sharedEdges;
            for (float t : {-0.9f, -0.5f, 0.0f, 0.5f, 0.9f}) {
                const auto direction = edge(first, a, t);
                auto other = edge(second, b, t);
                if (direction != other) other = edge(second, b, -t);
                if (direction != other) return EXIT_FAILURE;
                for (float roughness : {0.0f, 0.12f, 0.5f, 1.0f}) {
                    for (bool includeKey : {false, true}) {
                        const auto radiance = environmentRadiance(
                            direction, roughness, includeKey, false, lighting);
                        if (radiance != environmentRadiance(
                            other, roughness, includeKey, false, lighting))
                            return EXIT_FAILURE;
                        for (float value : radiance)
                            if (!std::isfinite(value) || value < 0.0f)
                                return EXIT_FAILURE;
                    }
                }
            }
        }
    if (sharedEdges != 12) return EXIT_FAILURE;
    const auto upper = environmentRadiance(
        {0.0f, 1.0f, 0.0f}, 0.0f, true, false, lighting);
    const auto upperWithoutKey = environmentRadiance(
        {0.0f, 1.0f, 0.0f}, 0.0f, false, false, lighting);
    const auto lower = environmentRadiance(
        {0.0f, -1.0f, 0.0f}, 0.0f, true, false, lighting);
    const auto lowerWithoutKey = environmentRadiance(
        {0.0f, -1.0f, 0.0f}, 0.0f, false, false, lighting);
    if (upperWithoutKey != lighting.environmentUpper
        || lowerWithoutKey != lighting.environmentLower)
        return EXIT_FAILURE;
    if (!(upper[0] > lower[0])) return EXIT_FAILURE;
    if (!(upper[0] > upperWithoutKey[0])) return EXIT_FAILURE;
    if (lower != lowerWithoutKey) return EXIT_FAILURE;
    const auto authoredAlongWidth = environmentRadiance(
        {0.35f, 0.9f, 0.0f}, 0.12f, true, true, lighting);
    const auto authoredAlongDepth = environmentRadiance(
        {0.0f, 0.9f, 0.35f}, 0.12f, true, true, lighting);
    if (!(authoredAlongDepth[0] > authoredAlongWidth[0]))
        return EXIT_FAILURE;

    const auto legacyScenePolicy = environmentCubeSamplingPolicy(
        lighting, EnvironmentCubeRole::SceneRadiance);
    const auto legacyIrradiancePolicy = environmentCubeSamplingPolicy(
        lighting, EnvironmentCubeRole::DiffuseIrradiance);
    const auto legacySpecularPolicy = environmentCubeSamplingPolicy(
        lighting, EnvironmentCubeRole::SpecularRadiance);
    if (!legacyScenePolicy.includeKeyFixture
        || legacyIrradiancePolicy.includeKeyFixture
        || !legacySpecularPolicy.includeKeyFixture)
        return EXIT_FAILURE;
    if (environmentCubeSamplingPolicy(
            lighting, EnvironmentCubeRole::SpecularRadiance, 1)
            .includeKeyFixture)
        return EXIT_FAILURE;
    const auto legacyAuthoredAspect = environmentCubeSamplingPolicy(
        lighting, EnvironmentCubeRole::SpecularRadiance, 2);
    if (!legacyAuthoredAspect.includeKeyFixture
        || !legacyAuthoredAspect.useAuthoredKeyAspect)
        return EXIT_FAILURE;

    LightingRigDefinition normalized = lighting;
    normalized.environmentContract =
        EnvironmentRadianceContract::NormalizedTintLuminanceV2;
    normalized.keyFixtureOwnership = KeyFixtureOwnership::DirectRectAreaOnly;
    normalized.environmentUpperTint = {1.0f, 1.0f, 1.0f};
    normalized.environmentLowerTint = {1.0f, 1.0f, 1.0f};
    normalized.environmentUpperLuminance = 0.18f;
    normalized.environmentLowerLuminance = 0.06f;
    if (!near(resolvedEnvironmentEndpoint(normalized, true),
              {0.18f, 0.18f, 0.18f})
        || !near(resolvedEnvironmentEndpoint(normalized, false),
                 {0.06f, 0.06f, 0.06f}))
        return EXIT_FAILURE;

    const auto normalizedUpper = resolvedEnvironmentEndpoint(normalized, true);
    const auto normalizedLower = resolvedEnvironmentEndpoint(normalized, false);
    const std::array<std::array<float, 3>, 5> convolutionNormals{{
        {{0.0f, 1.0f, 0.0f}}, {{0.0f, -1.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}},
        {{0.6f, 0.8f, 0.0f}},
    }};
    for (const auto& normal : convolutionNormals) {
        const auto analytic = environmentCubeRadiance(
            EnvironmentCubeRole::DiffuseIrradiance,
            normal, 1.0f, normalized);
        const auto numerical = numericalDiffuseIrradianceOverPi(
            normal, normalizedUpper, normalizedLower);
        if (!near(analytic, numerical, 3.0e-6f)) return EXIT_FAILURE;
        // Diffuse E/pi is independent of any specular-prefilter roughness.
        if (analytic != environmentCubeRadiance(
                EnvironmentCubeRole::DiffuseIrradiance,
                normal, 0.0f, normalized))
            return EXIT_FAILURE;
    }
    const auto averageEndpoint = std::array<float, 3>{{
        0.5f * (normalizedUpper[0] + normalizedLower[0]),
        0.5f * (normalizedUpper[1] + normalizedLower[1]),
        0.5f * (normalizedUpper[2] + normalizedLower[2])}};
    if (!near(environmentCubeRadiance(
                  EnvironmentCubeRole::DiffuseIrradiance,
                  {1.0f, 0.0f, 0.0f}, 1.0f, normalized),
              averageEndpoint))
        return EXIT_FAILURE;

    LightingRigDefinition constantEnvironment = normalized;
    constantEnvironment.environmentUpperLuminance = 0.12f;
    constantEnvironment.environmentLowerLuminance = 0.12f;
    for (const auto& normal : convolutionNormals)
        if (!near(environmentCubeRadiance(
                      EnvironmentCubeRole::DiffuseIrradiance,
                      normal, 0.43f, constantEnvironment),
                  {0.12f, 0.12f, 0.12f}))
            return EXIT_FAILURE;

    // The new analytic branch is diffuse-only.  Scene and specular roles keep
    // consuming the pre-existing directional/pre-filtered radiance function.
    const std::array<float, 3> roleProbe{{0.0f, 1.0f, 0.0f}};
    constexpr float roleProbeRoughness = 0.25f;
    const auto prefilteredProbe = environmentRadiance(
        roleProbe, roleProbeRoughness, false, false, normalized);
    if (environmentCubeRadiance(EnvironmentCubeRole::SceneRadiance,
            roleProbe, roleProbeRoughness, normalized) != prefilteredProbe
        || environmentCubeRadiance(EnvironmentCubeRole::SpecularRadiance,
            roleProbe, roleProbeRoughness, normalized) != prefilteredProbe
        || environmentCubeRadiance(EnvironmentCubeRole::DiffuseIrradiance,
            roleProbe, roleProbeRoughness, normalized) == prefilteredProbe)
        return EXIT_FAILURE;

    for (const auto role : {EnvironmentCubeRole::SceneRadiance,
                            EnvironmentCubeRole::DiffuseIrradiance,
                            EnvironmentCubeRole::SpecularRadiance}) {
        const auto policy = environmentCubeSamplingPolicy(normalized, role, 2);
        if (policy.includeKeyFixture || policy.useAuthoredKeyAspect)
            return EXIT_FAILURE;
    }

    LightingRigDefinition zero = normalized;
    zero.environmentUpperLuminance = 0.0f;
    zero.environmentLowerLuminance = 0.0f;
    zero.keyFluxLumens = 0.0f;
    for (const auto role : {EnvironmentCubeRole::SceneRadiance,
                            EnvironmentCubeRole::DiffuseIrradiance,
                            EnvironmentCubeRole::SpecularRadiance})
        for (const auto direction : faceAxes)
            if (environmentCubeRadiance(role, direction, 0.37f, zero)
                != std::array<float, 3>{{0.0f, 0.0f, 0.0f}})
                return EXIT_FAILURE;

    LightingRigDefinition doubled = normalized;
    doubled.environmentUpperLuminance *= 2.0f;
    doubled.environmentLowerLuminance *= 2.0f;
    for (const auto role : {EnvironmentCubeRole::SceneRadiance,
                            EnvironmentCubeRole::DiffuseIrradiance,
                            EnvironmentCubeRole::SpecularRadiance})
        for (const auto direction : faceAxes)
            for (float roughness : {0.0f, 0.37f, 1.0f}) {
                const auto base = environmentCubeRadiance(
                    role, direction, roughness, normalized);
                const auto twice = environmentCubeRadiance(
                    role, direction, roughness, doubled);
                for (std::size_t channel = 0; channel < base.size(); ++channel)
                    if (!near(twice[channel], base[channel] * 2.0f))
                        return EXIT_FAILURE;
            }

    // V2 owns the authored key only in the renderer's direct rectangular-light
    // path.  Key changes therefore cannot leak into any generated cube role.
    LightingRigDefinition strongerKey = normalized;
    strongerKey.keyFluxLumens *= 2.0f;
    for (const auto role : {EnvironmentCubeRole::SceneRadiance,
                            EnvironmentCubeRole::DiffuseIrradiance,
                            EnvironmentCubeRole::SpecularRadiance})
        if (environmentCubeRadiance(
                role, {0.0f, 1.0f, 0.0f}, 0.12f, normalized, 2)
            != environmentCubeRadiance(
                role, {0.0f, 1.0f, 0.0f}, 0.12f, strongerKey, 2))
            return EXIT_FAILURE;
    LightingRigDefinition strongerLegacyKey = lighting;
    strongerLegacyKey.keyFluxLumens *= 2.0f;
    if (environmentCubeRadiance(
            EnvironmentCubeRole::SceneRadiance,
            {0.0f, 1.0f, 0.0f}, 0.12f, lighting)
        == environmentCubeRadiance(
            EnvironmentCubeRole::SceneRadiance,
            {0.0f, 1.0f, 0.0f}, 0.12f, strongerLegacyKey))
        return EXIT_FAILURE;
    billiardgl::CameraPresentation camera;
    camera.eye = {10.0f, 20.0f, 30.0f};
    camera.target = {-4.0f, 8.0f, 12.0f};
    camera.projectionPan = {3.0f, -5.0f, 7.0f};
    if (sharedCameraEye(camera) != Float3{{13.0f, 15.0f, 37.0f}} ||
        sharedCameraTarget(camera) != Float3{{-1.0f, 3.0f, 19.0f}})
        return EXIT_FAILURE;
    billiardgl::CameraPresentation unpanned = camera;
    unpanned.eye = {13.0f, 15.0f, 37.0f};
    unpanned.target = {-1.0f, 3.0f, 19.0f};
    unpanned.projectionPan = {};
    if (sharedLookAt(camera) != sharedLookAt(unpanned))
        return EXIT_FAILURE;
    std::cout << "continuous_environment_edges=12 diffuse_e_over_pi=PASS\n";
    return EXIT_SUCCESS;
}
