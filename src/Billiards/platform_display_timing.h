#pragma once

namespace billiardgl {

// Enables swap synchronization for the current OpenGL context. The GLUT idle
// callback can then submit frames at the display cadence without coupling that
// cadence to the fixed physics step.
void enablePlatformVSync();

}  // namespace billiardgl
