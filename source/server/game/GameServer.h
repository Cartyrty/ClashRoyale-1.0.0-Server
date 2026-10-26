#pragma once
// The server's state: storage, the managers (accounts, clans, inboxes, battles, replays, leaderboards, content) and
// the players online.
#include "game/ServerConfig.h"
#include "logic/message/LogicMessageFactory.h"
#include "titan/udp/UdpSocket.h"

#include <chrono>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace server {
class AccountManager;
class AllianceManager;
class AvatarStreamManager;
class ContentManager;
class Database;
class LeaderboardManager;
class LogicServerCommand;
class MessageManager;
class ReplayManager;
class SectorManager;

class GameServer {
public:
    // What other players see of an account: from the live home when the player is in it, else the saved avatar.
    struct AvatarProfile {
        std::string name;
        int expLevel = 1;
        int score = 0;
    };

    explicit GameServer(const ServerConfig &config);
    ~GameServer();
    GameServer(const GameServer &) = delete;
    GameServer &operator=(const GameServer &) = delete;

    void update(); // every loop iteration: matchmaking, battle turns, shop cycles, UDP heartbeats

    // UDP battles: the socket every session shares, and the datagrams it received, handed to their sessions.
    bool startUdp();
    UdpSocket &getUdpSocket() { return udpSocket_; }
    int getUdpPort() const;
    void receiveUdpPackets();

    const ServerConfig &getConfig() const { return config_; }
    const std::string &getDataDirectory() const { return config_.dataDirectory; }
    Database &getDatabase() { return *database_; }
    AccountManager &getAccountManager() { return *accountManager_; }
    AvatarStreamManager &getAvatarStreamManager() { return *avatarStreamManager_; }
    AllianceManager &getAllianceManager() { return *allianceManager_; }
    SectorManager &getSectorManager() { return *sectorManager_; }
    ReplayManager &getReplayManager() { return *replayManager_; }
    LeaderboardManager &getLeaderboardManager() { return *leaderboardManager_; }
    ContentManager &getContentManager() { return *contentManager_; }

    // Connections, logged in or not.
    void addMessageManager(MessageManager &messageManager);
    void removeMessageManager(MessageManager &messageManager);
    MessageManager *getOnlinePlayer(std::int64_t accountId); // null when offline
    AvatarProfile getAvatarProfile(std::int64_t accountId);

    // LogicServerCommand ids (saved in meta next_server_command).
    int getNextServerCommandId();
    void setNextServerCommandId(int id);
    // A server command for a player: sent when they are in their home, applied to the saved account otherwise.
    void deliverServerCommand(std::int64_t accountId, const LogicServerCommand &command);

private:
    ServerConfig config_;
    std::unique_ptr<Database> database_;
    std::unique_ptr<AccountManager> accountManager_;
    std::unique_ptr<AvatarStreamManager> avatarStreamManager_;
    std::unique_ptr<AllianceManager> allianceManager_;
    std::unique_ptr<SectorManager> sectorManager_;
    std::unique_ptr<ReplayManager> replayManager_;
    std::unique_ptr<LeaderboardManager> leaderboardManager_;
    std::unique_ptr<ContentManager> contentManager_;
    std::vector<MessageManager *> messageManagers_;
    UdpSocket udpSocket_;
    LogicMessageFactory udpMessageFactory_{true};
    std::chrono::steady_clock::time_point lastUpdate_ = std::chrono::steady_clock::now();
    int nextServerCommandId_ = 1;
};
} // namespace server
