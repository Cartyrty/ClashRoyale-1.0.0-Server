#include "logic/home/LogicChest.h"

namespace server {
namespace LogicChest {
using library::call;
using library::Ptr;

namespace {
constexpr std::size_t kChestSize = 0x30;
} // namespace

Ptr create(Ptr treasureChestData, int source) {
    const auto chest = library::allocate(kChestSize);
    call("__ZN10LogicChestC1EPK22LogicTreasureChestData", {chest, treasureChestData});
    call("__ZN10LogicChest9setSourceEi", {chest, std::uint64_t(std::uint32_t(source))});
    return chest;
}

Ptr getTreasureChestData(Ptr chest) { return call("__ZNK10LogicChest20getTreasureChestDataEv", {chest}); }
} // namespace LogicChest
} // namespace server
