#include "logic/message/alliance/stream/DonateStreamEntry.h"

#include "logic/data/GlobalID.h"
#include "logic/data/LogicData.h"
#include "logic/data/LogicDataTables.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <algorithm>

namespace server {
bool DonateStreamEntry::isRequested(int spellGlobalId) const {
    return std::find(requestedSpells_.begin(), requestedSpells_.end(), spellGlobalId) != requestedSpells_.end();
}

int DonateStreamEntry::getUsedCapacity() const {
    int used = 0;
    for (const auto &container : donations_)
        for (int spell : container.spells) used += LogicData::getDonateCapacity(LogicDataTables::getDataById(spell));
    return used;
}

int DonateStreamEntry::getDonationCount(std::int64_t donorAvatarId) const {
    for (const auto &container : donations_)
        if (container.donorAvatarId == donorAvatarId) return int(container.spells.size());
    return 0;
}

void DonateStreamEntry::addDonation(std::int64_t donorAvatarId, int spellGlobalId) {
    for (auto &container : donations_) {
        if (container.donorAvatarId != donorAvatarId) continue;
        container.spells.push_back(spellGlobalId);
        return;
    }
    donations_.push_back({donorAvatarId, {spellGlobalId}});
}

// DonateStreamEntry (88 bytes): total capacity +0x38, donations LogicArrayList<DonationContainer*>* +0x40 and
// requested cards LogicArrayList<const LogicSpellData*>* +0x48 (both made by its constructor), message String* +0x50
// (null: none). Donations go in through the library's own DonateStreamEntry::addDonation (LogicDonationHelper: one
// DonationContainer per donor, holding a clone of the donor id).
void DonateStreamEntry::fillLibraryEntry(library::Ptr entry) const {
    constexpr int kTotalCapacityOffset = 0x38;
    constexpr int kRequestedSpellsOffset = 0x48;
    constexpr int kMessageOffset = 0x50;
    library::writeInt(entry + kTotalCapacityOffset, totalCapacity_);
    for (const auto &container : donations_) {
        const auto donor = library::newLogicLong(container.donorAvatarId);
        for (int spell : container.spells)
            library::call("__ZN17DonateStreamEntry11addDonationEPK9LogicLongPK14LogicSpellData",
                          {entry, donor, LogicDataTables::getDataById(spell)});
        library::release(donor);
    }
    const auto requested = library::readPointer(entry + kRequestedSpellsOffset);
    for (int spell : requestedSpells_)
        library::addArrayItem("__ZN14LogicArrayListIPK14LogicSpellDataE3addERKS2_", requested,
                              LogicDataTables::getDataById(spell));
    if (!message_.empty()) library::writePointer(entry + kMessageOffset, library::newString(message_));
}

void DonateStreamEntry::saveFields(json::Value &saved) const {
    saved["totalCapacity"] = totalCapacity_;
    auto donations = json::Value::array();
    for (const auto &container : donations_) {
        auto savedContainer = json::Value::object();
        savedContainer["avatar"] = container.donorAvatarId;
        auto units = json::Value::array();
        for (int spell : container.spells) units.push(GlobalID::saveToJSON(spell));
        savedContainer["units"] = units;
        donations.push(savedContainer);
    }
    saved["donations"] = donations;
    auto spells = json::Value::array();
    for (int spell : requestedSpells_) spells.push(GlobalID::saveToJSON(spell));
    saved["spells"] = spells;
    saved["text"] = message_;
}

void DonateStreamEntry::loadFields(const json::Value &saved) {
    totalCapacity_ = int(saved["totalCapacity"].integer());
    for (const auto &savedContainer : saved["donations"].items()) {
        DonationContainer container;
        container.donorAvatarId = savedContainer["avatar"].integer();
        for (const auto &unit : savedContainer["units"].items())
            container.spells.push_back(GlobalID::loadFromJSON(unit));
        donations_.push_back(std::move(container));
    }
    for (const auto &spell : saved["spells"].items()) requestedSpells_.push_back(GlobalID::loadFromJSON(spell));
    message_ = saved["text"].str("");
}
} // namespace server
