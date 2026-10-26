#include "logic/command/server/LogicClaimRewardCommand.h"

#include "logic/data/LogicData.h"
#include "logic/home/LogicChest.h"

namespace server {
namespace LogicClaimRewardCommand {
using library::call;
using library::Ptr;

namespace {
constexpr std::size_t kRandomizerSize = 0x10;
constexpr std::size_t kCommandSize = 0x38;
// LogicChest: location id +0x10.
constexpr int kChestLocationIdOffset = 0x10;

std::uint64_t argument(int value) { return std::uint64_t(std::uint32_t(value)); }
} // namespace

LogicCommand create(Ptr chest, int chestType, int randomSeed, const LogicClientHome &home) {
    const auto homeCopy = home.clone();
    const auto randomizer = library::allocate(kRandomizerSize);
    call("__ZN21LogicRewardRandomizerC1Ev", {randomizer});
    const auto reward = call("__ZN21LogicRewardRandomizer15randomizeRewardEPK10LogicChestiP15LogicClientHome",
                             {randomizer, chest, argument(randomSeed), homeCopy.get()});
    const auto command = library::allocate(kCommandSize);
    call("__ZN23LogicClaimRewardCommandC1Ev", {command});
    const auto chestData = LogicChest::getTreasureChestData(chest);
    call("__ZN23LogicClaimRewardCommand16setChestGlobalIDEi", {command, argument(LogicData::getGlobalID(chestData))});
    call("__ZN23LogicClaimRewardCommand13setLocationIdEi",
         {command, argument(library::readInt(chest + kChestLocationIdOffset))});
    call("__ZN23LogicClaimRewardCommand12setChestTypeEi", {command, argument(chestType)});
    call("__ZN23LogicClaimRewardCommand7setDataEP11LogicReward", {command, reward});
    call("__ZN21LogicRewardRandomizer8destructEv", {randomizer});
    return LogicCommand(command);
}
} // namespace LogicClaimRewardCommand
} // namespace server
