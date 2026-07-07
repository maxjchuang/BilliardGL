#include "platform_scroll.h"

#import <Cocoa/Cocoa.h>

#include <cmath>

namespace billiardgl {

namespace {

ScrollHandler g_scrollHandler = NULL;
id g_scrollMonitor = nil;

}  // namespace

void installPlatformScrollHandler(ScrollHandler handler)
{
    g_scrollHandler = handler;
    if (g_scrollMonitor != nil) {
        return;
    }

    g_scrollMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                                           handler:^NSEvent* (NSEvent* event) {
        const CGFloat deltaY = [event scrollingDeltaY];
        if (g_scrollHandler != NULL && std::fabs(static_cast<double>(deltaY)) >= 0.01) {
            g_scrollHandler(deltaY > 0.0 ? 1 : -1);
        }
        return event;
    }];
}

}  // namespace billiardgl
