#pragma once
// The library's LogicClientGlobals (client_globals.csv), for the settings both sides of a UDP session use.
namespace server {
namespace LogicClientGlobals {
int getUdpHeartbeatMs();         // UDP_HEARTBEAT_MS: a packet at least this often
int getUdpDisconnectedSeconds(); // UDP_DISCONNECTED_SECONDS: silence after which a session is not valid
} // namespace LogicClientGlobals
} // namespace server
