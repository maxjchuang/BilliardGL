#include "automation_runner.h"

#include <exception>

namespace billiardgl {

int runAutomation(
    AutomationTransport& transport,
    AutomationController& controller,
    const std::string& mode,
    const std::string& rendererBackend,
    const std::function<bool(const std::string&)>& screenshot,
    const json::Value::Object& readyMetadata,
    const std::function<json::Value(int, int)>& renderPerformance)
{
    json::Value ready = automationReadyEvent(
        mode, "stdio", controller.capabilities());
    // Renderer selection is presentation metadata. Keep it outside the
    // frozen automation protocol schema bound by the physics evidence.
    ready["renderer_backend"] = json::Value(rendererBackend);
    for (const auto& field : readyMetadata) ready[field.first] = field.second;
    if (!transport.writeMessage(json::stringify(ready))) {
        return 2;
    }
    while (true) {
        TransportReadResult input = transport.readMessage();
        if (input.eof) return 0;
        if (!input.ok) {
            transport.writeMessage(json::stringify(
                automationProtocolError("transport_error", input.error)));
            continue;
        }
        json::ParseResult parsed = json::parse(input.message);
        if (!parsed.ok) {
            transport.writeMessage(json::stringify(
                automationProtocolError("parse_error", parsed.error)));
            continue;
        }
        AutomationRequestResult request = parseAutomationRequest(parsed.value);
        if (!request.ok) {
            int id = 0;
            try {
                if (parsed.value.has("id")) id = parsed.value.at("id").asInt();
            } catch (...) {
            }
            transport.writeMessage(json::stringify(automationErrorResponse(
                id, request.errorCode, request.errorMessage)));
            continue;
        }
        ControllerResult result = controller.handle(request.request);
        if (result.renderPerformanceRequested) {
            if (!renderPerformance) {
                result.response = automationErrorResponse(
                    request.request.id, "render_performance_unavailable",
                    "render performance callback is unavailable");
            } else {
                try {
                    result.response = automationSuccessResponse(
                        request.request.id,
                        renderPerformance(result.performanceWarmupFrames,
                                          result.performanceSampleFrames));
                } catch (const std::exception& error) {
                    result.response = automationErrorResponse(
                        request.request.id, "render_performance_failed",
                        error.what());
                }
            }
        }
        if (result.screenshotRequested &&
            (!screenshot || !screenshot(result.screenshotPath))) {
            result.response = automationErrorResponse(
                request.request.id, "screenshot_failed", "failed to save framebuffer");
        }
        if (!transport.writeMessage(json::stringify(result.response))) return 2;
        if (result.quitRequested) return 0;
    }
}

}  // namespace billiardgl
