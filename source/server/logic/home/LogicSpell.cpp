#include "logic/home/LogicSpell.h"

namespace server {
namespace LogicSpell {
library::Ptr clone(library::Ptr spell) { return library::call("__ZNK10LogicSpell5cloneEv", {spell}); }
} // namespace LogicSpell
} // namespace server
