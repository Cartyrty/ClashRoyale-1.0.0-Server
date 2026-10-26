#pragma once
// A UDP packet's content after the 10-byte session id, as on the client: VInt ack count and the acked message ids
// (one byte each), then, when there are any, VInt message count and the messages. A packet with nothing to ack and
// nothing to send is empty: only the session id goes out, as a heartbeat.
#include "titan/logic/message/udp/UdpMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class LogicMessageFactory;

class UdpPacket {
public:
    // A datagram is at most 1400 bytes: the session id and 1390 bytes of packet.
    static constexpr int kMaxDatagramSize = 1400;
    static constexpr int kSessionIdSize = 10;
    static constexpr int kMaxPacketSize = kMaxDatagramSize - kSessionIdSize;

    std::vector<std::uint8_t> encode() const;
    // False for a packet the client would refuse (a size out of range, or a message type UDP does not carry).
    bool decode(const std::vector<std::uint8_t> &bytes, const LogicMessageFactory &messageFactory);

    const std::vector<std::uint8_t> &getAckMessageIds() const { return ackMessageIds_; }
    void setAckMessageIds(std::vector<std::uint8_t> ids) { ackMessageIds_ = std::move(ids); }
    const std::vector<UdpMessage> &getMessages() const { return messages_; }
    void addMessage(UdpMessage message) { messages_.push_back(std::move(message)); }

private:
    std::vector<std::uint8_t> ackMessageIds_;
    std::vector<UdpMessage> messages_;
};
} // namespace server
