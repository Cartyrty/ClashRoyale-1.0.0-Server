#pragma once
// TV Royale's replays (the server has none). Encoded by the library's own RoyalTVContentMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class RoyalTVContentMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24405;

    void encode() override {
        // RoyalTVContentMessage: LogicArrayList<RoyalTVEntry*>* +0x48 (empty: the server has no replays; a null list
        // would be written as -1).
        constexpr int kEntriesOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writePointer(kEntriesOffset, library::newArrayList());
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }
};
} // namespace server
