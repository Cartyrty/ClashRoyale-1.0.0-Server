#pragma once
// A replay to watch: its JSON in MySQL zlib format (ZlibHelper::decompressInMySQLFormat on the client). Encoded by the
// library's own HomeBattleReplayDataMessage.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class HomeBattleReplayDataMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24114;

    void encode() override;
    int getMessageType() const override { return kMessageType; }

    void setReplayData(const std::vector<std::uint8_t> &compressedReplay) { replayData_ = compressedReplay; }

private:
    std::vector<std::uint8_t> replayData_;
};
} // namespace server
