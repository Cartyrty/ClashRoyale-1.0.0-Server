#include "titan/udp/UdpPacketHandler.h"

#include "titan/logic/debug/Debugger.h"
#include "titan/udp/UdpMessageListener.h"

#include <algorithm>

namespace server {
UdpPacketHandler::UdpPacketHandler(UdpSocket &socket, std::vector<std::uint8_t> sessionId, const std::string &nonce)
    : socket_(socket),
      sessionId_(std::move(sessionId)),
      sendEncrypter_(std::string(RC4Encrypter::kKey) + nonce),
      receiveEncrypter_(std::string(RC4Encrypter::kKey) + nonce) {}

bool UdpPacketHandler::isValidConnection() const { return received_ && timeSinceReceive_ < disconnectedTime_; }

void UdpPacketHandler::receive(const UdpPacket &packet, const sockaddr_in &from, UdpMessageListener &listener) {
    peer_ = from;
    received_ = true;
    timeSinceReceive_ = 0.0f;
    sendBuffer_.ackMessages(packet.getAckMessageIds(), time_);
    std::vector<UdpMessage> ready;
    for (const auto &message : packet.getMessages()) {
        const auto messageId = message.getMessageId();
        receiveBuffer_.processMessage(message, ready);
        if (std::find(ackMessageIds_.begin(), ackMessageIds_.end(), messageId) != ackMessageIds_.end())
            Debugger::warning("UdpPacketHandler::receive id already in buffer");
        else
            ackMessageIds_.push_back(messageId);
        Debugger::print("udp", "UdpPacketHandler::receive got message: %d, ack count %zu", messageId,
                        packet.getAckMessageIds().size());
    }
    if (!ackMessageIds_.empty()) send();
    // In order now: decrypted with the session's receive stream, decoded and handed on.
    for (auto &message : ready) {
        auto payload = message.getPayload();
        receiveEncrypter_.encrypt(payload.data(), payload.size());
        auto piranhaMessage = messageFactory_.createMessageByType(message.getMessageType());
        if (!piranhaMessage) continue;
        piranhaMessage->getByteStream().setByteArray(payload);
        piranhaMessage->decode();
        listener.udpMessageReceived(std::move(piranhaMessage));
    }
}

bool UdpPacketHandler::send(PiranhaMessage &message) {
    message.encodeOnce();
    auto payload = message.getByteStream().getByteArray();
    sendEncrypter_.encrypt(payload.data(), payload.size());
    sendBuffer_.addMessage(message.getMessageType(), std::move(payload), time_);
    return send();
}

bool UdpPacketHandler::send() {
    timeSinceSend_ = 0.0f;
    if (!received_) return false; // the client's address is not known yet
    UdpPacket packet;
    packet.setAckMessageIds(std::move(ackMessageIds_));
    ackMessageIds_.clear();
    for (const auto &message : sendBuffer_.getNotAckedMessages()) packet.addMessage(message);
    const bool sent = socket_.send(peer_, sessionId_, packet);
    if (!sent)
        Debugger::warning("Failed to send UDP packet");
    else if (!packet.getMessages().empty())
        Debugger::print("udp", "UDP packet sent successfully with %zu messages", packet.getMessages().size());
    return sent;
}

void UdpPacketHandler::update(float deltaSeconds) {
    time_ += deltaSeconds;
    timeSinceReceive_ += deltaSeconds;
    timeSinceSend_ += deltaSeconds;
    if (timeSinceSend_ > heartbeatTime_) send();
}
} // namespace server
