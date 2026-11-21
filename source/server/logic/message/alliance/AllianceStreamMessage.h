#pragma once
// The whole clan stream (chat, requests, events). Encoded by the library's own AllianceStreamMessage.
#include "library/LibraryMessage.h"
#include "logic/message/alliance/stream/StreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AllianceStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24311;

    void encode() override {
        // AllianceStreamMessage: LogicArrayList<StreamEntry*>* +0x48.
        constexpr int kEntriesOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        const auto list = library::newArrayList();
        for (const auto *entry : entries_)
            library::addArrayItem("__ZN14LogicArrayListIP11StreamEntryE3addERKS1_", list, entry->createLibraryEntry());
        message.writePointer(kEntriesOffset, list);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    // Not copied: the entry outlives the message.
    void addStreamEntry(const StreamEntry &entry) { entries_.push_back(&entry); }

private:
    std::vector<const StreamEntry *> entries_;
};
} // namespace server
