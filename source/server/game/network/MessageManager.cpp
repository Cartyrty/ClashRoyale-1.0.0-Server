#include "game/network/MessageManager.h"

#include "logic/data/LogicClientGlobals.h"
#include "logic/message/udp/UdpCheckConnectionMessage.h"
#include "logic/message/udp/UdpConnectionInfoMessage.h"
#include "titan/udp/UdpPacketHandler.h"

#include <random>

#include "game/GameServer.h"
#include "game/account/AccountManager.h"
#include "game/alliance/AllianceManager.h"
#include "game/content/ContentManager.h"
#include "game/inbox/AvatarStreamManager.h"
#include "game/leaderboard/LeaderboardManager.h"
#include "game/network/Connection.h"
#include "game/replay/ReplayManager.h"
#include "game/sector/SectorManager.h"
#include "game/state/GameMode.h"
#include "game/utils/StringUtil.h"
#include "game/utils/TimeUtil.h"
#include "logic/avatar/LogicClientAvatar.h"
#include "logic/command/server/LogicChangeAvatarNameCommand.h"
#include "logic/message/account/ClientCapabilitiesMessage.h"
#include "logic/message/account/KeepAliveMessage.h"
#include "logic/message/account/KeepAliveServerMessage.h"
#include "logic/message/account/LoginFailedMessage.h"
#include "logic/message/account/LoginMessage.h"
#include "logic/message/account/LoginOkMessage.h"
#include "logic/message/account/SetDeviceTokenMessage.h"
#include "logic/message/alliance/AskForAllianceDataMessage.h"
#include "logic/message/alliance/AskForAllianceStreamMessage.h"
#include "logic/message/alliance/AskForJoinableAlliancesListMessage.h"
#include "logic/message/alliance/ChangeAllianceMemberRoleMessage.h"
#include "logic/message/alliance/ChangeAllianceSettingsMessage.h"
#include "logic/message/alliance/ChatToAllianceStreamMessage.h"
#include "logic/message/alliance/CreateAllianceMessage.h"
#include "logic/message/alliance/DonateAllianceUnitMessage.h"
#include "logic/message/alliance/JoinAllianceMessage.h"
#include "logic/message/alliance/LeaveAllianceMessage.h"
#include "logic/message/alliance/RequestJoinAllianceMessage.h"
#include "logic/message/alliance/RespondToAllianceJoinRequestMessage.h"
#include "logic/message/alliance/SearchAlliancesMessage.h"
#include "logic/message/avatar/AcceptChallengeMessage.h"
#include "logic/message/avatar/AskForAvatarStreamMessage.h"
#include "logic/message/avatar/AskForBattleReplayStreamMessage.h"
#include "logic/message/avatar/AvatarNameChangeFailedMessage.h"
#include "logic/message/avatar/AvatarNameCheckRequestMessage.h"
#include "logic/message/avatar/AvatarNameCheckResponseMessage.h"
#include "logic/message/avatar/CancelChallengeMessage.h"
#include "logic/message/avatar/ChangeAvatarNameMessage.h"
#include "logic/message/battle/SendBattleEventMessage.h"
#include "logic/message/friend/AskForPlayingFacebookFriendsMessage.h"
#include "logic/message/friend/AskForPlayingGamecenterFriendsMessage.h"
#include "logic/message/home/CancelMatchmakeMessage.h"
#include "logic/message/home/EndClientTurnMessage.h"
#include "logic/message/home/GoHomeMessage.h"
#include "logic/message/home/HomeBattleReplayMessage.h"
#include "logic/message/home/HomeBattleReplayViewedMessage.h"
#include "logic/message/home/HomeLogicStoppedMessage.h"
#include "logic/message/home/StartMissionMessage.h"
#include "logic/message/home/VisitHomeMessage.h"
#include "logic/message/home/VisitedHomeDataMessage.h"
#include "logic/message/ranking/AskForAllianceRankingListMessage.h"
#include "logic/message/ranking/AskForAvatarLocalRankingListMessage.h"
#include "logic/message/ranking/AskForAvatarRankingListMessage.h"
#include "logic/message/royaltv/AskForTVContentMessage.h"
#include "logic/message/royaltv/RoyalTVContentMessage.h"
#include "logic/message/sector/RequestSectorStateMessage.h"
#include "logic/message/sector/SectorCommandMessage.h"
#include "titan/logic/debug/Debugger.h"

#include <optional>

