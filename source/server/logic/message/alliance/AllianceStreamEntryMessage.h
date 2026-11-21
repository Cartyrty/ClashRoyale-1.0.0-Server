#pragma once
// A new or changed clan stream entry. Encoded by the library's own AllianceStreamEntryMessage.
#include "library/LibraryMessage.h"
#include "logic/message/alliance/stream/StreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceStreamEntryMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24312;

    void encode() override {
        // AllianceStreamEntryMessage: StreamEntry* +0x48.
        constexpr int kEntryOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writePointer(kEntryOffset, entry_->createLibraryEntry());
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    // Not copied: the entry outlives the message.
    void setStreamEntry(const StreamEntry &entry) { entry_ = &entry; }

private:
    const StreamEntry *entry_ = nullptr;
};
} // namespace server
