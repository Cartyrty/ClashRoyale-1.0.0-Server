#include "game/network/Connection.h"

#include "game/network/MessageManager.h"
#include "logic/message/account/KeepAliveMessage.h"
#include "logic/message/account/KeepAliveServerMessage.h"
#include "platform/Socket.h"
#include "titan/logic/debug/Debugger.h"

#include <chrono>
#include <thread>

namespace server {
namespace {
constexpr std::size_t kReceiveBufferSize = 1 << 16;
} // namespace

Connection::Connection(std::uintptr_t socket, std::string peer, GameServer &server)
    : socket_(socket), peer_(std::move(peer)), messageManager_(std::make_unique<MessageManager>(*this, server)) {}

Connection::~Connection() { close(); }

void Connection::close() {
    if (closed_) return;
    closed_ = true;
    platform::closeSocket(platform::socket_t(socket_));
}

// The socket blocks; a full send buffer (WouldBlock) is waited out.
void Connection::sendMessage(PiranhaMessage &message) {
    if (closed_) return;
    const auto frame = messaging_.encodeFrame(message);
    std::size_t sent = 0;
    while (sent < frame.size()) {
        const int n = int(::send(platform::socket_t(socket_), reinterpret_cast<const char *>(frame.data() + sent),
                                 int(frame.size() - sent), platform::kNoSignal));
        if (n < 0) {
            if (platform::lastNetError() == platform::NetError::WouldBlock) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            close();
            return;
        }
        sent += std::size_t(n);
    }
    if (message.getMessageType() != KeepAliveServerMessage::kMessageType)
        Debugger::print("msg", "%s <- %d (%zu bytes)", peer_.c_str(), message.getMessageType(),
                        frame.size() - Messaging::kHeaderSize);
}

void Connection::readMessages() {
    static std::vector<std::uint8_t> buffer(kReceiveBufferSize);
    const int n =
        int(recv(platform::socket_t(socket_), reinterpret_cast<char *>(buffer.data()), int(buffer.size()), 0));
    if (n <= 0) {
        close();
        return;
    }
    for (auto &message : messaging_.receive(buffer.data(), std::size_t(n))) {
        if (message->getMessageType() != KeepAliveMessage::kMessageType)
            Debugger::print("msg", "%s -> %d (%zu bytes)", peer_.c_str(), message->getMessageType(),
                            message->getByteStream().getByteArray().size());
        messageManager_->receiveMessage(*message);
        if (closed_) break;
    }
}

std::string Connection::getLocalAddress() const {
    sockaddr_in local{};
    platform::socklen length = sizeof local;
    if (getsockname(platform::socket_t(socket_), reinterpret_cast<sockaddr *>(&local), &length) != 0)
        return "127.0.0.1";
    char text[64];
    inet_ntop(AF_INET, &local.sin_addr, text, sizeof text);
    return text;
}
} // namespace server
