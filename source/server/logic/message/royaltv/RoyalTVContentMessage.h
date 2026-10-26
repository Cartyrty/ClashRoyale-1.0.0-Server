#pragma once
// TV Royale's replays (the server has none). Payload: VInt entryCount.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class RoyalTVContentMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24405;

    void encode() override { stream_.writeVInt(entryCount_); }
    int getMessageType() const override { return kMessageType; }

    void setEntryCount(int entryCount) { entryCount_ = entryCount; }

private:
    int entryCount_ = 0;
};
} // namespace server
