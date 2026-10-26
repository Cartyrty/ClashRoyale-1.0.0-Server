#pragma once
// One message inside a UDP packet, as on the client: Byte id (the sender's sequence number, wrapping at 256), VInt
// message type, VInt length, then the message's encoding, RC4-encrypted.
#include "titan/logic/datastream/ByteStream.h"

#include <cstdint>
#include <vector>

namespace server {
class UdpMessage {
public:
    UdpMessage() = default;
    UdpMessage(std::uint8_t messageId, int messageType, std::vector<std::uint8_t> encryptedPayload)
        : messageId_(messageId), messageType_(messageType), payload_(std::move(encryptedPayload)) {}

    void encode(ByteStream &stream) const;
    bool decode(ByteStream &stream); // false when the length is not valid

    std::uint8_t getMessageId() const { return messageId_; }
    int getMessageType() const { return messageType_; }
    const std::vector<std::uint8_t> &getPayload() const { return payload_; }
    // Whether this message comes after the id (ids wrap at 256: a window of 128 ids counts as newer).
    bool isMoreRecent(std::uint8_t messageId) const;

private:
    std::uint8_t messageId_ = 0;
    int messageType_ = 0;
    std::vector<std::uint8_t> payload_;
};
} // namespace server
