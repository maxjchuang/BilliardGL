#include "platform_display_timing.h"

#include <OpenGL/OpenGL.h>

namespace billiardgl {

void enablePlatformVSync()
{
    CGLContextObj context = CGLGetCurrentContext();
    if (!context) return;
    const GLint interval = 1;
    CGLSetParameter(context, kCGLCPSwapInterval, &interval);
}

}  // namespace billiardgl
