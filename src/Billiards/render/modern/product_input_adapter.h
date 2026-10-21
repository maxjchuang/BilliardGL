#pragma once

#include "product_input_controller.h"

#include <cstdint>
#include <string>
#include <vector>

namespace billiardgl::render {

enum class PlatformInputKind {
    KeyDown,
    KeyUp,
    Scroll,
    PointerMove,
    PointerButtonDown,
    PointerButtonUp,
    ModifierChanged,
};

enum PlatformModifier : std::uint32_t {
    PlatformModifierNone = 0,
    PlatformModifierShift = 1u << 0,
    PlatformModifierControl = 1u << 1,
    PlatformModifierOption = 1u << 2,
    PlatformModifierCommand = 1u << 3,
};

struct PlatformInputEvent {
    PlatformInputKind kind = PlatformInputKind::KeyDown;
    std::string control;
    std::uint32_t modifiers = PlatformModifierNone;
    double logicalX = 0.0;
    double logicalY = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    double scrollY = 0.0;
    double backingScale = 1.0;
    bool leftButtonDown = false;
    bool rightButtonDown = false;
};

struct ProductPointerSnapshot {
    double logicalX = 0.0;
    double logicalY = 0.0;
    double backingX = 0.0;
    double backingY = 0.0;
    double backingScale = 1.0;
};

// Native events only own a drag after a down in the game's content area.
// Window chrome, other windows and cancelled gestures never become gameplay.
struct ProductPointerOwnership {
    bool left = false;
    bool right = false;
    void cancel() noexcept { left = right = false; }
    bool accept(const PlatformInputEvent& event, bool sameWindow,
        bool focused, bool insideContent) noexcept {
        if (!sameWindow || !focused || !insideContent) {
            cancel();
            return false;
        }
        if (event.kind == PlatformInputKind::PointerButtonDown) {
            if (event.control == "LeftButton") left = true;
            if (event.control == "RightButton") right = true;
            return true;
        }
        if (event.kind == PlatformInputKind::PointerButtonUp) {
            bool& owned = event.control == "RightButton" ? right : left;
            const bool accepted = owned;
            owned = false;
            return accepted;
        }
        if (event.kind == PlatformInputKind::PointerMove)
            return (!event.leftButtonDown || left) &&
                (!event.rightButtonDown || right);
        return true;
    }
};

struct ProductInputAdaptation {
    bool handled = false;
    std::vector<NormalizedInputEvent> events;
    ProductPointerSnapshot pointer;
};

// Cocoa is intentionally kept out of this boundary. Native bridges provide a
// canonical key/button name and logical coordinates; this adapter supplies the
// stable product controls, triggers, drag chords, and backing-pixel evidence.
class ProductInputAdapter {
public:
    ProductInputAdaptation adapt(const PlatformInputEvent& event);
    const ProductPointerSnapshot& pointer() const noexcept { return pointer_; }

private:
    void updatePointer(const PlatformInputEvent& event);

    ProductPointerSnapshot pointer_;
};

}  // namespace billiardgl::render
