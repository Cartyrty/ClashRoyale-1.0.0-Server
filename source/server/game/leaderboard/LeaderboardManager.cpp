#include "game/leaderboard/LeaderboardManager.h"

#include "game/GameServer.h"
#include "game/account/AccountManager.h"
#include "game/alliance/AllianceManager.h"
#include "game/network/MessageManager.h"
#include "logic/message/ranking/AllianceLocalRankingListMessage.h"
#include "logic/message/ranking/AllianceRankingListMessage.h"
#include "logic/message/ranking/AskForAllianceRankingListMessage.h"
#include "logic/message/ranking/AvatarLocalRankingListMessage.h"
#include "logic/message/ranking/AvatarRankingListMessage.h"

#include <algorithm>
#include <vector>

namespace server {
namespace {
constexpr std::size_t kRankingSize = 200;

template <class Row> void sortByScore(std::vector<Row> &rows) {
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.score > b.score; });
    if (rows.size() > kRankingSize) rows.resize(kRankingSize);
}

template <class Message, class Entry> void sendRanking(MessageManager &player, const std::vector<Entry> &entries) {
    Message message;
    for (const auto &entry : entries) message.addRankingEntry(entry);
    player.sendMessage(message);
}
} // namespace

void LeaderboardManager::onAskForAvatarRankingList(MessageManager &player, bool local) {
    struct Row {
        std::int64_t accountId;
        GameServer::AvatarProfile profile;
        int score;
    };
    std::vector<Row> rows;
    for (auto accountId : server_.getAccountManager().getAccountIds()) {
        const auto profile = server_.getAvatarProfile(accountId);
        rows.push_back({accountId, profile, profile.score});
    }
    sortByScore(rows);
    std::vector<AvatarRankingEntry> entries;
    int order = 0;
    for (const auto &row : rows) {
        AvatarRankingEntry entry;
        entry.setId(row.accountId);
        entry.setName(row.profile.name);
        entry.setOrder(++order);
        entry.setScore(row.score);
        entry.setExpLevel(row.profile.expLevel);
        entry.setHomeId(row.accountId);
        if (const auto *alliance = server_.getAllianceManager().getAllianceOf(row.accountId))
            entry.setAlliance(alliance->getId(), alliance->getName(), alliance->getBadge());
        entries.push_back(entry);
    }
    if (local)
        sendRanking<AvatarLocalRankingListMessage>(player, entries);
    else
        sendRanking<AvatarRankingListMessage>(player, entries);
}

void LeaderboardManager::onAskForAllianceRankingList(MessageManager &player,
                                                     const AskForAllianceRankingListMessage &message) {
    struct Row {
        const Alliance *alliance;
        int score;
    };
    auto &alliances = server_.getAllianceManager();
    std::vector<Row> rows;
    for (const auto &alliance : alliances.getAlliances())
        rows.push_back({alliance.get(), alliances.getScore(*alliance)});
    sortByScore(rows);
    std::vector<AllianceRankingEntry> entries;
    int order = 0;
    for (const auto &row : rows) {
        AllianceRankingEntry entry;
        entry.setId(row.alliance->getId());
        entry.setName(row.alliance->getName());
        entry.setOrder(++order);
        entry.setScore(row.score);
        entry.setBadge(row.alliance->getBadge());
        entry.setRegion(row.alliance->getRegion());
        entry.setMemberCount(int(row.alliance->getMembers().size()));
        entries.push_back(entry);
    }
    if (message.getLocal())
        sendRanking<AllianceLocalRankingListMessage>(player, entries);
    else
        sendRanking<AllianceRankingListMessage>(player, entries);
}
} // namespace server
