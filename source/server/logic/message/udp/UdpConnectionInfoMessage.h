#pragma once
// Where the client opens its UDP battle connection (sent over TCP before a PvP battle): the server's address and port,
// the session id that starts every datagram, and the nonce of the UDP RC4 encrypters (key: the game's RC4 key +
// nonce). Payload: VInt port, String host, Bytes session id, String nonce.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <string>
#include <vector>

namespace server {
class UdpConnectionInfoMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24112;

    void encode() override {
        stream_.writeVInt(serverPort_);
        stream_.writeString(serverHost_);
        stream_.writeBytes(sessionId_);
        stream_.writeStringReference(nonce_);
    }
    int getMessageType() const override { return kMessageType; }

    void setServerHost(const std::string &host) { serverHost_ = host; } // a dotted IPv4 address: the client does no DNS
    void setServerPort(int port) { serverPort_ = port; }
    void setSessionId(const std::vector<std::uint8_t> &sessionId) { sessionId_ = sessionId; }
    void setNonce(const std::string &nonce) { nonce_ = nonce; }

private:
    int serverPort_ = 0;
    std::string serverHost_;
    std::vector<std::uint8_t> sessionId_;
    std::string nonce_;
};
} // namespace server
