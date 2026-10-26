#pragma once
// The library's LogicJSONObject / LogicJSONArray, as text (writeToString through a StringBuilder).
#include "library/Library.h"

#include <string>

namespace server {
namespace LogicJSON {
library::Ptr createObject();                  // an empty LogicJSONObject
std::string writeObject(library::Ptr object); // "{}" for a null object
std::string writeArray(library::Ptr array);   // "[]" for a null array
} // namespace LogicJSON
} // namespace server
