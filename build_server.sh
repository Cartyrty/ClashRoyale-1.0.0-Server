#!/usr/bin/env bash
# Recompiles ClashRoyaleServer for Linux from source/ into bin/ClashRoyaleServer.
#
# Needs: GCC 13 or newer (tested with GCC 13 and 15), CMake 3.16+ and make, plus nothing else (SQLite is compiled in). Missing ones are listed with
# the command that installs them.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
deps="${XDG_DATA_HOME:-$HOME/.local/share}/clashroyale-hle/deps"
modules=""
local_deps=0
[ "${1:-}" = "--local-deps" ] && local_deps=1

say() { printf '%s\n' "$*"; }
missing_tools=()
for tool in cmake make; do command -v "$tool" >/dev/null || missing_tools+=("$tool"); done
[ -n "$modules" ] && { command -v pkg-config >/dev/null || missing_tools+=(pkg-config); }
command -v "${CXX:-g++}" >/dev/null || command -v c++ >/dev/null || missing_tools+=("C++ compiler (g++)")

install_hint() {
    if command -v apt-get >/dev/null; then say "  sudo apt install build-essential cmake"
    elif command -v dnf >/dev/null; then say "  sudo dnf install gcc-c++ make cmake"
    elif command -v pacman >/dev/null; then say "  sudo pacman -S --needed base-devel cmake"
    elif command -v zypper >/dev/null; then say "  sudo zypper install gcc-c++ make cmake"
    else say "  (install CMake, make, a C++ compiler and your distribution's development packages for nothing else (SQLite is compiled in))"
    fi
}
if [ ${#missing_tools[@]} -ne 0 ]; then
    say "Missing tools: ${missing_tools[*]}. Install them, for example:"
    install_hint
    exit 1
fi

# Packages unpacked without root: their pkg-config files, headers and libraries are used from $deps/root.
cmake_extra=()
use_local_deps() {
    local root="$deps/root" lib="$deps/root/usr/lib/x86_64-linux-gnu"
    export PKG_CONFIG_SYSROOT_DIR="$root"
    export PKG_CONFIG_PATH="$lib/pkgconfig:$root/usr/share/pkgconfig"
    cmake_extra=(-DCMAKE_PREFIX_PATH="$root/usr" -DOpenGL_GL_PREFERENCE=LEGACY
                 "-DCMAKE_EXE_LINKER_FLAGS=$(find "$lib" -type d -printf '-Wl,-rpath-link,%p ')")
}
fetch_local_deps() {
    command -v apt-get >/dev/null && command -v dpkg >/dev/null || { say "--local-deps needs apt-get and dpkg (Debian / Ubuntu)."; exit 1; }
    say "Downloading the development packages into $deps (no root needed)..."
    mkdir -p "$deps/debs" "$deps/root"
    local wanted="libsdl2-dev libgl-dev libavformat-dev libavcodec-dev libavutil-dev libswresample-dev"
    apt-cache depends --recurse --no-recommends --no-suggests --no-conflicts --no-breaks --no-replaces --no-enhances $wanted 2>/dev/null |
        grep '^\w' | sort -u > "$deps/all.txt"
    dpkg-query -W -f='${Package}\n' | sort -u > "$deps/installed.txt"
    comm -23 "$deps/all.txt" "$deps/installed.txt" | grep -v ':i386' > "$deps/needed.txt" || true
    (cd "$deps/debs" && apt-get download $(cat "$deps/needed.txt") >/dev/null)
    for deb in "$deps/debs"/*.deb; do dpkg -x "$deb" "$deps/root"; done
    # Symlinks to libraries the system already has (installed packages were not downloaded) point at the system copy.
    local lib="$deps/root/usr/lib/x86_64-linux-gnu"
    find "$lib" -maxdepth 1 -xtype l | while read -r link; do
        target="/usr/lib/x86_64-linux-gnu/$(basename "$(readlink "$link")")"
        [ -e "$target" ] && ln -sf "$target" "$link"
    done
    touch "$deps/ready"
}

if [ -n "$modules" ]; then
    if [ $local_deps -eq 1 ] || [ -f "$deps/ready" ]; then
        [ -f "$deps/ready" ] || fetch_local_deps
        use_local_deps
    elif ! pkg-config --exists $modules 2>/dev/null; then
        say "Missing development packages: $(for m in $modules; do pkg-config --exists "$m" 2>/dev/null || printf '%s ' "$m"; done)"
        say "Install them, for example:"
        install_hint
        if command -v apt-get >/dev/null && command -v dpkg >/dev/null; then
            say "Without root they can be unpacked locally instead; doing that now (or run: $0 --local-deps)."
            fetch_local_deps
            use_local_deps
        else
            exit 1
        fi
    fi
fi

# Building on a Windows drive (WSL's /mnt/c) is slow: the build folder goes to the Linux home folder then.
case "$here" in
    /mnt/*) build="${XDG_CACHE_HOME:-$HOME/.cache}/clashroyale-hle/build-ClashRoyaleServer" ;;
    *) build="$here/source/build-linux" ;;
esac
say "Configuring..."
cmake -S "$here/source" -B "$build" -DCMAKE_BUILD_TYPE=Release -Wno-dev -Wno-deprecated "${cmake_extra[@]}" >/dev/null
say "Compiling (this takes a few minutes the first time)..."
cmake --build "$build" --target ClashRoyaleServer -j"$(nproc 2>/dev/null || echo 4)"
# Copied next to the old file and renamed over it: a running ClashRoyaleServer keeps its old copy.
cp "$build/ClashRoyaleServer" "$here/bin/ClashRoyaleServer.new"
chmod +x "$here/bin/ClashRoyaleServer.new"
mv -f "$here/bin/ClashRoyaleServer.new" "$here/bin/ClashRoyaleServer"
say ""
say "Done: bin/ClashRoyaleServer was rebuilt. Restart run_server.sh to use it."
