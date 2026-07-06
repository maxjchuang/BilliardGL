#include "game_state.h"
#include "render_resources.h"

#include <cassert>

int main()
{
    billiardgl::GameState state;
    billiardgl::RenderResources resources;

    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        resources.ballTextures[i] = static_cast<unsigned int>(100 + i);
    }

    billiardgl::applyBallTexturesToState(resources, state);

    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        assert(state.balls[i].texture == static_cast<unsigned int>(100 + i));
    }

    return 0;
}
