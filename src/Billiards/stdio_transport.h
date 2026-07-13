#pragma once
#include "automation_transport.h"
#include <istream>
#include <ostream>
namespace billiardgl { class StdioTransport:public AutomationTransport{public:StdioTransport(std::istream&i,std::ostream&o):input_(i),output_(o){}TransportReadResult readMessage()override;bool writeMessage(const std::string&)override;private:std::istream&input_;std::ostream&output_;}; }
