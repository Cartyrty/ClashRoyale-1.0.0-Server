#include "game/replay/ReplayManager.h"

#include "game/GameServer.h"
#include "game/database/Database.h"
#include "game/network/MessageManager.h"
#include "game/utils/FileUtil.h"
#include "game/utils/TimeUtil.h"
#include "game/utils/ZlibHelper.h"
#include "logic/message/avatar/AskForBattleReplayStreamMessage.h"
#include "logic/message/avatar/BattleReportStreamMessage.h"
#include "logic/message/home/HomeBattleReplayDataMessage.h"
#include "logic/message/home/HomeBattleReplayFailedMessage.h"
#include "logic/message/home/HomeBattleReplayMessage.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>
#include <filesystem>

namespace server {
namespace fs = std::filesystem;

namespace {
constexpr int kMaxBattleReports = 25; // battle log length per player
} // namespace

ReplayManager::ReplayManager(GameServer &server) : server_(server) {
    auto &database = server_.getDatabase();
    if (database.meta("replays_ready")) return;
    importJsonState();
    database.setMeta("replays_ready", 1);
}

// The files are copied in once and left in place.
void ReplayManager::importJsonState() {
    auto &database = server_.getDatabase();
    Database::Transaction transaction(database);
    std::size_t replays = 0;
    std::size_t reports = 0;
    const auto folder = fs::path(server_.getDataDirectory()) / "replays";
    if (fs::is_directory(folder))
        for (const auto &file : fs::directory_iterator(folder)) {
            if (file.path().extension() != ".json") continue;
            database.prepare("INSERT OR REPLACE INTO replays (id, data, created_ms) VALUES (?, ?, ?)")
                .bind(1, std::int64_t(std::stoll(file.path().stem().string())))
                .bind(2, FileUtil::readFile(file.path()))
                .bind(3, TimeUtil::getMilliseconds())
                .run();
            ++replays;
        }
    const auto state = FileUtil::readJson(fs::path(server_.getDataDirectory()) / "battlelog.json", false);
    for (const auto &[account, list] : state["reports"].members())
        for (const auto &report : list.items()) {
            database
                .prepare(
                    "INSERT INTO battle_reports (account_id, replay_id, time_ms, log, arena, sender, name, level, "
                    "is_new) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)")
                .bind(1, std::int64_t(std::stoll(account)))
                .bind(2, report["replay"].integer())
                .bind(3, report["time"].integer(TimeUtil::getMilliseconds()))
                .bind(4, report["log"].str(""))
                .bind(5, report["arena"].integer())
                .bind(6, report["sender"].integer())
                .bind(7, report["name"].str(""))
                .bind(8, report["level"].integer(1))
                .bind(9, report["isNew"].boolean(true) ? 1 : 0)
                .run();
            ++reports;
        }
    transaction.commit();
    if (replays || reports)
        Debugger::print("replay", "imported %zu replays and %zu battle reports into server.db", replays, reports);
}

void ReplayManager::recordBattle(const std::vector<ReplayPlayer> &players, const std::string &battleLogJSON,
                                 const std::string &replayJSON, int arenaGlobalId) {
    auto &database = server_.getDatabase();
    Database::Transaction transaction(database);
    database.prepare("INSERT INTO replays (data, created_ms) VALUES (?, ?)")
        .bind(1, replayJSON)
        .bind(2, TimeUtil::getMilliseconds())
        .run();
    const auto replayId = database.lastInsertId();
    for (const auto &player : players) {
        if (player.accountId <= 0) continue;
        const ReplayPlayer *opponent = nullptr;
        for (const auto &other : players)
            if (other.accountId != player.accountId) opponent = &other;
        database
            .prepare(
                "INSERT INTO battle_reports (account_id, replay_id, time_ms, log, arena, sender, name, level, is_new) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1)")
            .bind(1, player.accountId)
            .bind(2, replayId)
            .bind(3, TimeUtil::getMilliseconds())
            .bind(4, battleLogJSON)
            .bind(5, arenaGlobalId)
            .bind(6, opponent ? opponent->accountId : 0)
            .bind(7, opponent ? opponent->name : std::string())
            .bind(8, opponent ? opponent->expLevel : 1)
            .run();
        // Each battle log keeps its newest kMaxBattleReports entries.
        database
            .prepare(
                "DELETE FROM battle_reports WHERE account_id = ? AND id NOT IN "
                "(SELECT id FROM battle_reports WHERE account_id = ? ORDER BY id DESC LIMIT ?)")
            .bind(1, player.accountId)
            .bind(2, player.accountId)
            .bind(3, kMaxBattleReports)
            .run();
    }
    // Replays no battle log refers to any more.
    database.exec("DELETE FROM replays WHERE id NOT IN (SELECT replay_id FROM battle_reports)");
    transaction.commit();
    Debugger::print("replay", "battle %lld recorded (%zu bytes of replay, %zu bytes of battle log)",
                    (long long)replayId, replayJSON.size(), battleLogJSON.size());
}

// Newest first; they are not new any more once sent.
void ReplayManager::onAskForBattleReplayStream(MessageManager &player, const AskForBattleReplayStreamMessage &message) {
    auto &database = server_.getDatabase();
    const auto now = TimeUtil::getMilliseconds();
    BattleReportStreamMessage stream;
    stream.setAccountId(message.getAccountId());
    auto query = database.prepare(
        "SELECT id, replay_id, time_ms, log, arena, sender, name, level, is_new FROM battle_reports "
        "WHERE account_id = ? ORDER BY id DESC");
    query.bind(1, player.getAccountId());
    while (query.step()) {
        BattleReportStreamEntry report;
        report.id = query.integer(0);
        report.replayId = query.integer(1);
        report.ageSeconds = int(std::max<std::int64_t>(0, (now - query.integer(2)) / TimeUtil::kMillisecondsPerSecond));
        report.battleLogJSON = query.text(3);
        report.arena = int(query.integer(4));
        report.opponentAvatarId = query.integer(5);
        report.opponentName = query.text(6);
        report.opponentExpLevel = int(query.integer(7));
        report.isNew = query.integer(8) != 0;
        stream.addBattleReport(std::move(report));
    }
    database.prepare("UPDATE battle_reports SET is_new = 0 WHERE account_id = ? AND is_new = 1")
        .bind(1, player.getAccountId())
        .run();
    player.sendMessage(stream);
}

// The client plays the replay in its own game mode; its home comes back with GoHome afterwards, so the home
// simulation stops here (saved).
void ReplayManager::onHomeBattleReplay(MessageManager &player, const HomeBattleReplayMessage &message) {
    auto query = server_.getDatabase().prepare("SELECT data FROM replays WHERE id = ?");
    query.bind(1, message.getReplayId());
    if (!query.step()) {
        HomeBattleReplayFailedMessage failed;
        player.sendMessage(failed);
        return;
    }
    const auto replay = ZlibHelper::compressInMySQLFormat(query.text(0));
    player.leaveHome();
    HomeBattleReplayDataMessage data;
    data.setReplayData(replay);
    player.sendMessage(data);
    Debugger::print("replay", "%s watches battle %lld", player.getAvatarName().c_str(),
                    (long long)message.getReplayId());
}
} // namespace server
