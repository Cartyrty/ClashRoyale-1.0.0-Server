#pragma once
// The server's side of the client's UdpSocket: one non-blocking UDP socket on the game port that every battle
// session shares. Datagrams are the 10-byte session id, then the packet. (The client pads its packets to the full 1400
// bytes until the server has answered; the server only sends once it has heard from the client, at the packet's size.)
#include "platform/Socket.h"
#include "titan/logic/message/udp/UdpPacket.h"

#include <cstdint>
#include <string>
#include <vector>

namespace server {
class UdpSocket {
public:
    struct Datagram {
        sockaddr_in from{};
        std::vector<std::uint8_t> sessionId;
        std::vector<std::uint8_t> packet;
    };

    UdpSocket() = default;
    UdpSocket(const UdpSocket &) = delete;
    UdpSocket &operator=(const UdpSocket &) = delete;
    ~UdpSocket() { close(); }

    bool bind(const std::string &host, int port);
    void close();
    bool isOpen() const { return socket_ != platform::kInvalidSocket; }
    platform::socket_t getSocket() const { return socket_; }

    // The next datagram; false when none is waiting. Datagrams of the wrong size are dropped with a warning.
    bool receive(Datagram &datagram);
    bool send(const sockaddr_in &to, const std::vector<std::uint8_t> &sessionId, const UdpPacket &packet);

private:
    bool sendDatagram(const sockaddr_in &to, const std::vector<std::uint8_t> &datagram);

    platform::socket_t socket_ = platform::kInvalidSocket;
};
} // namespace server
