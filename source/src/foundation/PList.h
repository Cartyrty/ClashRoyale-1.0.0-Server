#pragma once
// XML property lists for NSUserDefaults, the keychain store and -writeToFile:atomically: of collections.
#include "foundation/Foundation.h"
#include <string>
namespace plist {
std::string serialize(ns::id root);
ns::id parse(const std::string &xml, bool mutableContainers = false);
bool writeFile(ns::id root, const std::string &hostPath);
ns::id readFile(const std::string &hostPath, bool mutableContainers = false);
} // namespace plist
