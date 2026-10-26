#include "logic/gameobject/LogicSummoner.h"

namespace server {
namespace LogicSummoner {
using library::call;
using library::Ptr;

Ptr getDeck(Ptr summoner) { return call("__ZNK13LogicSummoner7getDeckEv", {summoner}); }
int getStars(Ptr summoner, Ptr opponent) {
    return int(call("__ZNK13LogicSummoner8getStarsEPKS_", {summoner, opponent}));
}
} // namespace LogicSummoner
} // namespace server
