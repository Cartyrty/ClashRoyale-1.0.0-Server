#include "game/alliance/AllianceManager.h"

#include "game/GameServer.h"
#include "game/account/AccountManager.h"
#include "game/alliance/AllianceRole.h"
#include "game/database/Database.h"
#include "game/inbox/AvatarStreamManager.h"
#include "game/network/MessageManager.h"
#include "game/sector/SectorManager.h"
#include "game/state/GameMode.h"
#include "game/utils/FileUtil.h"
#include "game/utils/StringUtil.h"
#include "game/utils/TimeUtil.h"
#include "logic/command/home/LogicChallengeCommand.h"
#include "logic/command/home/LogicKickAllianceMemberCommand.h"
#include "logic/command/home/LogicRequestSpellsCommand.h"
#include "logic/command/home/LogicSendAllianceMailCommand.h"
#include "logic/command/home/LogicStartMatchmakeCommand.h"
#include "logic/command/server/LogicAllianceSettingsChangedCommand.h"
#include "logic/command/server/LogicAllianceUnitReceivedCommand.h"
#include "logic/command/server/LogicChangeAllianceRoleCommand.h"
#include "logic/command/server/LogicDonateAllianceUnitCommand.h"
#include "logic/command/server/LogicJoinAllianceCommand.h"
#include "logic/command/server/LogicLeaveAllianceCommand.h"
#include "logic/data/GlobalID.h"
#include "logic/data/LogicData.h"
#include "logic/data/LogicDataTables.h"
#include "logic/message/alliance/AllianceChangeFailedMessage.h"
#include "logic/message/alliance/AllianceCreateFailedMessage.h"
#include "logic/message/alliance/AllianceDataMessage.h"
#include "logic/message/alliance/AllianceJoinFailedMessage.h"
#include "logic/message/alliance/AllianceJoinRequestFailedMessage.h"
#include "logic/message/alliance/AllianceJoinRequestOkMessage.h"
#include "logic/message/alliance/AllianceListMessage.h"
#include "logic/message/alliance/AllianceStreamEntryMessage.h"
#include "logic/message/alliance/AllianceStreamEntryRemovedMessage.h"
#include "logic/message/alliance/AllianceStreamMessage.h"
#include "logic/message/alliance/AskForAllianceDataMessage.h"
#include "logic/message/alliance/ChangeAllianceMemberRoleMessage.h"
#include "logic/message/alliance/ChangeAllianceMemberRoleOkMessage.h"
#include "logic/message/alliance/ChangeAllianceSettingsMessage.h"
#include "logic/message/alliance/ChatToAllianceStreamMessage.h"
#include "logic/message/alliance/CreateAllianceMessage.h"
#include "logic/message/alliance/DonateAllianceUnitMessage.h"
#include "logic/message/alliance/JoinAllianceMessage.h"
#include "logic/message/alliance/JoinableAllianceListMessage.h"
#include "logic/message/alliance/KickAllianceMemberOkMessage.h"
#include "logic/message/alliance/RequestJoinAllianceMessage.h"
#include "logic/message/alliance/RespondToAllianceJoinRequestMessage.h"
#include "logic/message/alliance/SearchAlliancesMessage.h"
#include "logic/message/alliance/stream/AllianceEventStreamEntry.h"
#include "logic/message/alliance/stream/ChallengeStreamEntry.h"
#include "logic/message/alliance/stream/ChatStreamEntry.h"
#include "logic/message/alliance/stream/DonateStreamEntry.h"
#include "logic/message/alliance/stream/JoinRequestAllianceStreamEntry.h"
#include "logic/message/avatar/AcceptChallengeMessage.h"
#include "logic/message/avatar/CancelChallengeDoneMessage.h"
#include "logic/message/avatar/ChallengeFailedMessage.h"
#include "logic/message/avatar/stream/AllianceKickOutStreamEntry.h"
#include "logic/message/avatar/stream/AllianceMailAvatarStreamEntry.h"
#include "logic/message/avatar/stream/DonationReceivedStreamEntry.h"
#include "logic/message/avatar/stream/JoinAllianceResponseAvatarStreamEntry.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>
#include <filesystem>
#include <optional>

