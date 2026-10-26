#include "game/inbox/AvatarStreamManager.h"

#include "game/GameServer.h"
#include "game/account/AccountManager.h"
#include "game/database/Database.h"
#include "game/network/MessageManager.h"
#include "game/utils/TimeUtil.h"
#include "logic/message/avatar/AvatarStreamEntryMessage.h"
#include "logic/message/avatar/AvatarStreamMessage.h"

#include <string>

namespace server {
AvatarStreamManager::AvatarStreamManager(GameServer &server) : server_(server) {}

AvatarStreamManager::AvatarSocialState AvatarStreamManager::loadState(const json::Value &saved) {
    AvatarSocialState state;
    for (const auto &savedEntry : saved["avatarStream"].items())
        if (auto entry = AvatarStreamEntry::load(savedEntry)) state.entries.push_back(std::move(entry));
    state.nextEntryId = saved["nextAvatarStreamId"].integer(1);
    state.requestEnd = saved["requestEnd"].integer();
    state.teamMailEnd = saved["teamMailEnd"].integer();
    return state;
}

json::Value AvatarStreamManager::saveState(const AvatarSocialState &state) {
    auto saved = json::Value::object();
    auto entries = json::Value::array();
    for (const auto &entry : state.entries) entries.push(entry->save());
    saved["avatarStream"] = entries;
    saved["nextAvatarStreamId"] = state.nextEntryId;
    saved["requestEnd"] = state.requestEnd;
    saved["teamMailEnd"] = state.teamMailEnd;
    return saved;
}

void AvatarStreamManager::load(Database &database) {
    auto query = database.prepare("SELECT account_id, data FROM avatar_social");
    while (query.step()) {
        bool ok = false;
        const auto saved = json::Value::parse(query.text(1), &ok);
        if (ok) states_[query.integer(0)] = loadState(saved);
    }
}

void AvatarStreamManager::importNodeState(const json::Value &avatars) {
    for (const auto &[id, avatar] : avatars.members()) states_[std::stoll(id)] = loadState(avatar);
}

void AvatarStreamManager::saveRows(Database &database) const {
    database.exec("DELETE FROM avatar_social;");
    auto insert = database.prepare("INSERT INTO avatar_social (account_id, data) VALUES (?, ?)");
    for (const auto &[accountId, state] : states_) insert.bind(1, accountId).bind(2, saveState(state).dump()).run();
}

void AvatarStreamManager::save() {
    auto &database = server_.getDatabase();
    Database::Transaction transaction(database);
    saveRows(database);
    transaction.commit();
}

void AvatarStreamManager::addEntry(std::int64_t accountId, std::unique_ptr<AvatarStreamEntry> entry) {
    if (!server_.getAccountManager().getAccount(accountId)) return;
    auto &state = states_[accountId];
    entry->setId(state.nextEntryId++);
    entry->setCreationTime(TimeUtil::getMilliseconds());
    entry->setNew(true);
    auto &added = *state.entries.emplace_back(std::move(entry));
    if (auto *player = server_.getOnlinePlayer(accountId)) {
        AvatarStreamEntryMessage message;
        message.setStreamEntry(added);
        player->sendMessage(message);
        added.setNew(false);
    }
}

void AvatarStreamManager::sendAvatarStream(MessageManager &player) {
    auto &state = states_[player.getAccountId()];
    AvatarStreamMessage message;
    for (const auto &entry : state.entries) message.addStreamEntry(*entry);
    player.sendMessage(message);
    for (auto &entry : state.entries) entry->setNew(false);
    save();
}

std::int64_t AvatarStreamManager::getRequestEnd(std::int64_t accountId) const {
    const auto found = states_.find(accountId);
    return found == states_.end() ? 0 : found->second.requestEnd;
}

void AvatarStreamManager::setRequestEnd(std::int64_t accountId, std::int64_t seconds) {
    states_[accountId].requestEnd = seconds;
}

std::int64_t AvatarStreamManager::getTeamMailEnd(std::int64_t accountId) const {
    const auto found = states_.find(accountId);
    return found == states_.end() ? 0 : found->second.teamMailEnd;
}

void AvatarStreamManager::setTeamMailEnd(std::int64_t accountId, std::int64_t seconds) {
    states_[accountId].teamMailEnd = seconds;
}
} // namespace server
