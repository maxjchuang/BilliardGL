#pragma once
#include <string>
namespace billiardgl { struct TransportReadResult{bool ok=false;bool eof=false;std::string message;std::string error;}; class AutomationTransport{public:virtual~AutomationTransport(){};virtual TransportReadResult readMessage()=0;virtual bool writeMessage(const std::string&)=0;}; }
