#include "logic/data/LogicResources.h"

#include "Cpu.h"
#include "logic/data/LogicData.h"
#include "logic/data/LogicDataTables.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>
#include <set>
#include <vector>

namespace server {
namespace LogicResources {
using library::call;
using library::Ptr;

namespace {
constexpr std::size_t kDataLoaderFactorySize = 8;
constexpr std::size_t kResourceListenerSize = 0x28;
constexpr std::size_t kStringSize = 0x20;
// ResourceListener::addFile's four optional ints (none given).
constexpr std::uint64_t kNone = ~0ull;

Ptr newResourceListener() {
    const auto listener = library::allocate(kResourceListenerSize);
    call("__ZN16ResourceListenerC1Ev", {listener});
    return listener;
}

void addFile(Ptr listener, Ptr fileName) {
    call("__ZN16ResourceListener7addFileERK6Stringiiii", {listener, fileName, kNone, kNone, kNone, kNone});
}

void loadAll(Ptr listener) {
    call("__ZN16ResourceListener12startLoadingEv", {listener});
    while (call("__ZN15ResourceManager14resourceToLoadEv") != 0) call("__ZN15ResourceManager16loadNextResourceEv");
}

// ResourceManager::init(factory, Path::dataPath().c_str()), as GameMain does.
void initResourceManager() {
    const auto factory = library::allocate(kDataLoaderFactorySize);
    call("__ZN17DataLoaderFactoryC1Ev", {factory});
    // Path::dataPath returns its String by value: through x8, cpu::call's ninth argument.
    const auto dataPath = library::allocate(kStringSize);
    cpu::call(library::symbol("__ZN4Path8dataPathEv"), {0, 0, 0, 0, 0, 0, 0, 0, dataPath});
    const auto byteCount = std::uint32_t(library::readInt(dataPath + 4));
    const auto characters = byteCount + 1 > 8 ? library::readPointer(dataPath + 8) : dataPath + 8;
    Debugger::print("server", "data path \"%s\"", library::readString(dataPath).c_str());
    call("__ZN15ResourceManager4initEP17DataLoaderFactoryPKc", {factory, characters});
}
} // namespace

// Every LogicDataTableResource goes into a ResourceListener, the ResourceManager loads them, then
// LogicResources::load(array, index, CSVNode, nullptr, false) builds each table.
void loadDataTables() {
    initResourceManager();
    const auto listener = newResourceListener();
    const auto tables = call("__ZN14LogicResources29createDataTableResourcesArrayEv");
    const int count = library::arrayCount(tables);
    auto fileNameOf = [&](int index) {
        return call("__ZNK22LogicDataTableResource11getFileNameEv", {library::arrayItem(tables, index)});
    };
    for (int i = 0; i < count; ++i) addFile(listener, fileNameOf(i));
    loadAll(listener);
    for (int i = 0; i < count; ++i) {
        const auto fileName = fileNameOf(i);
        const auto csv = call("__ZN15ResourceManager6getCSVERK6String", {fileName});
        if (!csv) Debugger::error("data table %s did not load", library::readString(fileName).c_str());
        call("__ZN14LogicResources4loadEPK14LogicArrayListIP22LogicDataTableResourceEiPK7CSVNodePK15LogicJSONObjectb",
             {tables, std::uint64_t(i), csv, 0, 0});
    }
    Debugger::print("server", "%d data tables loaded", count);
}

void loadFile(const std::string &path) {
    static std::set<std::string> loaded;
    if (!loaded.insert(path).second) return;
    const auto listener = newResourceListener();
    addFile(listener, library::newString(path));
    loadAll(listener);
}

Ptr getCSV(const std::string &path) {
    return call("__ZN15ResourceManager6getCSVERK6String", {library::newString(path)});
}

void prepareLocation(Ptr locationData) {
    static std::vector<Ptr> prepared;
    if (std::find(prepared.begin(), prepared.end(), locationData) != prepared.end()) return;
    prepared.push_back(locationData);
    const auto file = LogicData::getTilemapFileName(locationData);
    loadFile(file);
    const auto csv = getCSV(file);
    if (!csv) {
        Debugger::warning("tilemap %s missing", file.c_str());
        return;
    }
    LogicDataTables::initTilemap(csv, LogicData::getInstanceID(locationData));
}
} // namespace LogicResources
} // namespace server
