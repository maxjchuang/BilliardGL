#pragma once
#include "automation_controller.h"
#include "automation_transport.h"
#include <functional>

namespace billiardgl {

int runAutomation(
    AutomationTransport& transport,
    AutomationController& controller,
    const std::string& mode,
    const std::string& rendererBackend,
    const std::function<bool(const std::string&)>& screenshot =
        std::function<bool(const std::string&)>(),
    const json::Value::Object& readyMetadata = json::Value::Object(),
    const std::function<json::Value(int, int)>& renderPerformance =
        std::function<json::Value(int, int)>());

}  // namespace billiardgl
