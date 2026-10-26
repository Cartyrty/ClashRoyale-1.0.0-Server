#!/usr/bin/env bash
# Same as run_server.sh, but only this computer can connect (127.0.0.1).
exec "$(dirname "${BASH_SOURCE[0]}")/run_server.sh" --host 127.0.0.1 "$@"
