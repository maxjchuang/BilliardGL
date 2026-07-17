#include "platform_display_timing.h"

namespace billiardgl {

void enablePlatformVSync()
{
    // The non-Apple GLUT backends use the window-system swap policy. This
    // hook keeps the main loop display-driven and leaves room for explicit
    // GLX/WGL support without changing game or physics code.
}

}  // namespace billiardgl
