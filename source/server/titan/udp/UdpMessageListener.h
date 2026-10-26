#pragma once
// Where a UDP session's messages go once they are in order and decrypted (the client's MessageManager is its
// UdpSocket's listener the same way).
#include "titan/logic/message/PiranhaMessage.h"

#include <memory>

namespace server {
class UdpMessageListener {
public:
    virtual ~UdpMessageListener() = default;
    virtual void udpMessageReceived(std::unique_ptr<PiranhaMessage> message) = 0;
};
} // namespace server
