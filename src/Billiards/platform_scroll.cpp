#include "platform_scroll.h"

#include <GL/freeglut.h>

namespace billiardgl {

namespace {

ScrollHandler g_scrollHandler = NULL;

void handlePlatformMouseWheel(int wheel, int direction, int x, int y)
{
    (void)wheel;
    (void)x;
    (void)y;

    if (g_scrollHandler != NULL && direction != 0) {
        g_scrollHandler(direction > 0 ? 1 : -1);
    }
}

}  // namespace

void installPlatformScrollHandler(ScrollHandler handler)
{
    g_scrollHandler = handler;
    glutMouseWheelFunc(handlePlatformMouseWheel);
}

}  // namespace billiardgl
