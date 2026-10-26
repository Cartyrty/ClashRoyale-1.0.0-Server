#pragma once
// Loading the game's data as the client's loading screen does: the CSV tables through the ResourceManager into
// LogicResources, and later single resources (a battle location's tilemap).
#include "library/Library.h"

#include <string>

namespace server {
namespace LogicResources {
void loadDataTables();
void loadFile(const std::string &path);       // once per path
library::Ptr getCSV(const std::string &path); // ResourceManager::getCSV, 0 when not loaded
// A location's tilemap (SectorManager::loadTilemap): its CSV, then LogicDataTables::initTilemap. Once per location.
void prepareLocation(library::Ptr locationData);
} // namespace LogicResources
} // namespace server
