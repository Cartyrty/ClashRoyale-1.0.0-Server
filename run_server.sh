#!/usr/bin/env bash
# Runs the game server on port 9339 and the content update server on 9340, for this computer and others on the
# network. Saved state is in server-data/. Ctrl+C stops it cleanly.
# It runs from a copy in bin/run, so build_server.sh can replace bin/ClashRoyaleServer while it runs.
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

mkdir -p bin/run
cp bin/ClashRoyaleServer bin/run/ClashRoyaleServer.new && mv -f bin/run/ClashRoyaleServer.new bin/run/ClashRoyaleServer
HLE_QUIET="${HLE_QUIET:-init,bridge,file,data,stdout,objc,macho,patch}" \
    exec ./bin/run/ClashRoyaleServer --library "$PWD/game/library" --host 0.0.0.0 --port 9339 --data "$PWD/server-data" "$@"
