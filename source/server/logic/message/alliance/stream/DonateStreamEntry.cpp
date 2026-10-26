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

// Int total capacity, Int container count, the containers (Long donor, Int count, the cards), VInt requested card
// count, the cards, Boolean has message (+ String message).
void DonateStreamEntry::encodeFields(ByteStream &stream) const {
    stream.writeInt(totalCapacity_);
    stream.writeInt(int(donations_.size()));
    for (const auto &container : donations_) {
        stream.writeLong(container.donorAvatarId);
        stream.writeInt(int(container.spells.size()));
        for (int spell : container.spells) ByteStreamHelper::writeDataReference(stream, spell);
    }
    stream.writeVInt(int(requestedSpells_.size()));
    for (int spell : requestedSpells_) ByteStreamHelper::writeDataReference(stream, spell);
    stream.writeBoolean(!message_.empty());
    if (!message_.empty()) stream.writeStringReference(message_);
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
