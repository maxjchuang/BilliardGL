#pragma once

namespace billiardgl {

enum class ActionType {
    KeyDown,
    KeyUp,
    SpecialKey,
    MouseMove,
    MouseButton,
    MouseWheel,
    Resize,
    ToggleAim,
    SetAimYaw,
    SetShotPower,
    Shoot,
    ToggleHelp,
    OrbitCamera,
    PanCamera,
    ZoomCamera
};

struct GameAction {
    ActionType type = ActionType::KeyDown;
    int firstInt = 0;
    int secondInt = 0;
    int thirdInt = 0;
    float first = 0.0f;
    float second = 0.0f;
    bool flag = false;
};

struct ActionResult {
    ActionResult() = default;
    ActionResult(bool success, const char* code) : ok(success), errorCode(code) {}
    bool ok = true;
    const char* errorCode = "";
};

}  // namespace billiardgl
