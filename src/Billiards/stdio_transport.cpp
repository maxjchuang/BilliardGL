#include "stdio_transport.h"
namespace billiardgl { TransportReadResult StdioTransport::readMessage(){TransportReadResult r;if(!std::getline(input_,r.message)){r.eof=true;return r;}if(r.message.size()>1024*1024){r.error="message too large";return r;}r.ok=true;return r;}bool StdioTransport::writeMessage(const std::string&m){output_<<m<<'\n';output_.flush();return static_cast<bool>(output_);} }
