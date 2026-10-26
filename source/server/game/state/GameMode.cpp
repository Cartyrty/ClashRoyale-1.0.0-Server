#include "game/state/GameMode.h"

#include "Hle.h"
#include "game/GameServer.h"
#include "game/account/AccountManager.h"
#include "game/alliance/AllianceManager.h"
#include "game/network/MessageManager.h"
#include "game/utils/TimeUtil.h"
#include "logic/command/server/LogicClaimRewardCommand.h"
#include "logic/command/server/LogicShopSeedChangedCommand.h"
#include "logic/data/LogicData.h"
#include "logic/home/LogicChest.h"
#include "logic/message/home/AvailableServerCommandMessage.h"
#include "logic/message/home/EndClientTurnMessage.h"
#include "logic/message/home/OutOfSyncMessage.h"
#include "logic/message/home/OwnHomeDataMessage.h"
#include "patches/StateDump.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>
#include <cstdlib>
#include <random>
#include <string>

namespace server {
namespace {
std::mt19937 &getRandom() {
    static std::mt19937 random{std::random_device{}()};
    return random;
}

int createRandomSeed() { return int(getRandom()() & 0x7fffffff); }
int createRewardSeed() { return int(getRandom()() & 0xffff); }

// LogicChest::setClaimed reports every chest that starts claiming its reward to the home's change listener
// (LogicHomeChangeListener::claimRewardStarted(chest, chestType)): the server answers each with a
// LogicClaimRewardCommand.
struct RewardClaim {
    library::Ptr chest;
    int chestType;
};
std::vector<RewardClaim> g_rewardClaims;
const bool g_rewardClaimHookInstalled = [] {
    hle::afterLoad([] {
        hle::hookFunction("__ZN23LogicHomeChangeListener18claimRewardStartedEP10LogicChesti", [] {
            g_rewardClaims.push_back({hle::arg(1), int(hle::arg(2))});
        });
    });
    return true;
}();

// The shop cycle (the server's choice, as on the Node server): one per UTC day; seed = the day number, weekday 1..7
// (1 = Monday), seconds until the next day. Both sides roll the same cards from them.
struct ShopCycle {
    int seed;
    int weekday;
    int secondsLeft;
};

ShopCycle getShopCycle() {
    constexpr int kDaysPerWeek = 7;
    constexpr int kEpochWeekdayOffset = 3; // 1 January 1970 was a Thursday
    const auto now = TimeUtil::getSeconds();
    const auto period = std::int64_t(TimeUtil::getShopPeriodSeconds());
    const auto day = now / period;
    return {int(day), int((day + kEpochWeekdayOffset) % kDaysPerWeek) + 1, int((day + 1) * period - now)};
}

bool isShopCurrent(const LogicClientHome &home, const ShopCycle &cycle) {
    return home.getShopSeed() == cycle.seed && home.getShopWeekday() == cycle.weekday;
}
} // namespace

GameMode::GameMode(MessageManager &player, GameServer &server) : player_(player), server_(server), mode_(false) {
    auto *account = player_.getAccount();
    const auto elapsed = int(std::max<std::int64_t>(0, TimeUtil::getSeconds() - account->savedAt));
    {
        LogicGameMode offline(false);
        offline.loadHomeState(LogicClientHome::decode(account->home), LogicClientAvatar::decode(account->avatar),
                              elapsed, 0);
        const auto cycle = getShopCycle();
        auto home = offline.getHome();
        if (!isShopCurrent(home, cycle)) home.setShopValues(cycle.seed, cycle.secondsLeft, cycle.weekday);
        account->home = home.encode();
        account->avatar = offline.getPlayerAvatar().encode();
        account->savedAt = TimeUtil::getSeconds();
        server_.getAccountManager().saveAccount(*account);
    }
    const int seed = createRandomSeed();
    mode_.loadHomeState(LogicClientHome::decode(account->home), LogicClientAvatar::decode(account->avatar), 0, seed);
    OwnHomeDataMessage ownHomeData;
    ownHomeData.setHome(account->home);
    ownHomeData.setAvatar(account->avatar);
    ownHomeData.setRandomSeed(seed);
    player_.sendMessage(ownHomeData);
}

void GameMode::checkShop() {
    const auto cycle = getShopCycle();
    if (cycle.seed == shopSeedSent_ || isShopCurrent(getHome(), cycle)) return;
    shopSeedSent_ = cycle.seed;
    auto command = LogicShopSeedChangedCommand(cycle.seed, cycle.secondsLeft, cycle.weekday)
                       .createCommand(server_.getNextServerCommandId());
    if (!command) return;
    Debugger::print("server", "%s: new shop (day %d)", player_.getAvatarName().c_str(), cycle.seed);
    sendServerCommand(std::move(command));
}

void GameMode::save() {
    auto *account = player_.getAccount();
    if (!account) return;
    account->home = getHome().encode();
    account->avatar = getAvatar().encode();
    account->savedAt = TimeUtil::getSeconds();
    server_.getAccountManager().saveAccount(*account);
}

// The commands run at their execute ticks in the server's LogicGameMode, which then must match the client's checksum.
void GameMode::endClientTurn(EndClientTurnMessage &message) {
    const auto &peer = player_.getPeer();
    const int clientTick = message.getTick();
    const int clientChecksum = message.getChecksum();
    auto &commands = message.getCommands();
    if (clientTick < mode_.getTick()) {
        Debugger::warning("%s: turn for tick %d is behind the server (%d)", peer.c_str(), clientTick, mode_.getTick());
        return;
    }
    {
        std::string types;
        for (const auto &command : commands)
            types += " " + std::to_string(command.getCommandType()) + "@" + std::to_string(command.getExecuteTick());
        Debugger::print("turn", "%s: tick %d -> %d, checksum %d, commands:%s", peer.c_str(), mode_.getTick(),
                        clientTick, clientChecksum, types.c_str());
    }
    // Server commands come back as acknowledgements; only as many as were sent are accepted. The client's echo only
    // gives the tick: its re-encoding drops data it already applied (a LogicClaimRewardCommand comes back without the
    // reward's cards), so the server runs its own copy of the command it sent.
    std::vector<bool> accepted(commands.size(), true);
    for (std::size_t i = 0; i < commands.size(); ++i) {
        if (!commands[i].isServerCommand()) continue;
        const int type = commands[i].getCommandType();
        if (sentServerCommands_.empty()) {
            Debugger::warning("%s: unexpected server command %d", peer.c_str(), type);
            accepted[i] = false;
            continue;
        }
        auto own = LogicCommand::decode(sentServerCommands_.front());
        sentServerCommands_.pop_front();
        if (!own || own.getCommandType() != type) {
            Debugger::warning("%s: server command %d came back as %d", peer.c_str(), own ? own.getCommandType() : 0,
                              type);
            continue;
        }
        own.setExecuteTick(commands[i].getExecuteTick());
        commands[i] = std::move(own);
    }
    for (std::size_t i = 0; i < commands.size(); ++i)
        if (accepted[i]) server_.getAllianceManager().onHomeCommand(player_, commands[i]);
    g_rewardClaims.clear();
    std::vector<bool> added(commands.size(), false);
    while (mode_.getTick() < clientTick) {
        const int tick = mode_.getTick();
        for (std::size_t i = 0; i < commands.size(); ++i) {
            if (!accepted[i] || added[i] || commands[i].getExecuteTick() != tick) continue;
            mode_.addCommand(std::move(commands[i]));
            added[i] = true;
        }
        mode_.updateOneTick();
    }
    for (std::size_t i = 0; i < commands.size(); ++i)
        if (!added[i] && accepted[i])
            Debugger::warning("%s: command %d for tick %d not executed (turn ends at %d)", peer.c_str(),
                              commands[i].getCommandType(), commands[i].getExecuteTick(), clientTick);
    const int serverChecksum = mode_.calculateChecksum();
    Debugger::print("turn", "%s: server tick %d checksum %d", peer.c_str(), mode_.getTick(), serverChecksum);
    statedump::dump(std::getenv("HLE_DUMP_HOME"), mode_.get(), "server"); // a debugging aid (off unless set)
    if (serverChecksum != clientChecksum) {
        Debugger::warning("%s: out of sync at tick %d (server %d, client %d)", peer.c_str(), clientTick, serverChecksum,
                          clientChecksum);
        OutOfSyncMessage outOfSync;
        outOfSync.setServerChecksum(serverChecksum);
        outOfSync.setClientChecksum(clientChecksum);
        outOfSync.setSubtick(clientTick);
        player_.sendMessage(outOfSync);
    }
    sendRewardClaims();
    save();
}

void GameMode::sendRewardClaims() {
    auto claims = std::move(g_rewardClaims);
    g_rewardClaims.clear();
    for (const auto &claim : claims) {
        auto command = LogicClaimRewardCommand::create(claim.chest, claim.chestType, createRewardSeed(), getHome());
        Debugger::print("server", "%s: %s reward (chest type %d)", player_.getPeer().c_str(),
                        LogicData::getName(LogicChest::getTreasureChestData(claim.chest)).c_str(), claim.chestType);
        sendServerCommand(std::move(command));
    }
}

void GameMode::sendServerCommand(LogicCommand command) {
    sentServerCommands_.push_back(command.encode());
    AvailableServerCommandMessage message;
    message.setServerCommand(std::move(command));
    player_.sendMessage(message);
}

void GameMode::applyServerCommandOffline(GameServer &server, std::int64_t accountId, LogicCommand command) {
    auto *account = server.getAccountManager().getAccount(accountId);
    if (!account) return;
    LogicGameMode mode(false);
    mode.loadHomeState(LogicClientHome::decode(account->home), LogicClientAvatar::decode(account->avatar), 0, 0);
    command.execute(mode);
    // Kept, as it always was: whether the home keeps pointers into an executed command is not known.
    command.release();
    account->home = mode.getHome().encode();
    account->avatar = mode.getPlayerAvatar().encode();
    server.getAccountManager().saveAccount(*account);
}
} // namespace server
