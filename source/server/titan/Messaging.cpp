#include "titan/Messaging.h"

#include "logic/message/LogicMessageFactory.h"

#include <string>

namespace server {
namespace {
const std::string kNonce = "nonce";
} // namespace

Messaging::Messaging()
    : receiveEncrypter_(std::string(RC4Encrypter::kKey) + kNonce),
      sendEncrypter_(std::string(RC4Encrypter::kKey) + kNonce) {}

std::vector<std::uint8_t> Messaging::encodeFrame(PiranhaMessage &message) {
    message.encodeOnce();
    const auto payload = message.getByteStream().getByteArray();
    const int type = message.getMessageType();
    const int version = message.getMessageVersion();
    std::vector<std::uint8_t> frame(kHeaderSize + payload.size());
    frame[0] = std::uint8_t(type >> 8);
    frame[1] = std::uint8_t(type);
    frame[2] = std::uint8_t(payload.size() >> 16);
    frame[3] = std::uint8_t(payload.size() >> 8);
    frame[4] = std::uint8_t(payload.size());
    frame[5] = std::uint8_t(version >> 8);
    frame[6] = std::uint8_t(version);
    std::copy(payload.begin(), payload.end(), frame.begin() + kHeaderSize);
    sendEncrypter_.encrypt(frame.data() + kHeaderSize, payload.size());
    return frame;
}

std::vector<std::unique_ptr<PiranhaMessage>> Messaging::receive(const std::uint8_t *data, std::size_t length) {
    pending_.insert(pending_.end(), data, data + length);
    std::vector<std::unique_ptr<PiranhaMessage>> messages;
    std::size_t at = 0;
    while (pending_.size() - at >= kHeaderSize) {
        const std::uint8_t *header = pending_.data() + at;
        const std::size_t size = (std::size_t(header[2]) << 16) | (std::size_t(header[3]) << 8) | header[4];
        if (pending_.size() - at < kHeaderSize + size) break;
        const int type = (header[0] << 8) | header[1];
        std::vector<std::uint8_t> payload(header + kHeaderSize, header + kHeaderSize + size);
        receiveEncrypter_.encrypt(payload.data(), payload.size());
        auto message = messageFactory_.createMessageByType(type);
        message->getByteStream().setByteArray(payload);
        message->decode();
        messages.push_back(std::move(message));
        at += kHeaderSize + size;
    }
    pending_.erase(pending_.begin(), pending_.begin() + std::ptrdiff_t(at));
    return messages;
}
} // namespace server
