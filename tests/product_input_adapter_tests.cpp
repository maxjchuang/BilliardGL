#include "render/modern/product_input_adapter.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool value, const std::string& message)
{
    if (value) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

bool near(double lhs, double rhs)
{
    return std::fabs(lhs - rhs) < 1.0e-9;
}

}  // namespace

int main()
{
    using namespace billiardgl::render;
    ProductInputAdapter adapter;
    ProductPointerOwnership ownership;
    PlatformInputEvent down;
    down.kind = PlatformInputKind::PointerButtonDown;
    down.control = "LeftButton";
    PlatformInputEvent drag = down;
    drag.kind = PlatformInputKind::PointerMove;
    drag.leftButtonDown = true;
    PlatformInputEvent up = down;
    up.kind = PlatformInputKind::PointerButtonUp;
    expect(!ownership.accept(down, true, true, false) &&
        !ownership.accept(drag, true, true, true) &&
        !ownership.accept(up, true, true, true),
        "titlebar drag cannot turn into gameplay after entering the content");
    expect(ownership.accept(down, true, true, true) &&
        ownership.accept(drag, true, true, true) &&
        ownership.accept(up, true, true, true), "content drag retains normal interaction");
    for (int cancellation = 0; cancellation < 3; ++cancellation) {
        expect(ownership.accept(down, true, true, true), "capture before cancellation");
        expect(!ownership.accept(drag, cancellation != 0, cancellation != 1,
            cancellation != 2), "other window, lost focus or content exit cancels capture");
        expect(!ownership.accept(drag, true, true, true) &&
            !ownership.accept(up, true, true, true),
            "returning to content cannot resume a cancelled gesture or shoot");
    }
    expect(ownership.accept(down, true, true, true), "capture before native resize notification");
    ownership.cancel();
    expect(!ownership.accept(up, true, true, true), "resize cancellation discards release");

    PlatformInputEvent key;
    key.kind = PlatformInputKind::KeyDown;
    key.control = "F10";
    auto result = adapter.adapt(key);
    expect(result.handled && result.events.size() == 1 &&
        result.events[0].device == "keyboard" &&
        result.events[0].control == "F10" &&
        result.events[0].trigger == "press",
        "key down normalizes to a product press");

    key.kind = PlatformInputKind::KeyUp;
    result = adapter.adapt(key);
    expect(result.events.size() == 1 && result.events[0].trigger == "release",
        "key up remains observable for rebind and future hold actions");

    key.kind = PlatformInputKind::KeyDown;
    key.control = "ArrowLeft";
    result = adapter.adapt(key);
    expect(result.events.size() == 2 &&
        result.events[0].control == "ArrowLeft" &&
        result.events[0].trigger == "press" &&
        result.events[1].control == "ArrowKeys" &&
        result.events[1].trigger == "continuous" &&
        result.events[1].value == -1.0 && result.events[1].secondaryValue == 0.0,
        "directional key preserves shell identity and gameplay axis");

    key.control = "D";
    result = adapter.adapt(key);
    expect(result.events.size() == 2 &&
        result.events[1].control == "A/D" &&
        result.events[1].trigger == "continuous" &&
        result.events[1].value == 1.0,
        "pan keys retain rebind identity and emit the configured aggregate axis");

    PlatformInputEvent scroll;
    scroll.kind = PlatformInputKind::Scroll;
    scroll.scrollY = -2.5;
    result = adapter.adapt(scroll);
    expect(result.handled && result.events[0].control == "WheelDown" &&
        result.events[0].trigger == "step" && result.events[0].value == 2.5,
        "precise scrolling becomes a signed product wheel step");

    PlatformInputEvent move;
    move.kind = PlatformInputKind::PointerMove;
    move.logicalX = 120.25;
    move.logicalY = 44.5;
    move.deltaX = 3.0;
    move.deltaY = -4.0;
    move.backingScale = 2.0;
    move.leftButtonDown = true;
    move.modifiers = PlatformModifierShift;
    result = adapter.adapt(move);
    expect(result.handled && result.events[0].control == "Shift+LeftDrag" &&
        result.events[0].trigger == "continuous" &&
        result.events[0].value == 3.0 &&
        result.events[0].secondaryValue == -4.0,
        "shift-left drag is a stable chord rather than scattered native state");
    expect(near(result.pointer.logicalX, 120.25) &&
        near(result.pointer.logicalY, 44.5) &&
        near(result.pointer.backingX, 240.5) &&
        near(result.pointer.backingY, 89.0) &&
        near(result.pointer.backingScale, 2.0),
        "logical and backing-pixel pointer coordinates are both retained");

    move.backingScale = 1.0;
    const auto standardScale = adapter.adapt(move);
    expect(standardScale.events[0].value == result.events[0].value &&
        standardScale.events[0].secondaryValue == result.events[0].secondaryValue,
        "Retina backing scale must not amplify pointer motion sensitivity");

    PlatformInputEvent button;
    button.kind = PlatformInputKind::PointerButtonUp;
    button.control = "LeftButton";
    button.logicalX = 12.0;
    button.logicalY = 9.0;
    button.backingScale = 2.0;
    result = adapter.adapt(button);
    expect(result.handled && result.events[0].device == "mouse" &&
        result.events[0].control == "LeftButton" &&
        result.events[0].trigger == "release" &&
        result.events[0].value == 24.0 &&
        result.events[0].secondaryValue == 18.0,
        "mouse release carries high-DPI backing coordinates");

    PlatformInputEvent modifier;
    modifier.kind = PlatformInputKind::ModifierChanged;
    modifier.control = "Shift";
    modifier.modifiers = PlatformModifierShift;
    result = adapter.adapt(modifier);
    expect(result.handled && result.events[0].control == "Shift" &&
        result.events[0].trigger == "press",
        "modifier activation is independently bindable");
    modifier.modifiers = PlatformModifierNone;
    result = adapter.adapt(modifier);
    expect(result.events[0].trigger == "release",
        "modifier release is independently observable");

    PlatformInputEvent invalidScroll;
    invalidScroll.kind = PlatformInputKind::Scroll;
    result = adapter.adapt(invalidScroll);
    expect(!result.handled && result.events.empty(),
        "zero-delta scroll does not fabricate an input action");

    std::cout << "product-input-adapter-tests: pass\n";
    return 0;
}
