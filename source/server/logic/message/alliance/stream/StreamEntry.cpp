#include "logic/message/alliance/stream/StreamEntry.h"

#include "game/utils/TimeUtil.h"
#include "logic/message/alliance/stream/AllianceEventStreamEntry.h"
#include "logic/message/alliance/stream/ChallengeStreamEntry.h"
#include "logic/message/alliance/stream/ChatStreamEntry.h"
#include "logic/message/alliance/stream/DonateStreamEntry.h"
#include "logic/message/alliance/stream/JoinRequestAllianceStreamEntry.h"

#include <algorithm>

namespace server {
namespace {
// StreamEntry: id LogicLong* +0x08, sender avatar id LogicLong* +0x10, sender home id LogicLong* +0x18, sender name
// String* +0x20, sender level +0x28, sender role +0x2c, age in seconds +0x30, removed bool +0x34.
constexpr int kIdOffset = 0x08;
constexpr int kSenderAvatarIdOffset = 0x10;
constexpr int kSenderHomeIdOffset = 0x18;
constexpr int kSenderNameOffset = 0x20;
constexpr int kSenderExpLevelOffset = 0x28;
constexpr int kSenderRoleOffset = 0x2c;
constexpr int kAgeSecondsOffset = 0x30;
constexpr int kRemovedOffset = 0x34;
} // namespace

library::Ptr StreamEntry::createLibraryEntry() const {
    const auto now = TimeUtil::getMilliseconds();
    const auto entry = library::call("__ZN18StreamEntryFactory23createStreamEntryByTypeEi",
                                     {std::uint64_t(std::uint32_t(getStreamEntryType()))});
    library::writePointer(entry + kIdOffset, library::newLogicLong(id_));
    library::writePointer(entry + kSenderAvatarIdOffset, library::newLogicLong(senderAvatarId_));
    library::writePointer(entry + kSenderHomeIdOffset, library::newLogicLong(senderAvatarId_));
    library::writePointer(entry + kSenderNameOffset, library::newString(senderName_));
    library::writeInt(entry + kSenderExpLevelOffset, senderExpLevel_);
    library::writeInt(entry + kSenderRoleOffset, senderRole_);
    library::writeInt(entry + kAgeSecondsOffset,
                      int(std::max<std::int64_t>(0, (now - creationTime_) / TimeUtil::kMillisecondsPerSecond)));
    library::writeByte(entry + kRemovedOffset, removed_ ? 1 : 0);
    fillLibraryEntry(entry);
    return entry;
}

json::Value StreamEntry::save() const {
    auto saved = json::Value::object();
    saved["type"] = getStreamEntryType();
    saved["id"] = id_;
    saved["time"] = creationTime_;
    saved["sender"] = senderAvatarId_;
    saved["name"] = senderName_;
    saved["role"] = senderRole_;
    saved["removed"] = removed_;
    saveFields(saved);
    return saved;
}

std::unique_ptr<StreamEntry> StreamEntry::load(const json::Value &saved) {
    std::unique_ptr<StreamEntry> entry;
    // The Node server's chat messages have no type.
    switch (int(saved["type"].integer(kChat))) {
    case kDonate: entry = std::make_unique<DonateStreamEntry>(); break;
    case kChat: entry = std::make_unique<ChatStreamEntry>(); break;
    case kJoinRequest: entry = std::make_unique<JoinRequestAllianceStreamEntry>(); break;
    case kAllianceEvent: entry = std::make_unique<AllianceEventStreamEntry>(); break;
    case kChallenge: entry = std::make_unique<ChallengeStreamEntry>(); break;
    default: return nullptr;
    }
    entry->id_ = saved["id"].integer();
    entry->creationTime_ = saved["time"].integer(TimeUtil::getMilliseconds());
    entry->senderAvatarId_ = saved["sender"].integer();
    entry->senderName_ = saved["name"].str("");
    entry->senderRole_ = int(saved["role"].integer(1));
    entry->removed_ = saved["removed"].boolean();
    entry->loadFields(saved);
    return entry;
}
} // namespace server
