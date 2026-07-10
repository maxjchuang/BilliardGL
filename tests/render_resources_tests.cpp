#include "game_state.h"
#include "render_resources.h"
#include "assets.h"
#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* expression)
{
    if (!condition) {
        std::cerr << "Expectation failed: " << expression << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void expect(bool condition)
{
    expect(condition, "condition");
}

}  // namespace

#include <vector>

int main()
{
    billiardgl::GameState state;
    billiardgl::RenderResources resources;

    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        resources.ballTextures[i] = static_cast<unsigned int>(100 + i);
    }

    billiardgl::applyBallTexturesToState(resources, state);
    expect(billiardgl::hasAllBallTextures(resources));

    for (int i = 0; i < billiardgl::kBallCount; ++i) {
        expect(state.balls[i].texture == static_cast<unsigned int>(100 + i));
    }

    resources.ballTextures[3] = 0;
    expect(!billiardgl::hasAllBallTextures(resources));
    expect(!billiardgl::hasRequiredRenderResources(resources));

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

    expect(billiardgl::hasRequiredRenderResources(resources));

    const int tableTextureIndex = resources.tableObj->mtlIndex[1];
    resources.tableObj->mtlIndex[1] = 2;
    expect(!billiardgl::hasRequiredRenderResources(resources));
    resources.tableObj->mtlIndex[1] = tableTextureIndex;

    const std::vector<int> cueMaterialRanges = resources.cueObj->mtlIndex;
    resources.cueObj->mtlIndex.clear();
    expect(!billiardgl::hasRequiredRenderResources(resources));
    resources.cueObj->mtlIndex = cueMaterialRanges;

    billiardgl::RenderResources emptyResources;
    billiardgl::destroyRenderResources(emptyResources);
    expect(emptyResources.tableVertexVBO == 0);
    expect(emptyResources.cueVertexVBO == 0);
    expect(emptyResources.benchVertexVBO == 0);
    expect(emptyResources.wardVertexVBO == 0);
    expect(emptyResources.ceilingTexture == 0);
    expect(emptyResources.blackTexture == 0);
    expect(emptyResources.wardTexture == 0);
    expect(emptyResources.flameTexture == 0);

    billiardgl::RenderResources incompleteResources;
    incompleteResources.tableObj.reset(new ObjLoader("/tmp/billiardgl-missing-render-resource.obj"));
    expect(!billiardgl::hasRequiredRenderResources(incompleteResources));

    return 0;
}
