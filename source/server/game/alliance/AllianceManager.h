#pragma once
// Every clan: rosters and streams (the clans, clan_members and clan_entries tables of server.db), and the clan
// messages. Everything that changes a player's home or avatar (joining, leaving, roles, badges, donations) is a server
// command, executed by the library (GameServer::deliverServerCommand).
#include "game/alliance/Alliance.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace server {
class AcceptChallengeMessage;
class AskForAllianceDataMessage;
class ChangeAllianceMemberRoleMessage;
class ChangeAllianceSettingsMessage;
class ChatToAllianceStreamMessage;
class CreateAllianceMessage;
class DonateAllianceUnitMessage;
class GameServer;
class JoinAllianceMessage;
class LogicCommand;
class MessageManager;
class PiranhaMessage;
class RequestJoinAllianceMessage;
class RespondToAllianceJoinRequestMessage;
class SearchAlliancesMessage;

class AllianceManager {
public:
    explicit AllianceManager(GameServer &server);

    // Messages.
    void onAskForAllianceData(MessageManager &player, const AskForAllianceDataMessage &message);
    void onAskForJoinableAlliancesList(MessageManager &player);
    void onAskForAllianceStream(MessageManager &player);
    void onCreateAlliance(MessageManager &player, const CreateAllianceMessage &message);
    void onJoinAlliance(MessageManager &player, const JoinAllianceMessage &message);
    void onLeaveAlliance(MessageManager &player);
    void onChangeAllianceMemberRole(MessageManager &player, const ChangeAllianceMemberRoleMessage &message);
    void onChangeAllianceSettings(MessageManager &player, const ChangeAllianceSettingsMessage &message);
    void onChatToAllianceStream(MessageManager &player, const ChatToAllianceStreamMessage &message);
    void onDonateAllianceUnit(MessageManager &player, const DonateAllianceUnitMessage &message);
    void onRequestJoinAlliance(MessageManager &player, const RequestJoinAllianceMessage &message);
    void onRespondToAllianceJoinRequest(MessageManager &player, const RespondToAllianceJoinRequestMessage &message);
    void onSearchAlliances(MessageManager &player, const SearchAlliancesMessage &message);
    void onCancelChallenge(MessageManager &player);
    void onAcceptChallenge(MessageManager &player, const AcceptChallengeMessage &message);

    // The original client never asks for its clan stream: it is sent after the home.
    void onLoggedIn(MessageManager &player);
    // Clan side effects of the home commands that need their fields (card requests, challenges, kicks, mail).
    void onHomeCommand(MessageManager &player, const LogicCommand &command);

    const std::vector<std::unique_ptr<Alliance>> &getAlliances() const { return alliances_; }
    Alliance *getAlliance(std::int64_t id);
    Alliance *getAllianceOf(std::int64_t avatarId); // null outside a clan
    int getScore(const Alliance &alliance) const;   // the members' trophies
    void renameMember(std::int64_t avatarId, const std::string &name);

private:
    // Persistence.
    void load();
    void importJsonState(); // clans.json (or the Node server's import/clan.json) from before server.db
    void save();            // clans and the avatar_social rows, in one transaction

    // Delivery.
    void broadcastMessage(const Alliance &alliance, PiranhaMessage &message);
    void broadcastStreamEntry(const Alliance &alliance, StreamEntry &entry);
    void broadcastStreamEntryRemoved(const Alliance &alliance, std::int64_t entryId);
    StreamEntry &addEvent(Alliance &alliance, int eventType, const AllianceMember &subject,
                          const AllianceMember *actor);
    void sendAllianceData(MessageManager &player, Alliance &alliance);
    void sendAllianceStream(MessageManager &player, Alliance &alliance);
    void sendChangeFailed(MessageManager &player, int reason);
    void joinAlliance(std::int64_t avatarId, const Alliance &alliance, bool created);
    void leaveAlliance(std::int64_t avatarId, std::int64_t allianceId, bool kicked);

    // Home commands.
    void onRequestSpells(MessageManager &player, const LogicCommand &command);
    void onChallenge(MessageManager &player, const LogicCommand &command);
    void onKickAllianceMember(MessageManager &player, const LogicCommand &command);
    void onSendAllianceMail(MessageManager &player, const LogicCommand &command);

    GameServer &server_;
    std::vector<std::unique_ptr<Alliance>> alliances_;
    std::int64_t nextAllianceId_ = 101;
};
} // namespace server
