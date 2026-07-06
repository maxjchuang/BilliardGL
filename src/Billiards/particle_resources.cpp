#include "particle_resources.h"

#include "particle.h"

#include <cstdlib>

namespace billiardgl {
namespace {

unsigned int sFlameTexture = 0;

particle* createFlameParticle()
{
    const float size = static_cast<float>(std::rand() % 90) * 0.02f;
    float speed[] = {
        static_cast<float>(std::rand() % 10 - 4) / 1600.0f,
        static_cast<float>(std::rand() % 10 - 4) / 800.0f,
        static_cast<float>(std::rand() % 10 - 4) / 1600.0f
    };
    float acc[] = {
        static_cast<float>(std::rand() % 3 - 1) / 9000000.0f,
        4.9f / 4000000.0f,
        static_cast<float>(std::rand() % 3 - 1) / 9000000.0f
    };
    float angle[] = {
        static_cast<float>(std::rand() % 360),
        static_cast<float>(std::rand() % 360),
        static_cast<float>(std::rand() % 360)
    };
    return new particle(vec(size, size, size), vec(speed), vec(acc), vec(angle), std::rand() % 50 + 10, sFlameTexture);
}

}  // namespace

void initializeParticleEmitters(RenderResources& resources, const GameState& state)
{
    sFlameTexture = resources.flameTexture;
    for (int i = 0; i < kBallCount; ++i) {
        const BallState& ball = state.balls[i];
        resources.emitters[i] = new emitter(
            createFlameParticle,
            5000,
            -kBallRadius + ball.position.x,
            kBallRadius + ball.position.x,
            ball.position.y,
            ball.position.y,
            ball.position.z,
            ball.position.z);
    }
}

void destroyParticleEmitters(RenderResources& resources)
{
    for (int i = 0; i < kBallCount; ++i) {
        delete resources.emitters[i];
        resources.emitters[i] = nullptr;
    }
}

}  // namespace billiardgl
