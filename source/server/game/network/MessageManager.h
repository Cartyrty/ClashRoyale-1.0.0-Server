#pragma once
// What the server does with one connection's messages, as the client's MessageManager does with the server's: login,
// the home (GameMode), and the messages for battles, clans, replays and leaderboards, handed to their managers. It is
// also the server's view of the player on that connection. Like the client's, it holds the connection's UDP battle
// session: battle messages go over UDP while the session is valid, over TCP otherwise (sendUdpMessage).
#include "platform/Socket.h"
#include "titan/udp/UdpMessageListener.h"

#include <cstdint>
#include <memory>
#include <string>

namespace server {
class AvatarNameCheckRequestMessage;
class ChangeAvatarNameMessage;
class Connection;
class EndClientTurnMessage;
class GameMode;
class GameServer;
class LoginMessage;
class PiranhaMessage;
class UdpPacket;
class UdpPacketHandler;
class VisitHomeMessage;
struct Account;

class MessageManager : public UdpMessageListener {
public:
    MessageManager(Connection &connection, GameServer &server);
    ~MessageManager() override;
    MessageManager(const MessageManager &) = delete;
    MessageManager &operator=(const MessageManager &) = delete;

    void receiveMessage(PiranhaMessage &message);
    void sendMessage(PiranhaMessage &message);
    void onDisconnected();

    std::int64_t getAccountId() const { return accountId_; } // 0 before login
    Account *getAccount() const;
    const std::string &getPeer() const;
    std::string getAvatarName() const;
    int getExpLevel() const;
    int getScore() const;

    GameMode *getGameMode() const { return gameMode_.get(); } // null outside the home
    void goHome();
    void leaveHome(); // ends the home simulation (a battle or a replay starts); the account keeps the saved state
    void checkShop();

    // UDP battles. sendUdpConnectionInfo starts a session for a battle (UdpConnectionInfoMessage over TCP); the client
    // ends its side when it gets BattleResultMessage, and clearUdpInfo ends the server's.
    void sendUdpConnectionInfo();
    void clearUdpInfo();
    bool sendUdpMessage(PiranhaMessage &message);
    UdpPacketHandler *getUdpPacketHandler() const { return udpPacketHandler_.get(); }
    void receiveUdpPacket(const UdpPacket &packet, const sockaddr_in &from);
    void udpMessageReceived(std::unique_ptr<PiranhaMessage> message) override;
    void updateUdp(float deltaSeconds);

private:
    void onLogin(const LoginMessage &message);
    void onEndClientTurn(EndClientTurnMessage &message);
    void onAvatarNameCheckRequest(const AvatarNameCheckRequestMessage &message);
    void onChangeAvatarName(const ChangeAvatarNameMessage &message);
    void onVisitHome(const VisitHomeMessage &message);
    void onAskForTVContent();
    void closeForNewLogin(); // the account logged in on another connection

    Connection &connection_;
    GameServer &server_;
    std::int64_t accountId_ = 0;
    std::unique_ptr<GameMode> gameMode_;
    std::unique_ptr<UdpPacketHandler> udpPacketHandler_;
};
} // namespace server
