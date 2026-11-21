#pragma once
// The player's whole inbox (avatar stream). Encoded by the library's own AvatarStreamMessage.
#include "library/LibraryMessage.h"
#include "logic/message/avatar/stream/AvatarStreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AvatarStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24411;

    void encode() override {
        // AvatarStreamMessage: LogicArrayList<AvatarStreamEntry*>* +0x48.
        constexpr int kEntriesOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        const auto list = library::newArrayList();
        for (const auto *entry : entries_)
            library::addArrayItem("__ZN14LogicArrayListIP17AvatarStreamEntryE3addERKS1_", list,
                                  entry->createLibraryEntry());
        message.writePointer(kEntriesOffset, list);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    // Not copied: the entry outlives the message.
    void addStreamEntry(const AvatarStreamEntry &entry) { entries_.push_back(&entry); }

private:
    std::vector<const AvatarStreamEntry *> entries_;
};
} // namespace server
