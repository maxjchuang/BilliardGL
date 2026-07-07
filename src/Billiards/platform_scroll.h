#pragma once

namespace billiardgl {

typedef void (*ScrollHandler)(int direction);

void installPlatformScrollHandler(ScrollHandler handler);

}  // namespace billiardgl