namespace server {
namespace fs = std::filesystem;

namespace {
// The development clan a server without clans starts with, as on the Node server: Test Clan, led by a test account.
constexpr std::int64_t kTestAllianceId = 100;
constexpr std::int64_t kTestLeaderId = 50;
constexpr std::int64_t kFirstAllianceId = 101;

// alliance_badges.csv and regions.csv row counts.
constexpr int kBadgeCount = 96;
constexpr int kRegionCount = 266;
constexpr int kMaxRequiredScore = 1000;
constexpr int kRequiredScoreStep = 200;
constexpr std::size_t kMaxNameCharacters = 15;
constexpr std::size_t kMaxDescriptionCharacters = 128;
constexpr std::size_t kMaxMessageCharacters = 128;
constexpr std::size_t kMaxMailTitleCharacters = 30;
constexpr std::size_t kMaxListedAlliances = 50;
constexpr int kDefaultRequestSize = 10;
constexpr int kDefaultMaxDonations = 1;
constexpr int kSecondsPerMinute = 60;
constexpr int kGenericFailure = 0;

// AllianceCreateFailedMessage / AllianceChangeFailedMessage reasons.
constexpr int kInvalidName = 1;
constexpr int kInvalidDescription = 2;
constexpr int kNameTooShort = 3;
// AllianceJoinFailedMessage reasons.
constexpr int kJoinFull = 1;
constexpr int kJoinNotOpen = 2;
constexpr int kJoinScoreTooLow = 4;
// AllianceJoinRequestFailedMessage reasons.
constexpr int kRequestClosed = 1;
constexpr int kRequestAlreadySent = 2;
constexpr int kRequestScoreTooLow = 3;

bool isValidReference(int globalId, int classId, int rowCount) {
    return GlobalID::getClassID(globalId) == classId && GlobalID::getInstanceID(globalId) < rowCount;
}

bool isValidSettings(int badge, int type, int requiredScore) {
    return isValidReference(badge, LogicDataTables::kAllianceBadgeClass, kBadgeCount) && type >= Alliance::kTypeOpen &&
           type <= Alliance::kTypeClosed && requiredScore >= 0 && requiredScore <= kMaxRequiredScore &&
           requiredScore % kRequiredScoreStep == 0;
}

bool isValidRegion(int region) { return isValidReference(region, LogicDataTables::kRegionClass, kRegionCount); }

// Chat: not empty, at most 128 characters, no control characters but new lines and tabs.
bool isValidChatMessage(const std::string &message) {
    if (StringUtil::trimSpaces(message).empty() || StringUtil::characterCount(message) > kMaxMessageCharacters)
        return false;
    for (unsigned char c : message)
        if (c < 0x20 && c != '\n' && c != '\t') return false;
    return true;
}
} // namespace

AllianceManager::AllianceManager(GameServer &server) : server_(server) { load(); }

// ---------------------------------------------------------------- persistence
void AllianceManager::load() {
    auto &database = server_.getDatabase();
    if (!database.meta("clans_ready")) {
        importJsonState();
        save();
        database.setMeta("clans_ready", 1);
        return;
    }
    nextAllianceId_ = database.meta("next_clan_id").value_or(kFirstAllianceId);
    auto alliances = database.prepare(
        "SELECT id, name, description, badge, type, required_score, region, next_entry FROM clans ORDER BY id");
    while (alliances.step()) {
        auto alliance = std::make_unique<Alliance>(alliances.integer(0), alliances.text(1));
        alliance->setSettings(alliances.text(2), int(alliances.integer(3)), int(alliances.integer(4)),
                              int(alliances.integer(5)), int(alliances.integer(6)));
        alliance->setNextStreamEntryId(alliances.integer(7));
        auto members =
            database.prepare("SELECT account_id, name, role FROM clan_members WHERE clan_id = ? ORDER BY position");
        members.bind(1, alliance->getId());
        while (members.step()) alliance->addMember({members.integer(0), members.text(1), int(members.integer(2))});
        auto entries = database.prepare("SELECT data FROM clan_entries WHERE clan_id = ? ORDER BY position");
        entries.bind(1, alliance->getId());
        while (entries.step()) {
            bool ok = false;
            const auto saved = json::Value::parse(entries.text(0), &ok);
            if (!ok) continue;
            if (auto entry = StreamEntry::load(saved)) alliance->loadStreamEntry(std::move(entry));
        }
        alliances_.push_back(std::move(alliance));
    }
    server_.getAvatarStreamManager().load(database);
    Debugger::print("server", "%zu clans loaded from server.db", alliances_.size());
}

void AllianceManager::importJsonState() {
    auto file = fs::path(server_.getDataDirectory()) / "clans.json";
    if (!fs::exists(file))
        file = fs::path(server_.getDataDirectory()) / "import" / "clan.json"; // the Node server's state
    if (!fs::exists(file)) {
        auto alliance = std::make_unique<Alliance>(kTestAllianceId, "Test Clan");
        alliance->setSettings("Local v1 development clan",
                              GlobalID::createGlobalID(LogicDataTables::kAllianceBadgeClass, 0), Alliance::kTypeOpen, 0,
                              0);
        alliance->addMember({kTestLeaderId, "Test Leader", AllianceRole::kLeader});
        alliances_.push_back(std::move(alliance));
        return;
    }
    const auto state = FileUtil::readJson(file, false);
    if (state.isNull()) Debugger::error("%s is not valid JSON", file.string().c_str());
    for (const auto &saved : state["clans"].items()) alliances_.push_back(Alliance::loadFromJSON(saved));
    nextAllianceId_ = state["nextClanId"].integer(kFirstAllianceId);
    server_.setNextServerCommandId(int(state["nextServerCommand"].integer(1)));
    // Only the social parts of the Node avatars carry over; homes and avatars are the library's account data.
    server_.getAvatarStreamManager().importNodeState(state["avatars"]);
    Debugger::print("server", "%zu clans imported into server.db from %s", alliances_.size(),
                    file.filename().string().c_str());
}

void AllianceManager::save() {
    auto &database = server_.getDatabase();
    Database::Transaction transaction(database);
    database.exec("DELETE FROM clan_entries; DELETE FROM clan_members; DELETE FROM clans;");
    auto insertAlliance = database.prepare(
        "INSERT INTO clans (id, name, description, badge, type, required_score, region, next_entry) VALUES (?, ?, ?, "
        "?, ?, ?, ?, ?)");
    auto insertMember =
        database.prepare("INSERT INTO clan_members (account_id, clan_id, name, role, position) VALUES (?, ?, ?, ?, ?)");
    auto insertEntry = database.prepare("INSERT INTO clan_entries (clan_id, position, data) VALUES (?, ?, ?)");
    for (const auto &alliance : alliances_) {
        insertAlliance.bind(1, alliance->getId())
            .bind(2, alliance->getName())
            .bind(3, alliance->getDescription())
            .bind(4, alliance->getBadge())
            .bind(5, alliance->getType())
            .bind(6, alliance->getRequiredScore())
            .bind(7, alliance->getRegion())
            .bind(8, alliance->getNextStreamEntryId())
            .run();
        int position = 0;
        for (const auto &member : alliance->getMembers())
            insertMember.bind(1, member.avatarId)
                .bind(2, alliance->getId())
                .bind(3, member.name)
                .bind(4, member.role)
                .bind(5, position++)
                .run();
        position = 0;
        for (const auto &entry : alliance->getStreamEntries())
            insertEntry.bind(1, alliance->getId()).bind(2, position++).bind(3, entry->save().dump()).run();
    }
    server_.getAvatarStreamManager().saveRows(database);
    database.setMeta("next_clan_id", nextAllianceId_);
    transaction.commit();
}

// ---------------------------------------------------------------- lookups
Alliance *AllianceManager::getAlliance(std::int64_t id) {
    for (auto &alliance : alliances_)
        if (alliance->getId() == id) return alliance.get();
    return nullptr;
}

Alliance *AllianceManager::getAllianceOf(std::int64_t avatarId) {
    for (auto &alliance : alliances_)
        if (alliance->getMember(avatarId)) return alliance.get();
    return nullptr;
}

int AllianceManager::getScore(const Alliance &alliance) const {
    int score = 0;
    for (const auto &member : alliance.getMembers()) score += server_.getAvatarProfile(member.avatarId).score;
    return score;
}

void AllianceManager::renameMember(std::int64_t avatarId, const std::string &name) {
    auto *alliance = getAllianceOf(avatarId);
    if (!alliance) return;
    alliance->getMember(avatarId)->name = name;
    save();
}

// ---------------------------------------------------------------- delivery
void AllianceManager::broadcastMessage(const Alliance &alliance, PiranhaMessage &message) {
    for (const auto &member : alliance.getMembers())
        if (auto *player = server_.getOnlinePlayer(member.avatarId)) player->sendMessage(message);
}

void AllianceManager::broadcastStreamEntry(const Alliance &alliance, StreamEntry &entry) {
    entry.setSenderExpLevel(server_.getAvatarProfile(entry.getSenderAvatarId()).expLevel);
    AllianceStreamEntryMessage message;
    message.setStreamEntry(entry);
    broadcastMessage(alliance, message);
}

void AllianceManager::broadcastStreamEntryRemoved(const Alliance &alliance, std::int64_t entryId) {
    AllianceStreamEntryRemovedMessage message;
    message.setStreamEntryId(entryId);
    broadcastMessage(alliance, message);
}

StreamEntry &AllianceManager::addEvent(Alliance &alliance, int eventType, const AllianceMember &subject,
                                       const AllianceMember *actor) {
    auto event = std::make_unique<AllianceEventStreamEntry>();
    event->setSender(subject.avatarId, subject.name, subject.role);
    event->setEventType(eventType);
    event->setTarget(actor ? actor->avatarId : 0, actor ? actor->name : "");
    return alliance.addStreamEntry(std::move(event));
}

void AllianceManager::sendAllianceData(MessageManager &player, Alliance &alliance) {
    AllianceFullEntry page;
    page.header = alliance.getHeaderEntry(getScore(alliance));
    page.description = alliance.getDescription();
    for (const auto &member : alliance.getMembers()) {
        const auto profile = server_.getAvatarProfile(member.avatarId);
        page.members.push_back({member.avatarId, profile.name.empty() ? member.name : profile.name, member.role,
                                profile.expLevel, profile.score, server_.getOnlinePlayer(member.avatarId) != nullptr});
    }
    AllianceDataMessage message;
    message.setAllianceFullEntry(std::move(page));
    player.sendMessage(message);
}

void AllianceManager::sendAllianceStream(MessageManager &player, Alliance &alliance) {
    AllianceStreamMessage message;
    for (const auto &entry : alliance.getStreamEntries()) {
        entry->setSenderExpLevel(server_.getAvatarProfile(entry->getSenderAvatarId()).expLevel);
        message.addStreamEntry(*entry);
    }
    player.sendMessage(message);
}

void AllianceManager::sendChangeFailed(MessageManager &player, int reason) {
    AllianceChangeFailedMessage message;
    message.setReason(reason);
    player.sendMessage(message);
}

void AllianceManager::joinAlliance(std::int64_t avatarId, const Alliance &alliance, bool created) {
    server_.deliverServerCommand(
        avatarId, LogicJoinAllianceCommand(alliance.getId(), alliance.getName(), alliance.getBadge(), created));
}

void AllianceManager::leaveAlliance(std::int64_t avatarId, std::int64_t allianceId, bool kicked) {
    server_.deliverServerCommand(avatarId, LogicLeaveAllianceCommand(allianceId, kicked));
}

// ---------------------------------------------------------------- clan messages
void AllianceManager::onAskForAllianceData(MessageManager &player, const AskForAllianceDataMessage &message) {
    if (auto *alliance = getAlliance(message.getAllianceId()))
        sendAllianceData(player, *alliance);
    else
        sendChangeFailed(player, kGenericFailure);
}

void AllianceManager::onAskForJoinableAlliancesList(MessageManager &player) {
    JoinableAllianceListMessage message;
    std::size_t listed = 0;
    for (const auto &alliance : alliances_) {
        if (alliance->getType() != Alliance::kTypeOpen || alliance->isFull() || listed >= kMaxListedAlliances) continue;
        message.addAlliance(alliance->getHeaderEntry(getScore(*alliance)));
        ++listed;
    }
    player.sendMessage(message);
}

// The reconstruction asks; the original client gets it after login.
void AllianceManager::onAskForAllianceStream(MessageManager &player) {
    if (auto *alliance = getAllianceOf(player.getAccountId())) sendAllianceStream(player, *alliance);
}

void AllianceManager::onLoggedIn(MessageManager &player) {
    auto *alliance = getAllianceOf(player.getAccountId());
    if (!alliance) return;
    alliance->getMember(player.getAccountId())->name = player.getAvatarName();
    sendAllianceStream(player, *alliance);
}

// The Gold cost is paid by LogicJoinAllianceCommand (created) on the home.
void AllianceManager::onCreateAlliance(MessageManager &player, const CreateAllianceMessage &message) {
    auto fail = [&](int reason) {
        AllianceCreateFailedMessage failed;
        failed.setReason(reason);
        player.sendMessage(failed);
    };
    const auto &name = message.getName();
    const auto &description = message.getDescription();
    if (!name || !StringUtil::isAllowedInput(*name, kMaxNameCharacters) || *name != StringUtil::trimSpaces(*name))
        return fail(kInvalidName);
    if (name->size() <= 1) return fail(kNameTooShort);
    for (const auto &alliance : alliances_)
        if (StringUtil::toLower(alliance->getName()) == StringUtil::toLower(*name)) return fail(kInvalidName);
    if (!description || !StringUtil::isAllowedInput(*description, kMaxDescriptionCharacters))
        return fail(kInvalidDescription);
    auto *gameMode = player.getGameMode();
    const int gold = gameMode ? gameMode->getAvatar().getGold() : 0;
    if (!isValidSettings(message.getBadge(), message.getType(), message.getRequiredScore()) ||
        !isValidRegion(message.getRegion()) || getAllianceOf(player.getAccountId()) ||
        gold < LogicDataTables::getIntGlobal("ALLIANCE_CREATE_COST"))
        return fail(kGenericFailure);
    auto alliance = std::make_unique<Alliance>(nextAllianceId_++, *name);
    alliance->setSettings(*description, message.getBadge(), message.getType(), message.getRequiredScore(),
                          message.getRegion());
    alliance->addMember({player.getAccountId(), player.getAvatarName(), AllianceRole::kLeader});
    auto &created = *alliances_.emplace_back(std::move(alliance));
    save();
    joinAlliance(player.getAccountId(), created, true);
    Debugger::print("server", "%s created clan %s", player.getAvatarName().c_str(), created.getName().c_str());
}

void AllianceManager::onJoinAlliance(MessageManager &player, const JoinAllianceMessage &message) {
    auto *alliance = getAlliance(message.getAllianceId());
    int reason = -1;
    if (!alliance || getAllianceOf(player.getAccountId()))
        reason = kGenericFailure;
    else if (alliance->isFull())
        reason = kJoinFull;
    else if (alliance->getType() != Alliance::kTypeOpen)
        reason = kJoinNotOpen;
    else if (alliance->getRequiredScore() > player.getScore())
        reason = kJoinScoreTooLow;
    if (reason >= 0) {
        AllianceJoinFailedMessage failed;
        failed.setReason(reason);
        player.sendMessage(failed);
        return;
    }
    alliance->addMember({player.getAccountId(), player.getAvatarName(), AllianceRole::kMember});
    auto &event = addEvent(*alliance, AllianceEventStreamEntry::kJoined, alliance->getMembers().back(), nullptr);
    save();
    joinAlliance(player.getAccountId(), *alliance, false);
    broadcastStreamEntry(*alliance, event);
}

// A leader leaving hands over to the highest authority; an empty clan is deleted.
void AllianceManager::onLeaveAlliance(MessageManager &player) {
    auto *alliance = getAllianceOf(player.getAccountId());
    if (!alliance) return sendChangeFailed(player, kGenericFailure);
    const AllianceMember leaving = *alliance->getMember(player.getAccountId());
    alliance->removeMember(leaving.avatarId);
    if (leaving.role == AllianceRole::kLeader && !alliance->getMembers().empty()) {
        const auto &members = alliance->getMembers();
        const auto successor =
            std::max_element(members.begin(), members.end(), [](const AllianceMember &a, const AllianceMember &b) {
                return AllianceRole::getAuthority(a.role) < AllianceRole::getAuthority(b.role);
            });
        alliance->getMember(successor->avatarId)->role = AllianceRole::kLeader;
    }
    const auto allianceId = alliance->getId();
    if (alliance->getMembers().empty()) {
        alliances_.erase(std::remove_if(alliances_.begin(), alliances_.end(),
                                        [&](const auto &other) { return other->getId() == allianceId; }),
                         alliances_.end());
        save();
    } else {
        auto &event = addEvent(*alliance, AllianceEventStreamEntry::kLeft, leaving, nullptr);
        save();
        broadcastStreamEntry(*alliance, event);
    }
    leaveAlliance(player.getAccountId(), allianceId, false);
}

// Promoting a co-leader to leader swaps the two.
void AllianceManager::onChangeAllianceMemberRole(MessageManager &player,
                                                 const ChangeAllianceMemberRoleMessage &message) {
    const auto targetId = message.getMemberId();
    const int role = message.getRole();
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *actor = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    auto *target = alliance ? alliance->getMember(targetId) : nullptr;
    if (!actor || !target || actor == target || !AllianceRole::isValid(role) ||
        !AllianceRole::hasRights(actor->role, target->role, role) ||
        (role == AllianceRole::kLeader && target->role != AllianceRole::kCoLeader))
        return sendChangeFailed(player, kGenericFailure);
    struct RoleChange {
        AllianceMember *member;
        int from;
        int to;
    };
    std::vector<RoleChange> changes{{target, target->role, role}};
    if (role == AllianceRole::kLeader) {
        changes.push_back({actor, actor->role, AllianceRole::kCoLeader});
        actor->role = AllianceRole::kCoLeader;
    }
    target->role = role;
    std::vector<StreamEntry *> events;
    for (const auto &change : changes) {
        if (change.member == actor) continue;
        const bool promoted = AllianceRole::getAuthority(change.to) > AllianceRole::getAuthority(change.from);
        events.push_back(&addEvent(*alliance,
                                   promoted ? AllianceEventStreamEntry::kPromoted : AllianceEventStreamEntry::kDemoted,
                                   *change.member, actor));
    }
    save();
    ChangeAllianceMemberRoleOkMessage ok;
    ok.setMemberId(targetId);
    ok.setRole(role);
    player.sendMessage(ok);
    for (const auto &change : changes)
        server_.deliverServerCommand(change.member->avatarId,
                                     LogicChangeAllianceRoleCommand(alliance->getId(), change.to));
    for (auto *event : events) broadcastStreamEntry(*alliance, *event);
}

void AllianceManager::onChangeAllianceSettings(MessageManager &player, const ChangeAllianceSettingsMessage &message) {
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *actor = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    const auto &description = message.getDescription();
    if (!actor || !AllianceRole::canChangeSettings(actor->role)) return sendChangeFailed(player, kGenericFailure);
    if (!description || !StringUtil::isAllowedInput(*description, kMaxDescriptionCharacters))
        return sendChangeFailed(player, kInvalidDescription);
    if (!isValidSettings(message.getBadge(), message.getType(), message.getRequiredScore()) ||
        (message.getRegion() && !isValidRegion(message.getRegion())))
        return sendChangeFailed(player, kGenericFailure);
    alliance->setSettings(*description, message.getBadge(), message.getType(), message.getRequiredScore(),
                          message.getRegion());
    save();
    for (const auto &member : alliance->getMembers())
        server_.deliverServerCommand(member.avatarId,
                                     LogicAllianceSettingsChangedCommand(alliance->getId(), alliance->getBadge()));
    for (const auto &member : alliance->getMembers())
        if (auto *online = server_.getOnlinePlayer(member.avatarId)) sendAllianceData(*online, *alliance);
}

void AllianceManager::onChatToAllianceStream(MessageManager &player, const ChatToAllianceStreamMessage &message) {
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *member = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    if (!member || !message.getMessage() || !isValidChatMessage(*message.getMessage()))
        return sendChangeFailed(player, kGenericFailure);
    auto chat = std::make_unique<ChatStreamEntry>();
    chat->setSender(member->avatarId, member->name, member->role);
    chat->setMessage(*message.getMessage());
    auto &entry = alliance->addStreamEntry(std::move(chat));
    save();
    broadcastStreamEntry(*alliance, entry);
}

// The donor's LogicDonateAllianceUnitCommand takes the card and pays the reward on their home; the requester gets
// LogicAllianceUnitReceivedCommand and a DonationReceivedStreamEntry.
void AllianceManager::onDonateAllianceUnit(MessageManager &player, const DonateAllianceUnitMessage &message) {
    const int spell = message.getSpell();
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *donor = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    auto *request = alliance ? alliance->getStreamEntry<DonateStreamEntry>(message.getStreamEntryId()) : nullptr;
    if (request && request->isRemoved()) request = nullptr;
    const auto spellData = LogicDataTables::getDataById(spell);
    auto *gameMode = player.getGameMode();
    if (!donor || !request || request->getSenderAvatarId() == player.getAccountId() || !request->isRequested(spell) ||
        !spellData || !gameMode) {
        Debugger::print("server", "%s: donation rejected", player.getAvatarName().c_str());
        return;
    }
    // Capacity: the card's within what the request has left, and the arena's per-donor limit.
    const auto arena = gameMode->getAvatar().getArena();
    const int maxDonations = arena ? LogicData::getIntegerValue(arena, "MaxDonationCount") : kDefaultMaxDonations;
    const int owned = gameMode->getHome().getSpellCount(spellData);
    if (LogicData::getDonateCapacity(spellData) > request->getTotalCapacity() - request->getUsedCapacity() ||
        request->getDonationCount(player.getAccountId()) >= maxDonations || owned < 1) {
        Debugger::print("server", "%s: donation over capacity", player.getAvatarName().c_str());
        return;
    }
    request->addDonation(player.getAccountId(), spell);
    const auto receiver = request->getSenderAvatarId();
    server_.deliverServerCommand(player.getAccountId(), LogicDonateAllianceUnitCommand(spell, request->getId()));
    if (server_.getAccountManager().getAccount(receiver)) {
        server_.deliverServerCommand(receiver, LogicAllianceUnitReceivedCommand(player.getAvatarName(), spell));
        auto received = std::make_unique<DonationReceivedStreamEntry>();
        received->setSender(player.getAccountId(), player.getAvatarName(), player.getExpLevel());
        received->setDonation(spell, 1);
        server_.getAvatarStreamManager().addEntry(receiver, std::move(received));
    }
    save();
    broadcastStreamEntry(*alliance, *request);
}

void AllianceManager::onRequestJoinAlliance(MessageManager &player, const RequestJoinAllianceMessage &message) {
    auto *alliance = getAlliance(message.getAllianceId());
    auto hasPendingRequest = [&] {
        for (const auto &entry : alliance->getStreamEntries()) {
            const auto *request = dynamic_cast<const JoinRequestAllianceStreamEntry *>(entry.get());
            if (request && request->getSenderAvatarId() == player.getAccountId() &&
                request->getState() == JoinRequestAllianceStreamEntry::kStatePending)
                return true;
        }
        return false;
    };
    int reason = -1;
    if (!alliance || getAllianceOf(player.getAccountId()) || alliance->isFull())
        reason = kGenericFailure;
    else if (alliance->getType() != Alliance::kTypeInviteOnly)
        reason = kRequestClosed;
    else if (hasPendingRequest())
        reason = kRequestAlreadySent;
    else if (alliance->getRequiredScore() > player.getScore())
        reason = kRequestScoreTooLow;
    if (reason >= 0) {
        AllianceJoinRequestFailedMessage failed;
        failed.setReason(reason);
        player.sendMessage(failed);
        return;
    }
    auto request = std::make_unique<JoinRequestAllianceStreamEntry>();
    request->setSender(player.getAccountId(), player.getAvatarName(), AllianceRole::kMember);
    request->setMessage(StringUtil::truncateCharacters(message.getMessage().value_or(""), kMaxMessageCharacters));
    auto &entry = alliance->addStreamEntry(std::move(request));
    save();
    AllianceJoinRequestOkMessage ok;
    player.sendMessage(ok);
    broadcastStreamEntry(*alliance, entry);
}

void AllianceManager::onRespondToAllianceJoinRequest(MessageManager &player,
                                                     const RespondToAllianceJoinRequestMessage &message) {
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *actor = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    auto *request =
        alliance ? alliance->getStreamEntry<JoinRequestAllianceStreamEntry>(message.getStreamEntryId()) : nullptr;
    if (!actor || !AllianceRole::canAcceptJoinRequests(actor->role) || !request ||
        request->getState() != JoinRequestAllianceStreamEntry::kStatePending)
        return;
    const auto requester = request->getSenderAvatarId();
    const bool joined = message.getAccepted() && !getAllianceOf(requester) && !alliance->isFull();
    request->setState(joined ? JoinRequestAllianceStreamEntry::kStateAccepted
                             : JoinRequestAllianceStreamEntry::kStateRefused);
    request->setResponderName(actor->name);
    const AllianceMember responder = *actor;
    StreamEntry *event = nullptr;
    if (joined) {
        alliance->addMember({requester, request->getSenderName(), AllianceRole::kMember});
        event = &addEvent(*alliance, AllianceEventStreamEntry::kAccepted, alliance->getMembers().back(), &responder);
    }
    auto response = std::make_unique<JoinAllianceResponseAvatarStreamEntry>();
    response->setSender(responder.avatarId, responder.name, player.getExpLevel());
    response->setAccepted(joined);
    response->setAlliance(alliance->getId(), alliance->getName(), alliance->getBadge());
    server_.getAvatarStreamManager().addEntry(requester, std::move(response));
    save();
    if (joined) joinAlliance(requester, *alliance, false);
    broadcastStreamEntry(*alliance, *request);
    if (event) broadcastStreamEntry(*alliance, *event);
}

void AllianceManager::onSearchAlliances(MessageManager &player, const SearchAlliancesMessage &message) {
    const auto searchString = message.getSearchString().value_or("");
    const auto searched = StringUtil::toLower(searchString);
    AllianceListMessage list;
    list.setSearchString(searchString);
    std::size_t listed = 0;
    for (const auto &alliance : alliances_) {
        const int score = getScore(*alliance);
        const int memberCount = int(alliance->getMembers().size());
        if (StringUtil::toLower(alliance->getName()).find(searched) == std::string::npos) continue;
        if (message.getRegion() && alliance->getRegion() != message.getRegion()) continue;
        if (memberCount < message.getMinMembers() || memberCount > message.getMaxMembers() ||
            score < message.getMinScore())
            continue;
        if (message.getOnlyJoinable() && (alliance->getType() != Alliance::kTypeOpen || alliance->isFull() ||
                                          alliance->getRequiredScore() > player.getScore()))
            continue;
        if (listed >= kMaxListedAlliances) continue;
        list.addAlliance(alliance->getHeaderEntry(score));
        ++listed;
    }
    player.sendMessage(list);
}

void AllianceManager::onCancelChallenge(MessageManager &player) {
    if (auto *alliance = getAllianceOf(player.getAccountId())) {
        for (const auto &entry : alliance->getStreamEntries()) {
            auto *challenge = dynamic_cast<ChallengeStreamEntry *>(entry.get());
            if (!challenge || challenge->getSenderAvatarId() != player.getAccountId() || !challenge->isOpen()) continue;
            challenge->setRemoved(true);
            save();
            broadcastStreamEntryRemoved(*alliance, challenge->getId());
        }
    }
    CancelChallengeDoneMessage done;
    player.sendMessage(done);
}

// The challenge closes and both players start a friendly battle.
void AllianceManager::onAcceptChallenge(MessageManager &player, const AcceptChallengeMessage &message) {
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *challenge = alliance ? alliance->getStreamEntry<ChallengeStreamEntry>(message.getStreamEntryId()) : nullptr;
    if (challenge && !challenge->isOpen()) challenge = nullptr;
    const auto challenger = challenge ? challenge->getSenderAvatarId() : 0;
    auto *opponent = challenger ? server_.getOnlinePlayer(challenger) : nullptr;
    if (!challenge || challenger == player.getAccountId() || !opponent || !opponent->getGameMode() ||
        !server_.getSectorManager().startFriendlyBattle(challenger, player.getAccountId())) {
        ChallengeFailedMessage failed;
        failed.setReason(kGenericFailure);
        player.sendMessage(failed);
        return;
    }
    challenge->setClosed(true);
    save();
    broadcastStreamEntry(*alliance, *challenge);
}

// ---------------------------------------------------------------- home commands with clan side effects
void AllianceManager::onHomeCommand(MessageManager &player, const LogicCommand &command) {
    switch (command.getCommandType()) {
    case LogicStartMatchmakeCommand::kCommandType:
        server_.getSectorManager().startMatchmaking(player, LogicStartMatchmakeCommand(command).getUseDiamonds());
        return;
    case LogicRequestSpellsCommand::kCommandType: return onRequestSpells(player, command);
    case LogicChallengeCommand::kCommandType: return onChallenge(player, command);
    case LogicKickAllianceMemberCommand::kCommandType: return onKickAllianceMember(player, command);
    case LogicSendAllianceMailCommand::kCommandType: return onSendAllianceMail(player, command);
    }
}

// The command's execute() checks the cooldown, the clan and the card on the home; the clan entry gets the arena's
// RequestSize. A player has one open request: the previous one is closed.
void AllianceManager::onRequestSpells(MessageManager &player, const LogicCommand &command) {
    const LogicRequestSpellsCommand requestSpells(command);
    const auto now = TimeUtil::getSeconds();
    auto &avatarStreams = server_.getAvatarStreamManager();
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *member = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    auto *gameMode = player.getGameMode();
    if (!member || !gameMode || requestSpells.getSpells().size() != 1 ||
        avatarStreams.getRequestEnd(player.getAccountId()) > now) {
        Debugger::print("server", "%s: request rejected", player.getAvatarName().c_str());
        return;
    }
    const auto arena = gameMode->getAvatar().getArena();
    auto request = std::make_unique<DonateStreamEntry>();
    request->setSender(member->avatarId, member->name, member->role);
    request->setTotalCapacity(arena ? LogicData::getIntegerValue(arena, "RequestSize") : kDefaultRequestSize);
    request->setRequestedSpells(requestSpells.getSpells());
    request->setMessage(StringUtil::truncateCharacters(requestSpells.getMessage().value_or(""), kMaxMessageCharacters));
    for (const auto &entry : alliance->getStreamEntries()) {
        auto *previous = dynamic_cast<DonateStreamEntry *>(entry.get());
        if (!previous || previous->getSenderAvatarId() != member->avatarId || previous->isRemoved()) continue;
        previous->setRemoved(true);
        broadcastStreamEntryRemoved(*alliance, previous->getId());
    }
    auto &entry = alliance->addStreamEntry(std::move(request));
    avatarStreams.setRequestEnd(
        player.getAccountId(),
        now + std::int64_t(LogicDataTables::getIntGlobal("REQUEST_SPELLS_COOLDOWN_MINUTES")) * kSecondsPerMinute);
    save();
    broadcastStreamEntry(*alliance, entry);
}

void AllianceManager::onChallenge(MessageManager &player, const LogicCommand &command) {
    const LogicChallengeCommand challengeCommand(command);
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *member = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    if (!member) return;
    for (const auto &entry : alliance->getStreamEntries()) {
        const auto *open = dynamic_cast<const ChallengeStreamEntry *>(entry.get());
        if (open && open->getSenderAvatarId() == member->avatarId && open->isOpen()) return;
    }
    auto challenge = std::make_unique<ChallengeStreamEntry>();
    challenge->setSender(member->avatarId, member->name, member->role);
    challenge->setMessage(
        StringUtil::truncateCharacters(challengeCommand.getMessage().value_or(""), kMaxMessageCharacters));
    auto &entry = alliance->addStreamEntry(std::move(challenge));
    save();
    broadcastStreamEntry(*alliance, entry);
}

void AllianceManager::onKickAllianceMember(MessageManager &player, const LogicCommand &command) {
    const LogicKickAllianceMemberCommand kick(command);
    const auto targetId = kick.getAvatarId();
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *actor = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    auto *target = alliance ? alliance->getMember(targetId) : nullptr;
    if (!actor || !target || target == actor || !AllianceRole::canKick(actor->role) ||
        !AllianceRole::hasLowerRole(target->role, actor->role)) {
        Debugger::print("server", "kick rejected");
        return;
    }
    const AllianceMember kicked = *target;
    const AllianceMember kicker = *actor;
    alliance->removeMember(targetId);
    auto &event = addEvent(*alliance, AllianceEventStreamEntry::kKicked, kicked, &kicker);
    auto kickOut = std::make_unique<AllianceKickOutStreamEntry>();
    kickOut->setSender(kicker.avatarId, kicker.name, player.getExpLevel());
    if (kick.getMessage() && !kick.getMessage()->empty())
        kickOut->setMessage(StringUtil::truncateCharacters(*kick.getMessage(), kMaxMessageCharacters));
    kickOut->setAlliance(alliance->getId(), alliance->getName(), alliance->getBadge());
    server_.getAvatarStreamManager().addEntry(targetId, std::move(kickOut));
    save();
    leaveAlliance(targetId, alliance->getId(), true);
    KickAllianceMemberOkMessage ok;
    ok.setMemberId(targetId);
    player.sendMessage(ok);
    broadcastStreamEntry(*alliance, event);
}

// TEAM_MAIL_COOLDOWN_SECS between mails.
void AllianceManager::onSendAllianceMail(MessageManager &player, const LogicCommand &command) {
    const LogicSendAllianceMailCommand mail(command);
    auto &avatarStreams = server_.getAvatarStreamManager();
    const auto now = TimeUtil::getSeconds();
    auto *alliance = getAllianceOf(player.getAccountId());
    auto *member = alliance ? alliance->getMember(player.getAccountId()) : nullptr;
    if (!member || !AllianceRole::canSendMail(member->role) || !mail.getMessage() || !mail.getTitle() ||
        avatarStreams.getTeamMailEnd(player.getAccountId()) > now) {
        Debugger::print("server", "mail rejected");
        return;
    }
    avatarStreams.setTeamMailEnd(player.getAccountId(), now + LogicDataTables::getIntGlobal("TEAM_MAIL_COOLDOWN_SECS"));
    const auto message = StringUtil::truncateCharacters(
        *mail.getMessage(), std::size_t(LogicDataTables::getIntGlobal("MAX_ALLIANCE_MAIL_LENGTH")));
    const auto title = StringUtil::truncateCharacters(*mail.getTitle(), kMaxMailTitleCharacters);
    for (const auto &recipient : alliance->getMembers()) {
        auto entry = std::make_unique<AllianceMailAvatarStreamEntry>();
        entry->setSender(member->avatarId, member->name, player.getExpLevel());
        entry->setMail(title, message);
        entry->setAlliance(alliance->getId(), alliance->getName(), alliance->getBadge());
        avatarStreams.addEntry(recipient.avatarId, std::move(entry));
    }
    save();
}
} // namespace server
