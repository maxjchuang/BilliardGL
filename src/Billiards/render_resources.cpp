#include "render_resources.h"

namespace billiardgl {

void applyBallTexturesToState(const RenderResources& resources, GameState& state)
{
    for (int i = 0; i < kBallCount; ++i) {
        state.balls[i].texture = resources.ballTextures[i];
    }
}

}  // namespace billiardgl