namespace server {
namespace {
// Name rules (EnterNamePopup / SettingsPopup): 2..15 characters, no control characters, no double or edge spaces.
// Error codes as the client's MessageManager reads them.
namespace AvatarNameRules {
constexpr int kOk = 0;
constexpr int kInvalid = 1;
constexpr int kTooShort = 2;
constexpr int kAlreadyChanged = 3;
constexpr int kTooLong = 4;
constexpr int kLevelTooLow = 5;
constexpr std::size_t kMinCharacters = 2;
constexpr std::size_t kMaxCharacters = 15;
constexpr int kNameChangeStartLevel = 4; // client_globals.csv NAME_CHANGE_START_LEVEL
constexpr int kNameChangeUsed = 1;       // LogicClientAvatar::getNameChangeState

int check(const std::optional<std::string> &name) {
    if (!name || StringUtil::hasControlCharacters(*name)) return kInvalid;
    if (name->find("  ") != std::string::npos || *name != StringUtil::trimSpaces(*name)) return kInvalid;
    if (StringUtil::characterCount(*name) < kMinCharacters) return kTooShort;
    if (StringUtil::characterCount(*name) > kMaxCharacters) return kTooLong;
    return kOk;
}
} // namespace AvatarNameRules
} // namespace

MessageManager::MessageManager(Connection &connection, GameServer &server) : connection_(connection), server_(server) {
    server_.addMessageManager(*this);
}

MessageManager::~MessageManager() {
    clearUdpInfo();
    gameMode_.reset();
    server_.removeMessageManager(*this);
}

void MessageManager::sendMessage(PiranhaMessage &message) { connection_.sendMessage(message); }

// ---------------------------------------------------------------- UDP battles
namespace {
constexpr int kNonceLength = 16;
constexpr const char *kNonceAlphabet = "abcdefghijklmnopqrstuvwxyz0123456789";
// A message larger than this goes over TCP: it would not fit a datagram with the packet's other content.
constexpr std::size_t kMaxUdpMessageSize = 1200;

std::mt19937_64 &getRandom() {
    static std::mt19937_64 random{std::random_device{}()};
    return random;
}
} // namespace

// The session id and the nonce are new for every session; the host is the address the client reached over TCP
// (--udp-host overrides it, for a server behind NAT).
void MessageManager::sendUdpConnectionInfo() {
    clearUdpInfo();
    auto &socket = server_.getUdpSocket();
    if (!socket.isOpen()) return;
    std::vector<std::uint8_t> sessionId(UdpPacket::kSessionIdSize);
    for (auto &byte : sessionId) byte = std::uint8_t(getRandom()());
    std::string nonce;
    for (int i = 0; i < kNonceLength; ++i) nonce += kNonceAlphabet[getRandom()() % 36];
    udpPacketHandler_ = std::make_unique<UdpPacketHandler>(socket, sessionId, nonce);
    udpPacketHandler_->setHeartbeatTimeSetting(float(LogicClientGlobals::getUdpHeartbeatMs()) / 1000.0f);
    udpPacketHandler_->setDisconnectedTimeSetting(float(LogicClientGlobals::getUdpDisconnectedSeconds()));
    const auto &config = server_.getConfig();
    UdpConnectionInfoMessage info;
    info.setServerHost(config.udpHost.empty() ? connection_.getLocalAddress() : config.udpHost);
    info.setServerPort(server_.getUdpPort());
    info.setSessionId(sessionId);
    info.setNonce(nonce);
    sendMessage(info);
}

void MessageManager::clearUdpInfo() { udpPacketHandler_.reset(); }

// As the client's sendUdpMessage: over UDP while the session is valid, over TCP otherwise.
bool MessageManager::sendUdpMessage(PiranhaMessage &message) {
    if (!udpPacketHandler_ || !udpPacketHandler_->isValidConnection()) {
        sendMessage(message);
        return true;
    }
    message.encodeOnce();
    const auto length = message.getByteStream().getByteArray().size();
    if (length > kMaxUdpMessageSize) {
        sendMessage(message);
        return true;
    }
    if (!udpPacketHandler_->send(message)) {
        Debugger::warning("Failed to send message with UDP");
        return false;
    }
    Debugger::print("msg", "%s <- %d (%zu bytes, udp)", getPeer().c_str(), message.getMessageType(), length);
    return true;
}

void MessageManager::receiveUdpPacket(const UdpPacket &packet, const sockaddr_in &from) {
    if (udpPacketHandler_) udpPacketHandler_->receive(packet, from, *this);
}

// The same handling as over TCP; UdpCheckConnectionMessage only opens the session (its packet was acked).
void MessageManager::udpMessageReceived(std::unique_ptr<PiranhaMessage> message) {
    if (message->getMessageType() == UdpCheckConnectionMessage::kMessageType) {
        Debugger::print("udp", "%s: UDP connection checked", getPeer().c_str());
        return;
    }
    Debugger::print("msg", "%s -> %d (%zu bytes, udp)", getPeer().c_str(), message->getMessageType(),
                    message->getByteStream().getByteArray().size());
    receiveMessage(*message);
}

void MessageManager::updateUdp(float deltaSeconds) {
    if (udpPacketHandler_) udpPacketHandler_->update(deltaSeconds);
}
const std::string &MessageManager::getPeer() const { return connection_.getPeer(); }
Account *MessageManager::getAccount() const { return server_.getAccountManager().getAccount(accountId_); }

std::string MessageManager::getAvatarName() const {
    if (gameMode_) return gameMode_->getAvatar().getName();
    const auto *account = getAccount();
    return account ? LogicClientAvatar::decode(account->avatar).getName() : std::string();
}

int MessageManager::getExpLevel() const {
    if (gameMode_) return gameMode_->getAvatar().getExpLevel();
    const auto *account = getAccount();
    return account ? LogicClientAvatar::decode(account->avatar).getExpLevel() : 1;
}

int MessageManager::getScore() const {
    if (gameMode_) return gameMode_->getAvatar().getScore();
    const auto *account = getAccount();
    return account ? LogicClientAvatar::decode(account->avatar).getScore() : 0;
}

void MessageManager::receiveMessage(PiranhaMessage &message) {
    const int type = message.getMessageType();
    switch (type) {
    case LoginMessage::kMessageType: return onLogin(static_cast<LoginMessage &>(message));
    case KeepAliveMessage::kMessageType: {
        KeepAliveServerMessage keepAlive;
        return sendMessage(keepAlive);
    }
    case ClientCapabilitiesMessage::kMessageType:
    case SetDeviceTokenMessage::kMessageType:
    case AskForPlayingGamecenterFriendsMessage::kMessageType:
    case AskForPlayingFacebookFriendsMessage::kMessageType: return;
    }
    if (!accountId_) {
        Debugger::warning("%s sent %d before logging in", getPeer().c_str(), type);
        connection_.close();
        return;
    }
    auto &alliances = server_.getAllianceManager();
    auto &sectors = server_.getSectorManager();
    auto &replays = server_.getReplayManager();
    auto &leaderboards = server_.getLeaderboardManager();
    switch (type) {
    // Home.
    case GoHomeMessage::kMessageType: return goHome();
    case EndClientTurnMessage::kMessageType: return onEndClientTurn(static_cast<EndClientTurnMessage &>(message));
    case HomeLogicStoppedMessage::kMessageType: return;
    case AvatarNameCheckRequestMessage::kMessageType:
        return onAvatarNameCheckRequest(static_cast<AvatarNameCheckRequestMessage &>(message));
    case ChangeAvatarNameMessage::kMessageType:
        return onChangeAvatarName(static_cast<ChangeAvatarNameMessage &>(message));
    case VisitHomeMessage::kMessageType: return onVisitHome(static_cast<VisitHomeMessage &>(message));
    case AskForAvatarStreamMessage::kMessageType: return server_.getAvatarStreamManager().sendAvatarStream(*this);
    case AskForAvatarRankingListMessage::kMessageType: return leaderboards.onAskForAvatarRankingList(*this, false);
    case AskForAvatarLocalRankingListMessage::kMessageType: return leaderboards.onAskForAvatarRankingList(*this, true);
    case AskForTVContentMessage::kMessageType: return onAskForTVContent();
    case AskForBattleReplayStreamMessage::kMessageType:
        return replays.onAskForBattleReplayStream(*this, static_cast<AskForBattleReplayStreamMessage &>(message));
    case HomeBattleReplayMessage::kMessageType:
        return replays.onHomeBattleReplay(*this, static_cast<HomeBattleReplayMessage &>(message));
    case HomeBattleReplayViewedMessage::kMessageType: return;
    // Sector.
    case StartMissionMessage::kMessageType:
        return sectors.onStartMission(*this, static_cast<StartMissionMessage &>(message));
    case CancelMatchmakeMessage::kMessageType: return sectors.onCancelMatchmake(*this);
    case SectorCommandMessage::kMessageType:
        return sectors.onSectorCommand(*this, static_cast<SectorCommandMessage &>(message));
    case RequestSectorStateMessage::kMessageType:
        return sectors.onRequestSectorState(*this, static_cast<RequestSectorStateMessage &>(message));
    case SendBattleEventMessage::kMessageType:
        return sectors.onSendBattleEvent(*this, static_cast<SendBattleEventMessage &>(message));
    // Alliance.
    case AskForAllianceDataMessage::kMessageType:
        return alliances.onAskForAllianceData(*this, static_cast<AskForAllianceDataMessage &>(message));
    case AskForJoinableAlliancesListMessage::kMessageType: return alliances.onAskForJoinableAlliancesList(*this);
    case AskForAllianceStreamMessage::kMessageType: return alliances.onAskForAllianceStream(*this);
    case CreateAllianceMessage::kMessageType:
        return alliances.onCreateAlliance(*this, static_cast<CreateAllianceMessage &>(message));
    case JoinAllianceMessage::kMessageType:
        return alliances.onJoinAlliance(*this, static_cast<JoinAllianceMessage &>(message));
    case ChangeAllianceMemberRoleMessage::kMessageType:
        return alliances.onChangeAllianceMemberRole(*this, static_cast<ChangeAllianceMemberRoleMessage &>(message));
    case LeaveAllianceMessage::kMessageType: return alliances.onLeaveAlliance(*this);
    case DonateAllianceUnitMessage::kMessageType:
        return alliances.onDonateAllianceUnit(*this, static_cast<DonateAllianceUnitMessage &>(message));
    case ChatToAllianceStreamMessage::kMessageType:
        return alliances.onChatToAllianceStream(*this, static_cast<ChatToAllianceStreamMessage &>(message));
    case ChangeAllianceSettingsMessage::kMessageType:
        return alliances.onChangeAllianceSettings(*this, static_cast<ChangeAllianceSettingsMessage &>(message));
    case RequestJoinAllianceMessage::kMessageType:
        return alliances.onRequestJoinAlliance(*this, static_cast<RequestJoinAllianceMessage &>(message));
    case RespondToAllianceJoinRequestMessage::kMessageType:
        return alliances.onRespondToAllianceJoinRequest(*this,
                                                        static_cast<RespondToAllianceJoinRequestMessage &>(message));
    case SearchAlliancesMessage::kMessageType:
        return alliances.onSearchAlliances(*this, static_cast<SearchAlliancesMessage &>(message));
    case CancelChallengeMessage::kMessageType: return alliances.onCancelChallenge(*this);
    case AcceptChallengeMessage::kMessageType:
        return alliances.onAcceptChallenge(*this, static_cast<AcceptChallengeMessage &>(message));
    case AskForAllianceRankingListMessage::kMessageType:
        return leaderboards.onAskForAllianceRankingList(*this,
                                                        static_cast<AskForAllianceRankingListMessage &>(message));
    }
    Debugger::print("server", "%s: message %d not handled", getPeer().c_str(), type);
}

// Id 0 (or an unknown id) creates an account; a known id needs its pass token. Outdated content gets LoginFailedMessage
// error 7 with the server's fingerprint and the content URL instead.
void MessageManager::onLogin(const LoginMessage &message) {
    const auto &content = server_.getContentManager();
    const auto &resourceSha = message.getResourceSha();
    if (content.isEnabled() && !resourceSha.empty() && !content.isUpToDate(resourceSha)) {
        const auto url = content.getContentUrl(connection_.getLocalAddress());
        Debugger::print("content", "%s has content %s, the server %s: content update from %s", getPeer().c_str(),
                        resourceSha.c_str(), content.getSha().c_str(), url.c_str());
        LoginFailedMessage failed;
        failed.setErrorCode(LoginFailedMessage::kErrorContentUpdate);
        failed.setResourceFingerprintData(content.getFingerprintJSON());
        failed.setContentUrl(url);
        sendMessage(failed);
        return;
    }
    auto &accounts = server_.getAccountManager();
    auto *account = message.getAccountId() ? accounts.getAccount(message.getAccountId()) : nullptr;
    if (account && account->passToken != message.getPassToken()) {
        Debugger::warning("%s: wrong pass token for account %lld", getPeer().c_str(),
                          (long long)message.getAccountId());
        connection_.close();
        return;
    }
    if (!account) account = &accounts.createAccount();
    if (auto *other = server_.getOnlinePlayer(account->id)) {
        Debugger::print("server", "account %lld logged in again; closing the older connection", (long long)account->id);
        other->closeForNewLogin();
    }
    accountId_ = account->id;
    ++account->sessionCount;
    Debugger::print("server", "%s logged in as %lld (%s)", getPeer().c_str(), (long long)account->id,
                    getAvatarName().c_str());
    LoginOkMessage loginOk;
    loginOk.setAccountId(account->id);
    loginOk.setPassToken(account->passToken);
    loginOk.setSessionCount(account->sessionCount);
    loginOk.setPlayTimeSeconds(account->playSeconds);
    loginOk.setDaysSinceStartedPlaying(
        int((TimeUtil::getMilliseconds() - account->createdMs) / TimeUtil::kMillisecondsPerDay));
    loginOk.setAccountCreatedDate(account->createdMs);
    sendMessage(loginOk);
    // A running battle takes the player back in; otherwise the home.
    if (!server_.getSectorManager().rejoin(*this)) goHome();
    server_.getAllianceManager().onLoggedIn(*this);
}

void MessageManager::closeForNewLogin() {
    connection_.close();
    onDisconnected();
    gameMode_.reset();
    accountId_ = 0;
}

void MessageManager::onDisconnected() {
    if (gameMode_) gameMode_->save();
    server_.getSectorManager().onDisconnected(*this);
}

void MessageManager::goHome() {
    server_.getSectorManager().leave(*this);
    leaveHome();
    gameMode_ = std::make_unique<GameMode>(*this, server_);
}

void MessageManager::leaveHome() {
    if (!gameMode_) return;
    gameMode_->save();
    gameMode_.reset();
}

void MessageManager::checkShop() {
    if (gameMode_) gameMode_->checkShop();
}

// In a battle the client's commands (card placements) come in EndClientTurn too.
void MessageManager::onEndClientTurn(EndClientTurnMessage &message) {
    if (gameMode_) return gameMode_->endClientTurn(message);
    if (!server_.getSectorManager().onEndClientTurn(*this, message))
        Debugger::print("server", "%s: EndClientTurn outside the home ignored", getPeer().c_str());
}

void MessageManager::onAvatarNameCheckRequest(const AvatarNameCheckRequestMessage &message) {
    const auto &name = message.getName();
    int error = AvatarNameRules::check(name);
    if (!error && gameMode_ && gameMode_->getAvatar().getNameChangeState() == AvatarNameRules::kNameChangeUsed)
        error = AvatarNameRules::kAlreadyChanged;
    if (!error && getExpLevel() < AvatarNameRules::kNameChangeStartLevel) error = AvatarNameRules::kLevelTooLow;
    AvatarNameCheckResponseMessage response;
    response.setInvalid(error != AvatarNameRules::kOk);
    response.setErrorCode(error);
    response.setName(name.value_or(""));
    sendMessage(response);
}

// The first name (not yet set by the user) only follows the name rules; a later change uses the one free change.
void MessageManager::onChangeAvatarName(const ChangeAvatarNameMessage &message) {
    if (!gameMode_) return;
    const auto avatar = gameMode_->getAvatar();
    const auto &name = message.getName();
    const bool firstName = !avatar.getNameSetByUser();
    const int nameChangeState = avatar.getNameChangeState();
    int error = AvatarNameRules::check(name);
    if (!error && !firstName && nameChangeState == AvatarNameRules::kNameChangeUsed)
        error = AvatarNameRules::kAlreadyChanged;
    if (!error && !firstName && getExpLevel() < AvatarNameRules::kNameChangeStartLevel)
        error = AvatarNameRules::kLevelTooLow;
    if (error) {
        AvatarNameChangeFailedMessage failed;
        failed.setErrorCode(error);
        sendMessage(failed);
        return;
    }
    const int newState = firstName ? nameChangeState : AvatarNameRules::kNameChangeUsed;
    server_.deliverServerCommand(accountId_, LogicChangeAvatarNameCommand(*name, newState, message.getNameSetByUser()));
    server_.getAllianceManager().renameMember(accountId_, *name);
    Debugger::print("server", "account %lld is now %s", (long long)accountId_, name->c_str());
}

void MessageManager::onVisitHome(const VisitHomeMessage &message) {
    const auto homeId = message.getHomeId();
    if (auto *owner = server_.getOnlinePlayer(homeId))
        if (owner->gameMode_) owner->gameMode_->save();
    const auto *account = server_.getAccountManager().getAccount(homeId);
    if (!account) {
        Debugger::print("server", "VisitHome for unknown home %lld", (long long)homeId);
        return;
    }
    VisitedHomeDataMessage visited;
    visited.setHome(account->home);
    visited.setOwnerAvatar(account->avatar);
    sendMessage(visited);
}

// TV Royale: the server has no replays to show.
void MessageManager::onAskForTVContent() {
    RoyalTVContentMessage content;
    content.setEntryCount(0);
    sendMessage(content);
}
} // namespace server
