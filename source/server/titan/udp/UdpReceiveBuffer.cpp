#include "titan/udp/UdpReceiveBuffer.h"

#include "titan/logic/debug/Debugger.h"

namespace server {
void UdpReceiveBuffer::processMessage(UdpMessage message, std::vector<UdpMessage> &ready) {
    if (message.getMessageId() == nextMessageId_) {
        ++nextMessageId_;
        ready.push_back(std::move(message));
    } else {
        bool waiting = false;
        for (const auto &other : waiting_)
            if (other.getMessageId() == message.getMessageId()) waiting = true;
        if (waiting) {
            Debugger::warning("UdpReceiveBuffer::processMessage already containing message");
        } else if (message.isMoreRecent(nextMessageId_)) {
            waiting_.push_back(std::move(message));
        } else {
            Debugger::print("udp", "UdpReceiveBuffer::processMessage ignoring packet from the past");
        }
    }
    checkBuffer(ready);
    if (waiting_.size() >= kMaxMessages) Debugger::warning("UdpReceiveBuffer::processMessage buffer is full");
}

void UdpReceiveBuffer::checkBuffer(std::vector<UdpMessage> &ready) {
    for (std::size_t delivered = 0; delivered < kMaxMessages; ++delivered) {
        auto next = waiting_.end();
        for (auto message = waiting_.begin(); message != waiting_.end(); ++message)
            if (message->getMessageId() == nextMessageId_) next = message;
        if (next == waiting_.end()) return;
        ++nextMessageId_;
        ready.push_back(std::move(*next));
        waiting_.erase(next);
    }
}
} // namespace server
