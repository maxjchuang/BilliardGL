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

void assertDisplayCallbackPrecedesEveryRenderInitialization(const std::string& content)
{
    std::size_t initialization = 0;
    while ((initialization = content.find("initializeRenderResources", initialization)) != std::string::npos) {
        const std::size_t window = content.rfind("initWindows();", initialization);
        const std::size_t callback = content.rfind("glutDisplayFunc(&myDisplay);", initialization);
        expect(window != std::string::npos, "render initialization should follow window creation");
        expect(callback != std::string::npos && callback > window,
            "display callback should be registered after window creation and before render initialization");
        initialization += 1;
    }
}

}

int main()
{
    const std::string billiards = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/billiards.cpp");
    const std::string renderer = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/renderer.cpp");
    const std::string renderResources = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/render_resources.cpp");
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
    assertContains(billiards, "billiardgl::handleMouseMove(Game, x, y)");
    assertContains(billiards, "billiardgl::handleMouseWheel(Game");
    assertContains(billiards, "if (mbutton == 3 || mbutton == 4)");
    assertContains(billiards, "case '+':");
    assertContains(billiards, "case '-':");
    assertContains(billiards, "billiardgl::handleCameraAnchorToggleKey(Game)");
    assertContains(billiards, "billiardgl::handleCameraReturnToCueBallKey(Game)");
    assertContains(billiards, "billiardgl::beginCameraPan(Game, x, y)");
    assertContains(billiards, "Game.camera.anchorMode = billiardgl::CameraAnchorMode::FreeLook;");
    assertContains(billiards, "Game.transitionPerspective = false;");
    assertContains(billiards, "billiardgl::installPlatformScrollHandler");
    assertContains(billiards, "Render.showPowerMeter = Game.aim.mode == billiardgl::AimMode::Aim");
    assertDisplayCallbackPrecedesEveryRenderInitialization(billiards);
    assertContains(cmake, "platform_scroll.cpp");
    assertContains(cmake, "platform_scroll_mac.mm");
    assertContains(platformScroll, "glutMouseWheelFunc");
    assertContains(platformScroll, "handlePlatformMouseWheel");
    assertContains(macScroll, "addLocalMonitorForEventsMatchingMask");
    assertContains(macScroll, "NSEventMaskScrollWheel");
    assertContains(macScroll, "scrollingDeltaY");
    assertNotContains(renderer, "const GLfloat meterLeft = 18.0f");
    assertContains(renderer, "static_cast<GLfloat>(resources.viewportWidth) - 54.0f");
    assertNotContains(renderer, "resources.cameraEye[0] - resources.cameraTarget[0]");
    assertNotContains(renderer, "resources.cameraEye[2] - resources.cameraTarget[2]");
    assertNotContains(renderer, "state.camera.angleX * 180.0f");
    assertNotContains(renderResources, "gluBuild2DMipmaps");
    assertNotContains(renderResources, "<OpenGL/glu.h>");
    assertNotContains(renderResources, "<GL/glu.h>");
    assertProjectTestDoesNotUseAssert("tests/input_tests.cpp");
    assertProjectTestDoesNotUseAssert("tests/shot_tests.cpp");
    assertProjectTestDoesNotUseAssert("tests/render_resources_tests.cpp");
    assertProjectTestDoesNotUseAssert("tests/architecture_tests.cpp");
    assertNotContains(cmake, "/tmp/billiardgl-ctest");
    const std::string screenshotTests = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/tests/screenshot_tests.cpp");
    assertNotContains(screenshotTests, "/tmp/billiardgl-screenshot-test.ppm");

    return 0;
}
