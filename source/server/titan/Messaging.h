#pragma once
// Messaging as on the client, server side: the Piranha frame (u16 type, u24 payload length, u16 version, payload)
// with the payload RC4-encrypted, one encrypter per direction. Incoming frames become messages through the
// LogicMessageFactory.
#include "logic/message/LogicMessageFactory.h"
#include "titan/RC4Encrypter.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace server {
class Messaging {
public:
    static constexpr std::size_t kHeaderSize = 7;

    Messaging();

    // Encodes the message and returns its frame, ready to send.
    std::vector<std::uint8_t> encodeFrame(PiranhaMessage &message);
    // Takes received bytes; returns the messages they complete, decoded.
    std::vector<std::unique_ptr<PiranhaMessage>> receive(const std::uint8_t *data, std::size_t length);

private:
    LogicMessageFactory messageFactory_;
    RC4Encrypter receiveEncrypter_;
    RC4Encrypter sendEncrypter_;
    std::vector<std::uint8_t> pending_; // a frame not received completely yet
};
} // namespace server
