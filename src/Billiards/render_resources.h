#pragma once

#include "game_state.h"
#include "ObjLoader.h"

#include <array>
#include <memory>
#include <string>

class emitter;

namespace billiardgl {

struct RenderResources {
    std::array<unsigned int, kBallCount> ballTextures = {};

    unsigned int tableVertexVBO = 0;
    unsigned int cueVertexVBO = 0;
    unsigned int benchVertexVBO = 0;
    unsigned int wardVertexVBO = 0;

    unsigned int ceilingTexture = 0;
    unsigned int blackTexture = 0;
    unsigned int wardTexture = 0;
    unsigned int flameTexture = 0;
    unsigned int tableTextures[2] = {0, 0};
    unsigned int cueTextures[2] = {0, 0};

    std::unique_ptr<ObjLoader> tableObj;
    std::unique_ptr<ObjLoader> cueObj;
    std::unique_ptr<ObjLoader> benchObj;
    std::unique_ptr<ObjLoader> wardObj;

    std::array<emitter*, kBallCount> emitters = {};
    std::array<bool, kBallCount> fired = {};
    bool allFired = false;

    float cameraEye[3] = {0.0f, 0.0f, 0.0f};
    float cameraTarget[3] = {0.0f, 0.0f, 0.0f};
    float cueTipOffsetCm = 0.0f;
    float shotPower = 0.0f;
    bool showCue = false;
    bool showPowerMeter = false;
    int viewportWidth = 1024;
    int viewportHeight = 768;
};

bool initializeRenderResources(RenderResources& resources, GameState& state);
void destroyRenderResources(RenderResources& resources);
unsigned int uploadTexture(const std::string& path);
void applyBallTexturesToState(const RenderResources& resources, GameState& state);
bool hasAllBallTextures(const RenderResources& resources);
bool hasRequiredRenderResources(const RenderResources& resources);
void toggleFired(RenderResources& resources, int index);
void toggleAllFired(RenderResources& resources);

}  // namespace billiardgl
