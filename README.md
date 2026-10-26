# Clash Royale v1.0.0 Server
*Disclaimer: AI was used to create this repo. Mostly the reason why I'm making this open source*

A game server for the original Clash Royale v1.0.0 client, for Windows and Linux. It runs the game library's own logic
(messages, home simulation, commands, checksums, battles) under the ClashRoyaleHLE emulator core, so it behaves
exactly as the client expects.

> This is an unofficial, non-commercial preservation project, not affiliated with or endorsed by Supercell.

## Quick start

- **Windows:** `Run-Server.bat`
- **Linux:** `./run_server.sh`

The server listens on port 9339 (the game) and 9340 (content updates), for this computer and others on the network.

| Windows | Linux | What it does |
| --- | --- | --- |
| `Run-Server.bat` | `./run_server.sh` | Starts the server for this computer and the network |
| `Run-Server-Local.bat` | `./run_server_local.sh` | Starts the server for this computer only |
| `Build-ContentPatch.bat` | `./build_content_patch.sh` | Makes a content patch (see [Content updates](#content-updates)) |
| `Build-Server.bat` | `./build_server.sh` | Recompiles the server from `source/` into `bin/`; restart the server afterwards |
| `Build-Server-Linux.bat` | | Runs `build_server.sh` in WSL, from Windows |

## Features

- New accounts (level 1, Arena 1, tutorial skipped), home, chests and the daily shop
- Clans: chat, roles, card requests and donations, join requests, clan mail, search, friendly battles
- NPC and PvP battles simulated on the server, with desync correction and rejoining after a disconnect
- PvP battles over UDP, as the original game does them (falling back to TCP when UDP cannot get through)
- Battle results, emotes, battle log and replays
- Player and clan leaderboards
- Content updates

## Content updates

The game's own update system: clients whose game files differ from the server's download the changed files at login
from the content server (port 9340), then reload.

1. Edit the master assets in `game/res` (the files the server itself runs on), for example a CSV.
2. Run `Build-ContentPatch.bat` (Linux: `./build_content_patch.sh`). Only changes since the last patch make a new one.
3. Restart the server. Patches are kept in `server-data/content/`.

## Linux

The server needs no graphics or extra libraries: it runs headless (over SSH, or as a service), and Ctrl+C or SIGTERM
stops it cleanly. The included `bin/ClashRoyaleServer` is statically linked, so it runs as it is on any 64-bit x86
Linux. `./build_server.sh` recompiles it from `source/`; this needs GCC 13 or newer, CMake and make, and prints the
install command when something is missing.

## Data

All saved state is in `server-data/`:

- `server.db`: one SQLite database with the accounts, clans, clan chat, battle logs and replays (plus `-wal` and
  `-shm` files while the server runs). Any SQLite tool, for example DB Browser for SQLite, opens it.
- `content/`: the content patches.
- `import/new-account.ownhome`: the template every new account starts from. Keep it.

To reset the server, stop it and delete `server.db*`. The same folder works for the Windows and the Linux server.

## Building from source

Building on Windows needs Visual Studio 2019 or 2022 (or the Build Tools) with *Desktop development with C++*.
Extract the package to a short folder first: the Visual Studio build fails inside
deeply nested folders because of the Windows path length limit.

## Contents

| Folder | Contents |
| --- | --- |
| `bin/` | `ClashRoyaleServer.exe` (Windows) and `ClashRoyaleServer` (Linux) |
| `game/` | The original game: `library` (the iOS executable, whose logic the server runs) and `res/` (its game files, also the master assets for content updates) |
| `server-data/` | Saved state (see [Data](#data)) |
| `source/` | The source code: `server/` (the server), `src/` (the emulator core it runs the game library on), `external/` (dynarmic, Boost headers, stb, SQLite, all compiled in) and `CMakeLists.txt` |
