#pragma once
// One client's TCP connection: its socket, its Messaging (framing and encryption) and its MessageManager (what the
// server does with the client's messages). The ServerApplication's loop owns it.
#include "titan/Messaging.h"

#include <cstdint>
#include <memory>
#include <string>

namespace server {
class GameServer;
class MessageManager;

class Connection {
public:
    Connection(std::uintptr_t socket, std::string peer, GameServer &server);
    ~Connection();
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    void sendMessage(PiranhaMessage &message);
    // Reads what the socket has and hands every complete message to the MessageManager.
    void readMessages();
    void close();
    bool isClosed() const { return closed_; }

    std::uintptr_t getSocket() const { return socket_; }
    const std::string &getPeer() const { return peer_; }
    std::string getLocalAddress() const; // the server address this client reached
    MessageManager &getMessageManager() { return *messageManager_; }

private:
    std::uintptr_t socket_;
    std::string peer_;
    bool closed_ = false;
    Messaging messaging_;
    std::unique_ptr<MessageManager> messageManager_;
};
} // namespace server
