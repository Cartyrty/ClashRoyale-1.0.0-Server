#pragma once
// Clans the player can join. Encoded by the library's own JoinableAllianceListMessage.
#include "library/LibraryMessage.h"
#include "logic/message/alliance/AllianceHeaderEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class JoinableAllianceListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24304;

    void encode() override {
        // JoinableAllianceListMessage: LogicArrayList<AllianceHeaderEntry*>* +0x48.
        constexpr int kAlliancesOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        const auto list = library::newArrayList();
        for (const auto &entry : alliances_)
            library::addArrayItem("__ZN14LogicArrayListIP19AllianceHeaderEntryE3addERKS1_", list,
                                  entry.createLibraryEntry());
        message.writePointer(kAlliancesOffset, list);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void addAlliance(AllianceHeaderEntry entry) { alliances_.push_back(std::move(entry)); }

private:
    std::vector<AllianceHeaderEntry> alliances_;
};
} // namespace server
