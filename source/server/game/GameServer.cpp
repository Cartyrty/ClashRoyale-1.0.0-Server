#include "game/GameServer.h"

#include "game/account/AccountManager.h"
#include "game/alliance/AllianceManager.h"
#include "game/content/ContentManager.h"
#include "game/database/Database.h"
#include "game/inbox/AvatarStreamManager.h"
#include "game/leaderboard/LeaderboardManager.h"
#include "game/network/MessageManager.h"
#include "game/replay/ReplayManager.h"
#include "game/sector/SectorManager.h"
#include "game/state/GameMode.h"
#include "logic/avatar/LogicClientAvatar.h"
#include "logic/command/server/LogicServerCommand.h"
#include "titan/logic/debug/Debugger.h"
#include "titan/udp/UdpPacketHandler.h"

#include <algorithm>
#include <filesystem>

namespace server {
namespace fs = std::filesystem;

GameServer::GameServer(const ServerConfig &config) : config_(config) {
    database_ = std::make_unique<Database>((fs::path(config_.dataDirectory) / "server.db").string());
    nextServerCommandId_ = int(database_->meta("next_server_command").value_or(1));
    accountManager_ = std::make_unique<AccountManager>(*database_, config_.dataDirectory);
    avatarStreamManager_ = std::make_unique<AvatarStreamManager>(*this);
    allianceManager_ = std::make_unique<AllianceManager>(*this);
    sectorManager_ = std::make_unique<SectorManager>(*this);
    replayManager_ = std::make_unique<ReplayManager>(*this);
    leaderboardManager_ = std::make_unique<LeaderboardManager>(*this);
    contentManager_ =
        std::make_unique<ContentManager>(config_.resourceDirectory, config_.dataDirectory, config_.contentUrl);
}

GameServer::~GameServer() = default;

void GameServer::update() {
    const auto now = std::chrono::steady_clock::now();
    const float deltaSeconds = std::chrono::duration<float>(now - lastUpdate_).count();
    lastUpdate_ = now;
    sectorManager_->update();
    for (auto *messageManager : messageManagers_) {
        messageManager->checkShop();
        messageManager->updateUdp(deltaSeconds);
    }
}

int GameServer::getUdpPort() const { return config_.udpPort < 0 ? config_.port : config_.udpPort; }

bool GameServer::startUdp() {
    if (getUdpPort() == 0) return false;
    if (!udpSocket_.bind(config_.host, getUdpPort())) {
        Debugger::warning("cannot use UDP port %d; battles go over TCP only", getUdpPort());
        return false;
    }
    Debugger::print("server", "UDP battles on %s:%d", config_.host.c_str(), getUdpPort());
    return true;
}

// Each datagram goes to the session its id names; one that does not decode is dropped, as the client drops it.
void GameServer::receiveUdpPackets() {
    UdpSocket::Datagram datagram;
    while (udpSocket_.receive(datagram)) {
        MessageManager *session = nullptr;
        for (auto *messageManager : messageManagers_) {
            const auto *handler = messageManager->getUdpPacketHandler();
            if (handler && handler->getSessionId() == datagram.sessionId) session = messageManager;
        }
        if (!session) {
            Debugger::print("udp", "UDP packet for no session");
            continue;
        }
        UdpPacket packet;
        if (!packet.decode(datagram.packet, udpMessageFactory_)) {
            Debugger::warning("%s: UDP packet did not decode", session->getPeer().c_str());
            continue;
        }
        session->receiveUdpPacket(packet, datagram.from);
    }
}

void GameServer::addMessageManager(MessageManager &messageManager) { messageManagers_.push_back(&messageManager); }

void GameServer::removeMessageManager(MessageManager &messageManager) {
    messageManagers_.erase(std::remove(messageManagers_.begin(), messageManagers_.end(), &messageManager),
                           messageManagers_.end());
}

MessageManager *GameServer::getOnlinePlayer(std::int64_t accountId) {
    for (auto *messageManager : messageManagers_)
        if (messageManager->getAccountId() == accountId) return messageManager;
    return nullptr;
}

GameServer::AvatarProfile GameServer::getAvatarProfile(std::int64_t accountId) {
    AvatarProfile profile;
    if (auto *player = getOnlinePlayer(accountId)) {
        profile.name = player->getAvatarName();
        profile.expLevel = player->getExpLevel();
        profile.score = player->getScore();
        return profile;
    }
    if (const auto *account = accountManager_->getAccount(accountId)) {
        const auto avatar = LogicClientAvatar::decode(account->avatar);
        profile.name = avatar.getName();
        profile.expLevel = avatar.getExpLevel();
        profile.score = avatar.getScore();
        return profile;
    }
    for (const auto &alliance : allianceManager_->getAlliances())
        for (const auto &member : alliance->getMembers())
            if (member.avatarId == accountId) profile.name = member.name;
    return profile;
}

int GameServer::getNextServerCommandId() {
    const int id = nextServerCommandId_++;
    database_->setMeta("next_server_command", nextServerCommandId_);
    return id;
}

void GameServer::setNextServerCommandId(int id) {
    nextServerCommandId_ = id;
    database_->setMeta("next_server_command", nextServerCommandId_);
}

void GameServer::deliverServerCommand(std::int64_t accountId, const LogicServerCommand &command) {
    auto logicCommand = command.createCommand(getNextServerCommandId());
    if (!logicCommand) {
        Debugger::warning("server command did not decode");
        return;
    }
    auto *player = getOnlinePlayer(accountId);
    if (player && player->getGameMode())
        player->getGameMode()->sendServerCommand(std::move(logicCommand));
    else
        GameMode::applyServerCommandOffline(*this, accountId, std::move(logicCommand));
}
} // namespace server
