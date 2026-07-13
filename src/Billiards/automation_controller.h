#pragma once

#include "automation_protocol.h"

namespace billiardgl {

enum class AutomationMode { Headless, Rendered };

struct ControllerResult {
    json::Value response = json::Value::object();
    bool quitRequested = false;
    bool screenshotRequested = false;
    std::string screenshotPath;
};

class AutomationController {
public:
    AutomationController(GameRuntime& runtime, AutomationMode mode) : runtime_(runtime), mode_(mode) {}
    ControllerResult handle(const AutomationRequest& request);
    std::vector<std::string> capabilities() const;

private:
    ControllerResult success(int id, const json::Value& result = json::Value::object()) const;
    ControllerResult failure(int id, const std::string& code, const std::string& message) const;
    GameRuntime& runtime_;
    AutomationMode mode_;
};

}  // namespace billiardgl
