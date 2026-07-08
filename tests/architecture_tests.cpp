#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string readFile(const std::string& path)
{
    std::ifstream input(path.c_str());
    assert(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void assertNotContains(const std::string& content, const std::string& needle)
{
    assert(content.find(needle) == std::string::npos);
}

void assertContains(const std::string& content, const std::string& needle)
{
    assert(content.find(needle) != std::string::npos);
}

}

int main()
{
    const std::string billiards = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/billiards.cpp");
    const std::string renderer = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/renderer.cpp");
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

    return 0;
}
