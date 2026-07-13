#pragma once
#include "automation_controller.h"
#include "automation_transport.h"
#include <functional>
namespace billiardgl { int runAutomation(AutomationTransport&,AutomationController&,const std::string& mode,const std::function<bool(const std::string&)>& screenshot=std::function<bool(const std::string&)>()); }
