#include "game/alliance/Alliance.h"

#include "game/utils/TimeUtil.h"
#include "logic/data/GlobalID.h"

#include <algorithm>

namespace server {
void Alliance::setSettings(const std::string &description, int badge, int type, int requiredScore, int region) {
    description_ = description;
    badge_ = badge;
    type_ = type;
    requiredScore_ = requiredScore;
    region_ = region;
}

AllianceHeaderEntry Alliance::getHeaderEntry(int score) const {
    AllianceHeaderEntry header;
    header.id = id_;
    header.name = name_;
    header.badge = badge_;
    header.type = type_;
    header.memberCount = int(members_.size());
    header.score = score;
    header.requiredScore = requiredScore_;
    header.region = region_;
    return header;
}

AllianceMember *Alliance::getMember(std::int64_t avatarId) {
    for (auto &member : members_)
        if (member.avatarId == avatarId) return &member;
    return nullptr;
}

void Alliance::removeMember(std::int64_t avatarId) {
    members_.erase(std::remove_if(members_.begin(), members_.end(),
                                  [&](const AllianceMember &member) { return member.avatarId == avatarId; }),
                   members_.end());
}

StreamEntry &Alliance::addStreamEntry(std::unique_ptr<StreamEntry> entry) {
    entry->setId(nextStreamEntryId_++);
    entry->setCreationTime(TimeUtil::getMilliseconds());
    auto &added = *entry;
    streamEntries_.push_back(std::move(entry));
    if (streamEntries_.size() > kMaxStreamEntries) streamEntries_.erase(streamEntries_.begin());
    return added;
}

std::unique_ptr<Alliance> Alliance::loadFromJSON(const json::Value &saved) {
    auto alliance = std::make_unique<Alliance>(saved["id"].integer(), saved["name"].str(""));
    alliance->setSettings(saved["description"].str(""), GlobalID::loadFromJSON(saved["badge"]),
                          int(saved["type"].integer(kTypeOpen)), int(saved["requiredScore"].integer()),
                          GlobalID::loadFromJSON(saved["region"]));
    for (const auto &member : saved["members"].items())
        alliance->addMember({member["id"].integer(), member["name"].str(""), int(member["role"].integer(1))});
    for (const auto &savedEntry : saved["messages"].items())
        if (auto entry = StreamEntry::load(savedEntry)) alliance->loadStreamEntry(std::move(entry));
    alliance->setNextStreamEntryId(saved["nextMessage"].integer(1));
    return alliance;
}
} // namespace server
