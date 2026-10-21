#pragma once

#include "render/modern/pbr_materials.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace billiardgl::render {

enum class EnvironmentCubeRole {
    SceneRadiance,
    DiffuseIrradiance,
    SpecularRadiance,
};

struct EnvironmentCubeSamplingPolicy {
    bool includeKeyFixture = false;
    bool useAuthoredKeyAspect = false;
};

inline EnvironmentCubeSamplingPolicy environmentCubeSamplingPolicy(
    const LightingRigDefinition& lighting, EnvironmentCubeRole role,
    int legacySpecularKeyLobeDiagnosticMode = 0)
{
    if (lighting.keyFixtureOwnership == KeyFixtureOwnership::DirectRectAreaOnly)
        return {};
    switch (role) {
    case EnvironmentCubeRole::SceneRadiance:
        return {true, false};
    case EnvironmentCubeRole::DiffuseIrradiance:
        return {};
    case EnvironmentCubeRole::SpecularRadiance:
        if (legacySpecularKeyLobeDiagnosticMode == 1) return {};
        return {true, legacySpecularKeyLobeDiagnosticMode == 2};
    }
    return {};
}

inline std::array<float, 3> resolvedEnvironmentEndpoint(
    const LightingRigDefinition& lighting, bool upper)
{
    if (lighting.environmentContract
        == EnvironmentRadianceContract::LegacyAbsoluteLinearRgbV1)
        return upper ? lighting.environmentUpper : lighting.environmentLower;
    const auto& tint = upper
        ? lighting.environmentUpperTint : lighting.environmentLowerTint;
    const float luminance = upper
        ? lighting.environmentUpperLuminance : lighting.environmentLowerLuminance;
    return {{tint[0] * luminance, tint[1] * luminance,
             tint[2] * luminance}};
}

// OpenGL cubemap face convention. Sampling one continuous directional field
// avoids treating the six storage faces as six separate colored lights.
inline std::array<float, 3> environmentCubeDirection(int face, float u, float v)
{
    std::array<float, 3> direction{};
    switch (face) {
    case 0: direction = {1.0f, -v, -u}; break;
    case 1: direction = {-1.0f, -v, u}; break;
    case 2: direction = {u, 1.0f, v}; break;
    case 3: direction = {u, -1.0f, -v}; break;
    case 4: direction = {u, -v, 1.0f}; break;
    case 5: direction = {-u, -v, -1.0f}; break;
    default: return {};
    }
    const float length = std::sqrt(direction[0] * direction[0] +
        direction[1] * direction[1] + direction[2] * direction[2]);
    for (auto& component : direction) component /= length;
    return direction;
}

inline std::array<float, 3> environmentRadiance(
    const std::array<float, 3>& direction, float roughness,
    bool includeKeyFixture, bool useAuthoredKeyAspect,
    const LightingRigDefinition& lighting)
{
    roughness = std::clamp(roughness, 0.0f, 1.0f);
    const float upperWeight = 0.5f + 0.5f * direction[1] * (1.0f - roughness);
    const float strength = std::clamp(lighting.keyFluxLumens / 6200.0f, 0.25f, 2.0f) * 3.0f;
    const float warmth = std::clamp((6500.0f - lighting.keyTemperatureKelvin) / 3000.0f, 0.0f, 1.0f);
    const std::array<float, 3> lampColor{1.0f, 1.0f - 0.15f * warmth, 1.0f - 0.40f * warmth};
    float lamp = 0.0f;
    const bool ownsKeyFixture = lighting.keyFixtureOwnership
        != KeyFixtureOwnership::DirectRectAreaOnly;
    if (includeKeyFixture && ownsKeyFixture && direction[1] > 0.0f) {
        float xCoefficient = 3.5f;
        float zCoefficient = 10.5f;
        if (useAuthoredKeyAspect) {
            // Keep the former lobe's geometric-mean width while deriving its
            // anisotropy from the authored rectangular fixture.  Width maps
            // to world X and depth maps to world Z, matching the direct-light
            // basis used by the renderer.
            const float meanCoefficient = std::sqrt(3.5f * 10.5f);
            xCoefficient = meanCoefficient *
                lighting.keyDepthMetres / lighting.keyWidthMetres;
            zCoefficient = meanCoefficient *
                lighting.keyWidthMetres / lighting.keyDepthMetres;
        }
        const float angularDistance =
            (xCoefficient * direction[0] * direction[0] +
             zCoefficient * direction[2] * direction[2]) /
            (direction[1] * direction[1]);
        lamp = strength * std::exp(-angularDistance / (1.0f + roughness * 6.0f)) /
            (1.0f + roughness * 8.0f);
    }
    const auto upper = resolvedEnvironmentEndpoint(lighting, true);
    const auto lower = resolvedEnvironmentEndpoint(lighting, false);
    std::array<float, 3> result{};
    for (int channel = 0; channel < 3; ++channel)
        result[channel] = lower[channel] * (1.0f - upperWeight) +
            upper[channel] * upperWeight + lamp * lampColor[channel];
    return result;
}

// The normalized-v2 environment is the linear directional field
// L(w)=a+b*w.y, where a=(upper+lower)/2 and b=(upper-lower)/2.  A Lambertian
// material consumes E/pi, not raw radiance.  Analytically integrating over
// the normal-oriented hemisphere gives
//
//   E(n)/pi = a + (2/3)*b*n.y.
//
// Keeping the division by pi in this producer makes the texture's unit match
// the shader's existing diffuse multiplication and avoids a hidden pi factor.
inline std::array<float, 3> normalizedDiffuseIrradianceOverPi(
    const std::array<float, 3>& normal,
    const LightingRigDefinition& lighting)
{
    const auto upper = resolvedEnvironmentEndpoint(lighting, true);
    const auto lower = resolvedEnvironmentEndpoint(lighting, false);
    const float normalY = std::clamp(normal[1], -1.0f, 1.0f);
    std::array<float, 3> result{};
    for (std::size_t channel = 0; channel < result.size(); ++channel) {
        const float average = 0.5f * (upper[channel] + lower[channel]);
        const float gradient = 0.5f * (upper[channel] - lower[channel]);
        result[channel] = average + (2.0f / 3.0f) * gradient * normalY;
    }
    return result;
}

inline std::array<float, 3> environmentCubeRadiance(
    EnvironmentCubeRole role, const std::array<float, 3>& direction,
    float roughness, const LightingRigDefinition& lighting,
    int legacySpecularKeyLobeDiagnosticMode = 0)
{
    if (role == EnvironmentCubeRole::DiffuseIrradiance
        && lighting.environmentContract
            == EnvironmentRadianceContract::NormalizedTintLuminanceV2)
        return normalizedDiffuseIrradianceOverPi(direction, lighting);
    const auto policy = environmentCubeSamplingPolicy(
        lighting, role, legacySpecularKeyLobeDiagnosticMode);
    return environmentRadiance(direction, roughness, policy.includeKeyFixture,
                               policy.useAuthoredKeyAspect, lighting);
}

} // namespace billiardgl::render
