#include "titan/logic/message/udp/UdpMessage.h"

namespace server {
namespace {
constexpr int kMaxPayloadLength = 1400;
constexpr int kIdWindow = 128;
} // namespace

void UdpMessage::encode(ByteStream &stream) const {
    stream.writeByte(messageId_);
    stream.writeVInt(messageType_);
    stream.writeVInt(int(payload_.size()));
    stream.writeBytesWithoutLength(payload_);
}

bool UdpMessage::decode(ByteStream &stream) {
    messageId_ = stream.readByte();
    messageType_ = stream.readVInt();
    const int length = stream.readVInt();
    if (length < 0 || length > kMaxPayloadLength) return false;
    payload_ = stream.readBytes(length, kMaxPayloadLength);
    return int(payload_.size()) == length;
}

// UdpMessage::isMoreRecent, with the client's signed byte ids.
bool UdpMessage::isMoreRecent(std::uint8_t messageId) const {
    const int own = std::int8_t(messageId_);
    const int other = std::int8_t(messageId);
    if (own > other && own - other < kIdWindow) return true;
    return own < other && other - own >= kIdWindow;
}
} // namespace server
