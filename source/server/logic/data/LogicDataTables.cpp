#include "logic/data/LogicDataTables.h"

#include "logic/data/GlobalID.h"
#include "logic/data/LogicData.h"

namespace server {
using library::call;
using library::Ptr;

namespace LogicDataTables {
namespace {
constexpr int kMaxGlobalRows = 400;
} // namespace

Ptr getDataById(int globalId) {
    return call("__ZN15LogicDataTables11getDataByIdEi", {std::uint64_t(std::uint32_t(globalId))});
}

int getIntGlobal(const char *name) {
    for (int i = 0; i < kMaxGlobalRows; ++i) {
        const auto data = getDataById(GlobalID::createGlobalID(kGlobalsClass, i));
        if (!data) break;
        if (LogicData::getName(data) == name) return LogicData::getIntegerValue(data, "NumberValue");
    }
    return 0;
}

Ptr getArenaByName(const std::string &name) {
    return call("__ZN15LogicDataTables14getArenaByNameERK6StringPK9LogicData", {library::newString(name), 0});
}

Ptr getLocationByName(const std::string &name) {
    return call("__ZN15LogicDataTables17getLocationByNameERK6StringPK9LogicData", {library::newString(name), 0});
}

Ptr getPredefinedDeckByName(const std::string &name) {
    return call("__ZN15LogicDataTables23getPredefinedDeckByNameERK6StringPK9LogicData", {library::newString(name), 0});
}

Ptr getGoldData() { return call("__ZN15LogicDataTables11getGoldDataEv"); }
Ptr getFreeGoldData() { return call("__ZN15LogicDataTables15getFreeGoldDataEv"); }

void initTilemap(Ptr csvNode, int locationInstanceId) {
    call("__ZN15LogicDataTables11initTilemapEPK7CSVNodei", {csvNode, std::uint64_t(std::uint32_t(locationInstanceId))});
}
} // namespace LogicDataTables
} // namespace server
