#!/usr/bin/env bash
# Makes a new content patch from the master assets: game/res (the files the server runs on), or the folder given as
# the first argument. Clients with other content download the changed files at their next login. Only changes since
# the last patch make a new one. Restart the server afterwards: it loads the newest patch when it starts.
cd "$(dirname "${BASH_SOURCE[0]}")"

deps="${XDG_DATA_HOME:-$HOME/.local/share}/clashroyale-hle/deps/root/usr/lib/x86_64-linux-gnu"
if [ -d "$deps" ]; then
    export LD_LIBRARY_PATH="$(find "$deps" -type d | tr '\n' ':')${LD_LIBRARY_PATH:-}"
fi
if [ ! -x "bin/ClashRoyaleServer" ]; then
    echo "bin/ClashRoyaleServer is missing: run ./build_server.sh first."
    exit 1
fi
missing="$(ldd "bin/ClashRoyaleServer" 2>&1 | grep -E 'not found' || true)"
if [ -n "$missing" ]; then
    echo "bin/ClashRoyaleServer was built on a newer Linux than this one and cannot start here:"
    echo "$missing"
    echo "Rebuild it for this computer with ./build_server.sh"
    exit 1
fi

master="${1:-$PWD/game/res}"
exec ./bin/ClashRoyaleServer --library "$PWD/game/library" --build-content "$master" --data "$PWD/server-data"
