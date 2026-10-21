#include <fstream>
#include <sstream>
#include <string>
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

std::string readFile(const std::string& path)
{
    std::ifstream input(path.c_str());
    expect(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void assertNotContains(const std::string& content, const std::string& needle)
{
    expect(content.find(needle) == std::string::npos);
}

void assertContains(const std::string& content, const std::string& needle)
{
    expect(content.find(needle) != std::string::npos);
}

void assertProjectTestDoesNotUseAssert(const std::string& relativePath)
{
    const std::string content = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/" + relativePath);
    const std::string forbidden = std::string("assert") + "(";
    expect(content.find(forbidden) == std::string::npos);
}

}

int main()
{
    const std::string billiards = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/billiards.cpp");
    const std::string gameRuntime = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/game_runtime.cpp");
    const std::string renderer = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/renderer.cpp");
    const std::string rendererHeader = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/renderer.h");
    const std::string hud = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/hud.cpp");
    const std::string hudHeader = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/hud.h");
    const std::string presentationSnapshot = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/presentation_snapshot.h");
    const std::string renderResources = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/render_resources.cpp");
    const std::string rendererSelection = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/renderer_selection.h");
    const std::string physics = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/physics.cpp");
    const std::string physicsTelemetry = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/physics_telemetry.cpp");
    const std::string legacyRenderer = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/render/legacy/legacy_renderer.cpp");
    const std::string legacyRendererHeader = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/render/legacy/legacy_renderer.h");
    const std::string legacyRendererGl = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/render/legacy/legacy_renderer_gl.cpp");
    const std::string cmake = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/CMakeLists.txt");
    const std::string platformScroll = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/platform_scroll.cpp");
    const std::string macScroll = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/platform_scroll_mac.mm");

    assertNotContains(billiards, "GLuint texture[");
    assertNotContains(billiards, "emitter *e[");
    assertNotContains(billiards, "bool Fired[");
    assertNotContains(billiards, "bool AllFired");
    assertNotContains(billiards, "new emitter(");
    assertNotContains(billiards, "Game.camera.target[0] - Game.camera.eye[0]");
    assertNotContains(billiards, "Game.camera.target[2] - Game.camera.eye[2]");
    assertNotContains(billiards, "pow(Game.camera.target[0] - Game.camera.eye[0]");
    assertNotContains(billiards, "#define TABLE_IN_WIDTH 124.5");
    assertNotContains(billiards, "#define TABLE_IN_LENGTH 252");
    assertNotContains(billiards, "#define TABLE_HEIGHT 87");
    assertNotContains(billiards, "#define BALL_RADIUS 5.715");
    assertContains(billiards, "glutMotionFunc(mouseMove)");
    assertContains(billiards, "glutPassiveMotionFunc(mouseMove)");
    assertContains(billiards, "enablePlatformVSync()");
    assertContains(billiards, "glutIdleFunc(&myIdle)");
    assertNotContains(billiards, "glutTimerFunc");
    assertContains(billiards, "interactiveChinesePoolPhysicsProfile()");
    assertContains(billiards, "buildPresentationSnapshot(");
    assertContains(billiards, "billiardgl::render::runModernApplication(");
    expect(billiards.find("billiardgl::render::runModernApplication(") <
        billiards.find("glutInit(&glutArgc, glutArgv)"),
        "modern application dispatch must precede all rendered GLUT initialization");
    assertContains(billiards, "frameInterpolationAlpha(");
    assertNotContains(billiards, "sleepMilliseconds(1000)");
    assertContains(billiards, "telemetry.stepStatus == billiardgl::PhysicsStepStatus::Failed");
    assertContains(billiards, "billiardgl::physicsFailureCodeName(telemetry.failureCode)");
    assertContains(billiards, "billiardgl::handleMouseMove(Game, x, y)");
    assertContains(billiards, "billiardgl::handleMouseWheel(Game");
    assertContains(billiards, "if (mbutton == 3 || mbutton == 4)");
    assertContains(billiards, "case '+':");
    assertContains(billiards, "case '-':");
    assertContains(billiards, "billiardgl::handleCameraAnchorToggleKey(Game)");
    assertContains(billiards, "billiardgl::handleCameraReturnToCueBallKey(Game)");
    assertContains(billiards, "billiardgl::beginCameraPan(Game, x, y)");
    assertNotContains(billiards, "Game.camera.anchorMode = billiardgl::CameraAnchorMode::FreeLook;");
    assertContains(billiards, "billiardgl::applyCueShot(");
    assertContains(gameRuntime, "state.camera.anchorMode = CameraAnchorMode::FreeLook;");
    assertContains(billiards, "Game.transitionPerspective = false;");
    assertContains(billiards, "billiardgl::installPlatformScrollHandler");
    assertNotContains(billiards, "Render.showPowerMeter");
    assertContains(cmake, "platform_scroll.cpp");
    assertContains(cmake, "platform_scroll_mac.mm");
    assertContains(cmake, "render/modern/modern_application_mac.mm");
    assertContains(cmake, "render/legacy/legacy_renderer.cpp");
    assertContains(cmake, "BilliardsLegacyRendererCompatibilityTests");
    assertContains(platformScroll, "glutMouseWheelFunc");
    assertContains(platformScroll, "handlePlatformMouseWheel");
    assertContains(macScroll, "addLocalMonitorForEventsMatchingMask");
    assertContains(macScroll, "NSEventMaskScrollWheel");
    assertContains(macScroll, "scrollingDeltaY");
    assertNotContains(renderer, "const GLfloat meterLeft = 18.0f");
    assertContains(renderer, "static_cast<GLfloat>(hud.viewportWidth) - 54.0f");
    assertNotContains(renderer, "resources.cameraEye[0] - resources.cameraTarget[0]");
    assertNotContains(renderer, "resources.cameraEye[2] - resources.cameraTarget[2]");
    assertNotContains(renderer, "state.camera.angleX * 180.0f");
    assertContains(renderer, "tableObj.mtlIndex[6] - tableObj.mtlIndex[4]");
    assertContains(renderer, "tableObj.vertices.size() - tableObj.mtlIndex[6]");
    assertContains(renderer, "cueObj.vertices.size() - cueObj.mtlIndex[4]");
    assertContains(renderer, "cueObj.mtlIndex[4] - cueObj.mtlIndex[2]");
    assertNotContains(renderer, "glDrawArrays(GL_TRIANGLES, cueObj.mtlIndex[4], cueObj.vertices.size())");
    assertNotContains(renderResources, "gluBuild2DMipmaps");
    assertNotContains(renderResources, "<OpenGL/glu.h>");
    assertNotContains(renderResources, "<GL/glu.h>");
    assertContains(rendererHeader, "const PresentationSnapshot&");
    assertContains(rendererHeader, "const CameraPresentation&");
    assertContains(hudHeader, "const HudViewModel&");
    assertNotContains(rendererHeader, "game_state.h");
    assertNotContains(rendererHeader, "GameState");
    assertNotContains(rendererHeader, "physics.h");
    assertNotContains(rendererHeader, "mutableState");
    assertNotContains(renderer, "#include \"physics");
    assertNotContains(renderer, "mutableState");
    assertNotContains(renderer, "applyBallTexturesToState");
    assertNotContains(hudHeader, "game_state.h");
    assertNotContains(hudHeader, "GameState");
    assertNotContains(hudHeader, "physics.h");
    assertNotContains(hudHeader, "mutableState");
    assertNotContains(hud, "#include \"physics");
    assertNotContains(hud, "mutableState");
    assertNotContains(hud, "GameState");
    assertNotContains(presentationSnapshot, "game_state.h");
    assertNotContains(presentationSnapshot, "GameState");
    assertNotContains(presentationSnapshot, "physics.h");
    assertNotContains(presentationSnapshot, "solver");
    assertNotContains(presentationSnapshot, "mutableState");
    assertNotContains(presentationSnapshot, "dispatch(");
    assertContains(rendererSelection, "enum class RendererBackendSelection");
    assertContains(billiards, "modern backend unavailable in this build/path");
    assertNotContains(gameRuntime, "rendererBackend");
    assertNotContains(physics, "rendererBackend");
    assertNotContains(physicsTelemetry, "rendererBackend");
    assertContains(legacyRendererHeader, "public RendererBackend");
    assertContains(legacyRendererHeader, "const PresentationSnapshot& snapshot");
    assertNotContains(legacyRenderer, "GameState");
    assertNotContains(legacyRenderer, "mutableState");
    assertNotContains(legacyRenderer, "physics.h");
    assertContains(legacyRendererGl, "initializeRenderResources(resources, resourceState)");
    assertContains(legacyRendererGl, "destroyRenderResources(resources)");
    assertContains(legacyRendererGl, "setupCameraFromSnapshot(camera)");
    assertContains(legacyRendererGl, "billiardgl::renderScene(snapshot, resources)");
    assertContains(legacyRendererGl, "billiardgl::drawHud(hud)");
    assertContains(legacyRendererGl, "captureOffscreenToPpm(path, width, height, renderFrame)");
    assertProjectTestDoesNotUseAssert("tests/input_tests.cpp");
    assertProjectTestDoesNotUseAssert("tests/shot_tests.cpp");
    assertProjectTestDoesNotUseAssert("tests/render_resources_tests.cpp");
    assertProjectTestDoesNotUseAssert("tests/architecture_tests.cpp");
    assertNotContains(cmake, "/tmp/billiardgl-ctest");
    const std::string screenshotTests = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/tests/screenshot_tests.cpp");
    assertNotContains(screenshotTests, "/tmp/billiardgl-screenshot-test.ppm");

    return 0;
}
