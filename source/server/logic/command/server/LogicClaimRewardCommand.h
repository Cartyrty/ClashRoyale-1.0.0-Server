#pragma once
// The reward of a chest that started claiming it (LogicHomeChangeListener::claimRewardStarted). Unlike the other
// server commands it is built with the library's setters: the reward (LogicReward) is the library's own object.
#include "logic/command/LogicCommand.h"
#include "logic/home/LogicClientHome.h"

namespace server {
namespace LogicClaimRewardCommand {
// The reward comes from LogicRewardRandomizer::randomizeReward(chest, seed, home) on a copy of the home (the
// randomizer moves the free and crown chest diamond counters; the client's home does not).
LogicCommand create(library::Ptr chest, int chestType, int randomSeed, const LogicClientHome &home);
} // namespace LogicClaimRewardCommand
} // namespace server
