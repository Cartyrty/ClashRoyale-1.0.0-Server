#include "titan/udp/UdpSocket.h"

#include "titan/logic/debug/Debugger.h"

namespace server {
namespace {
constexpr int kMaxDatagramsPerRead = 64; // one receive pass of the server loop
} // namespace

bool UdpSocket::bind(const std::string &host, int port) {
    close();
    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == platform::kInvalidSocket) return false;
    platform::prepareDatagramSocket(socket_);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(std::uint16_t(port));
    inet_pton(AF_INET, host.c_str(), &address.sin_addr);
    if (::bind(socket_, reinterpret_cast<sockaddr *>(&address), sizeof address) != 0 ||
        !platform::setNonBlocking(socket_, true)) {
        close();
        return false;
    }
    return true;
}

void UdpSocket::close() {
    if (socket_ != platform::kInvalidSocket) platform::closeSocket(socket_);
    socket_ = platform::kInvalidSocket;
}

bool UdpSocket::receive(Datagram &datagram) {
    std::uint8_t buffer[UdpPacket::kMaxDatagramSize + 1];
    for (int attempt = 0; attempt < kMaxDatagramsPerRead && isOpen(); ++attempt) {
        platform::socklen fromLength = sizeof datagram.from;
        const int received = int(::recvfrom(socket_, reinterpret_cast<char *>(buffer), int(sizeof buffer), 0,
                                            reinterpret_cast<sockaddr *>(&datagram.from), &fromLength));
        if (received < 0) {
            if (platform::lastNetError() == platform::NetError::WouldBlock) return false;
            continue; // an error about another datagram: try the next one
        }
        if (received < UdpPacket::kSessionIdSize) {
            Debugger::warning("Received too small udp packet");
            continue;
        }
        if (received > UdpPacket::kMaxDatagramSize) {
            Debugger::warning("Received too big udp packet");
            continue;
        }
        datagram.sessionId.assign(buffer, buffer + UdpPacket::kSessionIdSize);
        datagram.packet.assign(buffer + UdpPacket::kSessionIdSize, buffer + received);
        return true;
    }
    return false;
}

bool UdpSocket::send(const sockaddr_in &to, const std::vector<std::uint8_t> &sessionId, const UdpPacket &packet) {
    auto datagram = sessionId;
    const auto bytes = packet.encode();
    datagram.insert(datagram.end(), bytes.begin(), bytes.end());
    return sendDatagram(to, datagram);
}

bool UdpSocket::sendDatagram(const sockaddr_in &to, const std::vector<std::uint8_t> &datagram) {
    if (!isOpen()) return false;
    const int sent = int(::sendto(socket_, reinterpret_cast<const char *>(datagram.data()), int(datagram.size()),
                                  platform::kNoSignal, reinterpret_cast<const sockaddr *>(&to), sizeof to));
    return sent == int(datagram.size());
}
} // namespace server
