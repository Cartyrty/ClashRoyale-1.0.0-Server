#pragma once
// A card a clan member donated to the player.
#include "logic/data/GlobalID.h"
#include "logic/message/avatar/stream/AvatarStreamEntry.h"
#include "logic/data/LogicDataTables.h"

namespace server {
class DonationReceivedStreamEntry : public AvatarStreamEntry {
public:
    int getAvatarStreamEntryType() const override { return kDonationReceived; }

    void setDonation(int spellGlobalId, int count) {
        spell_ = spellGlobalId;
        count_ = count;
    }

protected:
    void fillLibraryEntry(library::Ptr entry) const override {
        // DonationReceivedStreamEntry (64 bytes): spell LogicSpellData* +0x30, count +0x38.
        constexpr int kSpellOffset = 0x30;
        constexpr int kCountOffset = 0x38;
        library::writePointer(entry + kSpellOffset, LogicDataTables::getDataById(spell_));
        library::writeInt(entry + kCountOffset, count_);
    }
    void saveFields(json::Value &saved) const override {
        saved["count"] = count_;
        saved["spell"] = GlobalID::saveToJSON(spell_);
    }
    void loadFields(const json::Value &saved) override {
        count_ = int(saved["count"].integer(1));
        spell_ = GlobalID::loadFromJSON(saved["spell"]);
    }

private:
    int spell_ = 0;
    int count_ = 1;
};
} // namespace server
