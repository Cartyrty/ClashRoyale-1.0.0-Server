#pragma once
// A battle's full state. SectorManager::receiveSectorState reads a Byte first: 1 is a compressed state (the rest is
// the state in MySQL zlib format, SectorManager::receiveCompressedSectorState), 0 the state as it is. The server
// always compresses: a full state (1.4 KB and more) only fits a UDP datagram that way. The state is
// LogicGameMode::encode(stream, true, viewer) - the call LogicGameMode::executeOneTick uses for its snapshots, read
// back by LogicGameMode::decode(stream, true, ...); the server rebuilds a PvP player's view from exactly these bytes.
#include "game/utils/ZlibHelper.h"
#include "logic/mode/LogicGameMode.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <string>
#include <vector>

namespace server {
class SectorStateMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 21903;
    static constexpr std::uint8_t kCompressed = 1;

    void encode() override {
        stream_.writeByte(kCompressed);
        stream_.writeBytesWithoutLength(ZlibHelper::compressInMySQLFormat(std::string(state_.begin(), state_.end())));
    }
    int getMessageType() const override { return kMessageType; }

    void setState(const std::vector<std::uint8_t> &state) { state_ = state; }

    // The state for the viewer, uncompressed.
    static std::vector<std::uint8_t> encodeState(const LogicGameMode &mode, library::Ptr viewerAvatar) {
        ByteStream stream;
        mode.encode(stream, viewerAvatar);
        return stream.getByteArray();
    }
    // The state as the client rebuilds it.
    static LogicGameMode decodeState(const std::vector<std::uint8_t> &state) {
        ByteStream stream(state);
        LogicGameMode mode(true);
        mode.decode(stream);
        return mode;
    }

private:
    std::vector<std::uint8_t> state_;
};
} // namespace server
