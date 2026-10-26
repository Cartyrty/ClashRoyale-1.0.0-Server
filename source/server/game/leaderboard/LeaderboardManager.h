#pragma once
// The leaderboards: players by trophies, clans by their members' trophies (the top 200; global and local are the same
// lists).
namespace server {
class AskForAllianceRankingListMessage;
class GameServer;
class MessageManager;

class LeaderboardManager {
public:
    explicit LeaderboardManager(GameServer &server) : server_(server) {}

    void onAskForAvatarRankingList(MessageManager &player, bool local);
    void onAskForAllianceRankingList(MessageManager &player, const AskForAllianceRankingListMessage &message);

private:
    GameServer &server_;
};
} // namespace server
