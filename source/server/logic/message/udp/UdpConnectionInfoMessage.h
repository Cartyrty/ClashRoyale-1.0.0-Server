#pragma once
// Where the client opens its UDP battle connection (sent over TCP before a PvP battle): the server's address and port,
// the session id that starts every datagram, and the nonce of the UDP RC4 encrypters (key: the game's RC4 key +
// nonce). Payload: VInt port, String host, Bytes session id, String nonce. Encoded by the library's own
// UdpConnectionInfoMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <string>
#include <vector>

namespace server {
class UdpConnectionInfoMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24112;

    void encode() override {
        // UdpConnectionInfoMessage: server port +0x44, server host String* +0x48, session id bytes +0x50 and their
        // length +0x58, nonce String +0x60 (a String member, not a pointer: set with String::operator=).
        constexpr int kServerPortOffset = 0x44;
        constexpr int kServerHostOffset = 0x48;
        constexpr int kSessionIdOffset = 0x50;
        constexpr int kSessionIdLengthOffset = 0x58;
        constexpr int kNonceOffset = 0x60;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeInt(kServerPortOffset, serverPort_);
        message.writeString(kServerHostOffset, serverHost_);
        message.writePointer(kSessionIdOffset, library::copyBytes(sessionId_));
        message.writeInt(kSessionIdLengthOffset, int(sessionId_.size()));
        const auto nonce = library::copyBytes(std::vector<std::uint8_t>(nonce_.begin(), nonce_.end()));
        library::call("__ZN6StringaSEPKc", {message.get() + kNonceOffset, nonce});
        library::release(nonce);
        stream_.writeBytesWithoutLength(message.encode());
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
