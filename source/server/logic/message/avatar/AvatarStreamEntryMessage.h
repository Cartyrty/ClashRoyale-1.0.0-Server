#pragma once
// A new inbox entry. Encoded by the library's own AvatarStreamEntryMessage.
#include "library/LibraryMessage.h"
#include "logic/message/avatar/stream/AvatarStreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AvatarStreamEntryMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24412;

    void encode() override {
        // AvatarStreamEntryMessage: AvatarStreamEntry* +0x48.
        constexpr int kEntryOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writePointer(kEntryOffset, entry_->createLibraryEntry());
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    // Not copied: the entry outlives the message.
    void setStreamEntry(const AvatarStreamEntry &entry) { entry_ = &entry; }

private:
    const AvatarStreamEntry *entry_ = nullptr;
};
} // namespace server
