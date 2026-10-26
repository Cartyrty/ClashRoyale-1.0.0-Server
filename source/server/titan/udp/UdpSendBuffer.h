#pragma once
// The messages sent and not acknowledged yet, as on the client: each gets the next message id (from 1, wrapping at
// 256) and goes out again in every packet until the other side acks it.
#include "titan/logic/message/udp/UdpMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class UdpSendBuffer {
public:
    static constexpr std::size_t kMaxMessages = 256;

    // Returns the message's id.
    std::uint8_t addMessage(int messageType, std::vector<std::uint8_t> encryptedPayload, float time);
    void ackMessages(const std::vector<std::uint8_t> &messageIds, float time);
    const std::vector<UdpMessage> &getNotAckedMessages() const { return messages_; }
    float getRoundTripTime() const { return roundTripTime_; } // of the last acked message, in seconds

private:
    std::uint8_t nextMessageId_ = 1;
    std::vector<UdpMessage> messages_;
    std::vector<float> sendTimes_;
    float roundTripTime_ = 10000.0f;
};
} // namespace server
