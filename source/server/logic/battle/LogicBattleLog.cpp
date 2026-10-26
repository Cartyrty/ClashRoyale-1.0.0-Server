#include "logic/battle/LogicBattleLog.h"

#include "titan/logic/json/LogicJSON.h"

namespace server {
namespace LogicBattleLog {
using library::call;
using library::Ptr;

namespace {
constexpr std::size_t kBattleLogSize = 0x28;
} // namespace

Ptr create(Ptr arenaData) {
    const auto log = library::allocate(kBattleLogSize);
    call("__ZN14LogicBattleLogC1Ev", {log});
    call("__ZN14LogicBattleLog12setArenaDataEPK14LogicArenaData", {log, arenaData});
    return log;
}

std::string getJSON(Ptr battleLog) {
    return LogicJSON::writeObject(call("__ZNK14LogicBattleLog7getJSONEv", {battleLog}));
}
} // namespace LogicBattleLog
} // namespace server
