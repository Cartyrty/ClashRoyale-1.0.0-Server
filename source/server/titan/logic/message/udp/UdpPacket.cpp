#include "titan/logic/message/udp/UdpPacket.h"

#include "logic/message/LogicMessageFactory.h"
#include "titan/logic/debug/Debugger.h"

namespace server {
// The messages go oldest first, as many as fit: the receiver delivers them in id order, so the oldest unacked one is
// the one it waits for.
std::vector<std::uint8_t> UdpPacket::encode() const {
    if (ackMessageIds_.empty() && messages_.empty()) return {};
    ByteStream stream;
    stream.writeVInt(int(ackMessageIds_.size()));
    stream.writeBytesWithoutLength(ackMessageIds_);
    auto packet = stream.getByteArray();
    std::vector<std::uint8_t> messages;
    int count = 0;
    for (const auto &message : messages_) {
        ByteStream encoded;
        message.encode(encoded);
        const auto bytes = encoded.getByteArray();
        if (packet.size() + messages.size() + bytes.size() + 2 > std::size_t(kMaxPacketSize)) {
            Debugger::warning("UdpPacket::encode over max size");
            break;
        }
        messages.insert(messages.end(), bytes.begin(), bytes.end());
        ++count;
    }
    if (count > 0) {
        ByteStream countStream;
        countStream.writeVInt(count);
        const auto countBytes = countStream.getByteArray();
        packet.insert(packet.end(), countBytes.begin(), countBytes.end());
        packet.insert(packet.end(), messages.begin(), messages.end());
    }
    return packet;
}

// The client pads its packets to full size until the server answers: what follows the messages is ignored.
bool UdpPacket::decode(const std::vector<std::uint8_t> &bytes, const LogicMessageFactory &messageFactory) {
    if (int(bytes.size()) > kMaxPacketSize) return false;
    if (bytes.empty()) return true;
    ByteStream stream(bytes);
    const int ackCount = stream.readVInt();
    if (ackCount < 0 || ackCount > kMaxPacketSize) return false;
    ackMessageIds_ = stream.readBytes(ackCount, kMaxPacketSize);
    if (stream.isAtEnd()) return true;
    const int messageCount = stream.readVInt();
    if (messageCount < 0 || messageCount > kMaxPacketSize) return false;
    for (int i = 0; i < messageCount; ++i) {
        UdpMessage message;
        if (!message.decode(stream)) return false;
        if (!messageFactory.createMessageByType(message.getMessageType())) {
            Debugger::warning("UdpMessage::decode unable to read message type %d", message.getMessageType());
            return false;
        }
        messages_.push_back(std::move(message));
    }
    return true;
}
} // namespace server
