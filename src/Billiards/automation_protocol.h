#pragma once

#include "automation_json.h"
#include "game_runtime.h"

#include <string>
#include <vector>

namespace billiardgl {

constexpr int kAutomationProtocolVersion = 1;

struct AutomationRequest {
    int id = 0;
    int version = 0;
    std::string command;
    json::Value params = json::Value::object();
};

struct AutomationRequestResult {
    bool ok = false;
    AutomationRequest request;
    std::string errorCode;
    std::string errorMessage;
};

AutomationRequestResult parseAutomationRequest(const json::Value& value);
json::Value automationSuccessResponse(int id, const json::Value& result);
json::Value automationErrorResponse(int id, const std::string& code, const std::string& message);
json::Value automationProtocolError(const std::string& code, const std::string& message);
json::Value automationReadyEvent(const std::string& mode, const std::string& transport,
    const std::vector<std::string>& capabilities);
json::Value serializeAutomationState(const GameRuntime& runtime);
json::Value serializeRuntimeEvent(const RuntimeEvent& event);

}  // namespace billiardgl
