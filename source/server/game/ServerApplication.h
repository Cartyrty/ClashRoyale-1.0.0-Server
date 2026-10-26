#pragma once
// The server's main loop: accepts connections on the game port, reads their messages, serves content updates, and
// runs the GameServer's updates (battles advance in 50 ms ticks) between socket events.
#include "game/ServerConfig.h"

namespace server {
namespace ServerApplication {
int run(const ServerConfig &config); // the process exit code
// Set by SIGINT / SIGTERM (Ctrl+C): the loop stops, every player is saved and the database is closed.
void requestStop();
} // namespace ServerApplication
} // namespace server
