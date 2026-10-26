#pragma once
// The server's command line options.
#include <string>

namespace server {
struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 9339;
    std::string dataDirectory;
    std::string resourceDirectory; // the res folder the server runs on (also the content clients are updated to)
    int contentPort = 9340;        // the content update HTTP server (content.js uses 9340); 0 turns content updates off
    std::string contentUrl;        // overrides http://<address the client reached>:<contentPort>
    int udpPort = -1;              // UDP battles (UdpConnectionInfoMessage); -1: the game port, 0: off (TCP only)
    std::string udpHost;           // the IPv4 address clients send UDP to; default: the address the client reached
};
} // namespace server
