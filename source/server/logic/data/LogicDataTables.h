#pragma once
// The library's LogicDataTables: every CSV table, by global id or by name.
#include "library/Library.h"

#include <string>

namespace server {
namespace LogicDataTables {
// Class ids of the CSV tables the server refers to (LogicDataTables / GlobalID).
constexpr int kGlobalsClass = 3;
constexpr int kNpcClass = 18;
constexpr int kAllianceBadgeClass = 16;
constexpr int kArenaClass = 54;
constexpr int kRegionClass = 57;

library::Ptr getDataById(int globalId);
int getIntGlobal(const char *name); // globals.csv NumberValue, 0 when absent
library::Ptr getArenaByName(const std::string &name);
library::Ptr getLocationByName(const std::string &name);
library::Ptr getPredefinedDeckByName(const std::string &name);
library::Ptr getGoldData();
library::Ptr getFreeGoldData();
void initTilemap(library::Ptr csvNode, int locationInstanceId);
} // namespace LogicDataTables
} // namespace server
