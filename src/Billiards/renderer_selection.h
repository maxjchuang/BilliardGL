#pragma once

namespace billiardgl {

enum class RendererBackendSelection {
    Legacy,
    Modern
};

inline const char* rendererBackendSelectionName(RendererBackendSelection selection)
{
    switch (selection) {
    case RendererBackendSelection::Legacy:
        return "legacy";
    case RendererBackendSelection::Modern:
        return "modern";
    }
    return "unknown";
}

}  // namespace billiardgl
