#include "logic/data/LogicClientGlobals.h"

#include "library/Library.h"

namespace server {
namespace LogicClientGlobals {
namespace {
// LogicClientGlobals: UDP_HEARTBEAT_MS +0x58, UDP_DISCONNECTED_SECONDS +0x5c (the fields the client's
// MessageManager reads when it opens its UDP socket). The CSV's values are the defaults when the table is missing.
constexpr int kUdpHeartbeatMsOffset = 0x58;
constexpr int kUdpDisconnectedSecondsOffset = 0x5c;
constexpr int kDefaultUdpHeartbeatMs = 200;
constexpr int kDefaultUdpDisconnectedSeconds = 10;

int readGlobal(int offset, int fallback) {
    const auto globals = library::call("__ZN15LogicDataTables16getClientGlobalsEv");
    return globals ? library::readInt(globals + offset) : fallback;
}
} // namespace

int getUdpHeartbeatMs() { return readGlobal(kUdpHeartbeatMsOffset, kDefaultUdpHeartbeatMs); }
int getUdpDisconnectedSeconds() { return readGlobal(kUdpDisconnectedSecondsOffset, kDefaultUdpDisconnectedSeconds); }
} // namespace LogicClientGlobals
} // namespace server
