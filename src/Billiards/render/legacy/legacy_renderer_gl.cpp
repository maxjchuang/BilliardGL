#define GLEW_STATIC
#include <GL/glew.h>

#include "render/legacy/legacy_renderer.h"

#include "game_state.h"
#include "hud.h"
#include "particle_resources.h"
#include "render_resources.h"
#include "renderer.h"
#include "screenshot.h"

namespace billiardgl::render {

LegacyRendererOperations makeLegacyRendererOperations(GameState& resourceState)
{
    LegacyRendererOperations operations;
    operations.initializeResources = [&resourceState](
        RenderResources& resources,
        const RendererInitialization&,
        std::string& error) {
        if (!initializeRenderResources(resources, resourceState)) {
            error = "LegacyRenderer failed to initialize RenderResources";
            return false;
        }
        try {
            initializeParticleEmitters(resources, resourceState);
            setupLights();
        } catch (...) {
            destroyParticleEmitters(resources);
            destroyRenderResources(resources);
            throw;
        }
        return true;
    };
    operations.shutdownResources = [](RenderResources& resources) {
        destroyParticleEmitters(resources);
        destroyRenderResources(resources);
    };
    operations.resizeViewport = [](int width, int height) {
        glViewport(0, 0, width, height);
    };
    operations.setupCamera = [](const CameraPresentation& camera) {
        setupCameraFromSnapshot(camera);
    };
    operations.renderScene = [](
        const PresentationSnapshot& snapshot, RenderResources& resources) {
        billiardgl::renderScene(snapshot, resources);
    };
    operations.drawHud = [](const HudViewModel& hud) {
        billiardgl::drawHud(hud);
    };
    operations.captureOffscreen = [](
        const std::string& path, int width, int height,
        const std::function<void()>& renderFrame) {
        return captureOffscreenToPpm(path, width, height, renderFrame);
    };
    return operations;
}

}  // namespace billiardgl::render
