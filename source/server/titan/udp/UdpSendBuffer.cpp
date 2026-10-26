#include "titan/udp/UdpSendBuffer.h"

#include "titan/logic/debug/Debugger.h"

namespace server {
std::uint8_t UdpSendBuffer::addMessage(int messageType, std::vector<std::uint8_t> encryptedPayload, float time) {
    const auto messageId = nextMessageId_++;
    Debugger::print("udp", "UdpSendBuffer::addMessage type %d, udp id %d - not acked count %zu", messageType, messageId,
                    messages_.size());
    messages_.emplace_back(messageId, messageType, std::move(encryptedPayload));
    sendTimes_.push_back(time);
    if (messages_.size() >= kMaxMessages) Debugger::warning("UdpSendBuffer::addMessage buffer is full");
    return messageId;
}

void UdpSendBuffer::ackMessages(const std::vector<std::uint8_t> &messageIds, float time) {
    for (const auto messageId : messageIds) {
        for (std::size_t i = 0; i < messages_.size(); ++i) {
            if (messages_[i].getMessageId() != messageId) continue;
            roundTripTime_ = time - sendTimes_[i];
            messages_.erase(messages_.begin() + std::ptrdiff_t(i));
            sendTimes_.erase(sendTimes_.begin() + std::ptrdiff_t(i));
            Debugger::print("udp", "Acking message id: %d remaining count %zu", messageId, messages_.size());
            break;
        }
    }
}
} // namespace server
