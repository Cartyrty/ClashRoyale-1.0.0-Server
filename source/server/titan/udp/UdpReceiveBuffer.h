#pragma once
// The messages received, put back in order, as on the client: the next expected id is delivered at once, a later one
// waits until the ones before it arrive, and a repeated or older one is dropped.
#include "titan/logic/message/udp/UdpMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class UdpReceiveBuffer {
public:
    static constexpr std::size_t kMaxMessages = 256;

    // Adds the messages that are now in order to ready.
    void processMessage(UdpMessage message, std::vector<UdpMessage> &ready);

private:
    void checkBuffer(std::vector<UdpMessage> &ready);

    std::uint8_t nextMessageId_ = 1;
    std::vector<UdpMessage> waiting_; // arrived ahead of their turn
};
} // namespace server
