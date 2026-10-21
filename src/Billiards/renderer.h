#pragma once

#include "presentation_snapshot.h"

namespace billiardgl {

struct RenderResources;

void setupCameraFromSnapshot(const CameraPresentation& camera);
void setupLights();
void renderScene(const PresentationSnapshot& snapshot, RenderResources& resources);

}  // namespace billiardgl
