#include "game/ServerApplication.h"

#include "game/GameServer.h"
#include "game/content/ContentHttpServer.h"
#include "game/content/ContentManager.h"
#include "game/network/Connection.h"
#include "game/network/MessageManager.h"
#include "platform/Socket.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>
#include <atomic>
#include <list>
#include <memory>

namespace server {
namespace ServerApplication {
namespace {
constexpr int kListenBacklog = 16;
// Battles advance in 50 ms ticks (10-tick turns); the loop wakes up often enough to keep their heartbeats on time.
constexpr int kSelectTimeoutMicroseconds = 10 * 1000;

std::atomic<bool> g_stopRequested{false};

platform::socket_t listenOn(const ServerConfig &config) {
    const platform::socket_t listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    platform::prepareListener(listener); // Windows: a second server on the port fails; Linux: restarts bind at once
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(std::uint16_t(config.port));
    inet_pton(AF_INET, config.host.c_str(), &address.sin_addr);
    if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof address) != 0 ||
        listen(listener, kListenBacklog) != 0)
        Debugger::error("cannot listen on %s:%d (error %d)", config.host.c_str(), config.port,
                        platform::lastNetErrorCode());
    platform::setNonBlocking(listener, true); // accept never blocks the loop
    return listener;
}

std::unique_ptr<Connection> acceptConnection(platform::socket_t listener, std::size_t connectionCount,
                                             GameServer &server) {
    sockaddr_in from{};
    platform::socklen fromLength = sizeof from;
    const platform::socket_t accepted = accept(listener, reinterpret_cast<sockaddr *>(&from), &fromLength);
    if (accepted == platform::kInvalidSocket) return nullptr;
    if (connectionCount >= FD_SETSIZE - 1) {
        platform::closeSocket(accepted);
        return nullptr;
    }
    platform::setNonBlocking(accepted, false); // Windows sockets inherit the listener's mode; connections block
    int noDelay = 1;
    setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&noDelay), sizeof noDelay);
    char text[64];
    inet_ntop(AF_INET, &from.sin_addr, text, sizeof text);
    auto connection = std::make_unique<Connection>(
        std::uintptr_t(accepted), std::string(text) + ":" + std::to_string(ntohs(from.sin_port)), server);
    Debugger::print("server", "connection from %s", connection->getPeer().c_str());
    return connection;
}
} // namespace

void requestStop() { g_stopRequested = true; }

int run(const ServerConfig &config) {
    platform::initSockets();
    GameServer server(config);
    auto &content = server.getContentManager();
    ContentHttpServer contentServer(config.resourceDirectory, config.dataDirectory, content.getSha());
    if (config.contentPort > 0 && content.isEnabled() && contentServer.listen(config.host, config.contentPort))
        content.setPort(config.contentPort);
    const auto listener = listenOn(config);
    Debugger::print("server", "listening on %s:%d (data in %s)", config.host.c_str(), config.port,
                    config.dataDirectory.c_str());
    server.startUdp();
    const auto &udpSocket = server.getUdpSocket();

    std::list<std::unique_ptr<Connection>> connections;
    while (!g_stopRequested) {
        fd_set readable;
        fd_set writable;
        FD_ZERO(&readable);
        FD_ZERO(&writable);
        contentServer.addSockets(readable, writable);
        FD_SET(listener, &readable);
        platform::socket_t highest = std::max(listener, contentServer.getHighestSocket());
        if (udpSocket.isOpen()) {
            FD_SET(udpSocket.getSocket(), &readable);
            highest = std::max(highest, udpSocket.getSocket());
        }
        for (const auto &connection : connections) {
            const auto socket = platform::socket_t(connection->getSocket());
            FD_SET(socket, &readable);
            highest = std::max(highest, socket);
        }
        timeval timeout{0, kSelectTimeoutMicroseconds};
        // nfds is ignored on Windows. On failure (EINTR: a signal such as Ctrl+C) the sets are undefined: clear them.
        if (select(int(highest) + 1, &readable, &writable, nullptr, &timeout) < 0) {
            FD_ZERO(&readable);
            FD_ZERO(&writable);
        }
        contentServer.service(readable, writable);
        if (FD_ISSET(listener, &readable))
            if (auto connection = acceptConnection(listener, connections.size(), server))
                connections.push_back(std::move(connection));
        for (auto &connection : connections)
            if (!connection->isClosed() && FD_ISSET(platform::socket_t(connection->getSocket()), &readable))
                connection->readMessages();
        if (udpSocket.isOpen() && FD_ISSET(udpSocket.getSocket(), &readable)) server.receiveUdpPackets();
        server.update();
        for (auto connection = connections.begin(); connection != connections.end();) {
            if (!(*connection)->isClosed()) {
                ++connection;
                continue;
            }
            Debugger::print("server", "%s disconnected", (*connection)->getPeer().c_str());
            (*connection)->getMessageManager().onDisconnected();
            connection = connections.erase(connection);
        }
    }
    // Every player saves as on a disconnect; the GameServer closes the database on the way out.
    Debugger::print("server", "stopping");
    for (auto &connection : connections) {
        connection->close();
        connection->getMessageManager().onDisconnected();
    }
    connections.clear();
    platform::closeSocket(listener);
    return 0;
}
} // namespace ServerApplication
} // namespace server
