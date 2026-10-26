#pragma once
// Clans the player can join.
#include "logic/message/alliance/AllianceHeaderEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class JoinableAllianceListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24304;

    void encode() override {
        stream_.writeVInt(int(alliances_.size()));
        for (const auto &entry : alliances_) entry.encode(stream_);
    }
    int getMessageType() const override { return kMessageType; }

    void addAlliance(AllianceHeaderEntry entry) { alliances_.push_back(std::move(entry)); }

private:
    std::vector<AllianceHeaderEntry> alliances_;
};
} // namespace server
