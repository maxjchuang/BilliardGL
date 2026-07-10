#include "game_state.h"
#include "render_resources.h"
#include "assets.h"

#include <cassert>

int main()
{
    billiardgl::GameState state;
    billiardgl::RenderResources resources;

    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        resources.ballTextures[i] = static_cast<unsigned int>(100 + i);
    }

    billiardgl::applyBallTexturesToState(resources, state);
    assert(billiardgl::hasAllBallTextures(resources));

    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        assert(state.balls[i].texture == static_cast<unsigned int>(100 + i));
    }

    resources.ballTextures[3] = 0;
    assert(!billiardgl::hasAllBallTextures(resources));
    assert(!billiardgl::hasRequiredRenderResources(resources));

    resources.ballTextures[3] = 103;
    resources.tableTextures[0] = 1;
    resources.tableTextures[1] = 2;
    resources.cueTextures[0] = 3;
    resources.cueTextures[1] = 4;
    resources.ceilingTexture = 5;
    resources.blackTexture = 6;
    resources.wardTexture = 7;
    resources.flameTexture = 8;
    resources.tableVertexVBO = 9;
    resources.cueVertexVBO = 10;
    resources.benchVertexVBO = 11;
    resources.wardVertexVBO = 12;
    resources.tableObj.reset(new ObjLoader(billiardgl::getObjectPath("table.obj")));
    resources.cueObj.reset(new ObjLoader(billiardgl::getObjectPath("cue.obj")));
    resources.benchObj.reset(new ObjLoader(billiardgl::getObjectPath("bench.obj")));
    resources.wardObj.reset(new ObjLoader(billiardgl::getObjectPath("wardrobe.obj")));

    assert(billiardgl::hasRequiredRenderResources(resources));

    billiardgl::RenderResources emptyResources;
    billiardgl::destroyRenderResources(emptyResources);
    assert(emptyResources.tableVertexVBO == 0);
    assert(emptyResources.cueVertexVBO == 0);
    assert(emptyResources.benchVertexVBO == 0);
    assert(emptyResources.wardVertexVBO == 0);
    assert(emptyResources.ceilingTexture == 0);
    assert(emptyResources.blackTexture == 0);
    assert(emptyResources.wardTexture == 0);
    assert(emptyResources.flameTexture == 0);

    billiardgl::RenderResources incompleteResources;
    incompleteResources.tableObj.reset(new ObjLoader("/tmp/billiardgl-missing-render-resource.obj"));
    assert(!billiardgl::hasRequiredRenderResources(incompleteResources));

    return 0;
}
