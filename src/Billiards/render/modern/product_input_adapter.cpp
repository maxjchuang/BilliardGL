#include "render/modern/product_input_adapter.h"

#include <algorithm>
#include <cmath>

namespace billiardgl::render {
namespace {

NormalizedInputEvent normalized(const char* device, const std::string& control,
    const char* trigger, double value = 0.0, double secondary = 0.0)
{
    return {device, control, trigger, value, secondary};
}

bool isDirectionalKey(const std::string& control)
{
    return control == "ArrowLeft" || control == "ArrowRight" ||
        control == "ArrowUp" || control == "ArrowDown";
}

bool isContinuousKeyboardControl(const std::string& control)
{
    return control == "W" || control == "A" || control == "S" ||
        control == "D";
}

bool isPanKey(const std::string& control)
{
    return control == "A" || control == "D";
}

std::string modifierControl(std::uint32_t modifiers)
{
    if ((modifiers & PlatformModifierShift) != 0) return "Shift";
    if ((modifiers & PlatformModifierControl) != 0) return "Control";
    if ((modifiers & PlatformModifierOption) != 0) return "Option";
    if ((modifiers & PlatformModifierCommand) != 0) return "Command";
    return {};
}

}  // namespace

void ProductInputAdapter::updatePointer(const PlatformInputEvent& event)
{
    const double scale = std::isfinite(event.backingScale) &&
            event.backingScale > 0.0 ? event.backingScale : 1.0;
    pointer_.logicalX = event.logicalX;
    pointer_.logicalY = event.logicalY;
    pointer_.backingX = event.logicalX * scale;
    pointer_.backingY = event.logicalY * scale;
    pointer_.backingScale = scale;
}

ProductInputAdaptation ProductInputAdapter::adapt(
    const PlatformInputEvent& event)
{
    ProductInputAdaptation result;
    if (event.kind == PlatformInputKind::PointerMove ||
        event.kind == PlatformInputKind::PointerButtonDown ||
        event.kind == PlatformInputKind::PointerButtonUp)
        updatePointer(event);
    result.pointer = pointer_;

    switch (event.kind) {
    case PlatformInputKind::KeyDown:
    case PlatformInputKind::KeyUp: {
        if (event.control.empty()) return result;
        const bool down = event.kind == PlatformInputKind::KeyDown;
        result.events.push_back(normalized("keyboard", event.control,
            down && isContinuousKeyboardControl(event.control) ?
                "continuous" : down ? "press" : "release",
            down ? 1.0 : 0.0));
        if (down && isDirectionalKey(event.control)) {
            const double horizontal = event.control == "ArrowLeft" ? -1.0 :
                event.control == "ArrowRight" ? 1.0 : 0.0;
            const double vertical = event.control == "ArrowUp" ? 1.0 :
                event.control == "ArrowDown" ? -1.0 : 0.0;
            result.events.push_back(normalized("keyboard", "ArrowKeys",
                "continuous", horizontal, vertical));
        }
        if (down && isPanKey(event.control)) {
            result.events.push_back(normalized("keyboard", "A/D",
                "continuous", event.control == "A" ? -1.0 : 1.0, 0.0));
        }
        result.handled = true;
        break;
    }
    case PlatformInputKind::Scroll:
        if (!std::isfinite(event.scrollY) || event.scrollY == 0.0) return result;
        result.events.push_back(normalized("mouse",
            event.scrollY > 0.0 ? "WheelUp" : "WheelDown", "step",
            std::max(1.0, std::fabs(event.scrollY))));
        result.handled = true;
        break;
    case PlatformInputKind::PointerMove: {
        std::string control = "MoveX";
        if (event.leftButtonDown) {
            control = (event.modifiers & PlatformModifierShift) != 0 ?
                "Shift+LeftDrag" : "LeftDrag";
        } else if (event.rightButtonDown) {
            control = "RightDrag";
        }
        result.events.push_back(normalized("mouse", control, "continuous",
            event.deltaX, event.deltaY));
        result.handled = true;
        break;
    }
    case PlatformInputKind::PointerButtonDown:
    case PlatformInputKind::PointerButtonUp: {
        if (event.control != "LeftButton" && event.control != "RightButton")
            return result;
        result.events.push_back(normalized("mouse", event.control,
            event.kind == PlatformInputKind::PointerButtonDown ?
                "press" : "release",
            pointer_.backingX, pointer_.backingY));
        result.handled = true;
        break;
    }
    case PlatformInputKind::ModifierChanged: {
        const std::string control = event.control.empty() ?
            modifierControl(event.modifiers) : event.control;
        if (control.empty()) return result;
        const bool active = control == "Shift" ?
                (event.modifiers & PlatformModifierShift) != 0 :
            control == "Control" ?
                (event.modifiers & PlatformModifierControl) != 0 :
            control == "Option" ?
                (event.modifiers & PlatformModifierOption) != 0 :
                (event.modifiers & PlatformModifierCommand) != 0;
        result.events.push_back(normalized("keyboard", control,
            active ? "press" : "release"));
        result.handled = true;
        break;
    }
    }
    return result;
}

}  // namespace billiardgl::render
