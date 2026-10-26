#pragma once
// One UDP session (a battle player), the server's side of the client's UdpPacketHandler. Messages are reliable and in
// order: each is RC4-encrypted once (key: the game's RC4 key + the session nonce; one encrypter per direction), kept
// in the send buffer and sent again in every packet until acked; received ones are acked at once and delivered in id
// order. With nothing to send, a packet still goes out every heartbeat interval (UDP_HEARTBEAT_MS). The session is
// valid while packets keep arriving (UDP_DISCONNECTED_SECONDS).
#include "logic/message/LogicMessageFactory.h"
#include "titan/RC4Encrypter.h"
#include "titan/udp/UdpReceiveBuffer.h"
#include "titan/udp/UdpSendBuffer.h"
#include "titan/udp/UdpSocket.h"

#include <cstdint>
#include <string>
#include <vector>

namespace server {
class UdpMessageListener;

class UdpPacketHandler {
public:
    UdpPacketHandler(UdpSocket &socket, std::vector<std::uint8_t> sessionId, const std::string &nonce);

    const std::vector<std::uint8_t> &getSessionId() const { return sessionId_; }
    // A packet from the client: its address becomes the session's (the client's NAT mapping may change).
    void receive(const UdpPacket &packet, const sockaddr_in &from, UdpMessageListener &listener);
    // Encrypts and queues the message, then sends a packet.
    bool send(PiranhaMessage &message);
    void update(float deltaSeconds); // heartbeats and the disconnect timer
    bool isValidConnection() const;  // the client's address is known and it was heard from recently

    void setHeartbeatTimeSetting(float seconds) { heartbeatTime_ = seconds; }
    void setDisconnectedTimeSetting(float seconds) { disconnectedTime_ = seconds; }

private:
    bool send(); // a packet with the pending acks and every message not acked yet

    UdpSocket &socket_;
    std::vector<std::uint8_t> sessionId_;
    LogicMessageFactory messageFactory_{true};
    RC4Encrypter sendEncrypter_;
    RC4Encrypter receiveEncrypter_;
    UdpSendBuffer sendBuffer_;
    UdpReceiveBuffer receiveBuffer_;
    std::vector<std::uint8_t> ackMessageIds_;
    sockaddr_in peer_{};
    bool received_ = false; // the client was heard from: its address is known
    float time_ = 0.0f;     // session clock, in seconds
    float timeSinceSend_ = 0.0f;
    float timeSinceReceive_ = 0.0f;
    float heartbeatTime_ = 0.2f;
    float disconnectedTime_ = 10.0f;
};
} // namespace server
