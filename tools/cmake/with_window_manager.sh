#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 command [arguments...]" >&2
    exit 2
fi

# Run inside Xvfb. GLFW caches the window manager's capabilities during init.
openbox --sm-disable >/dev/null 2>&1 &
window_manager_pid=$!
trap 'kill "$window_manager_pid" 2>/dev/null || true' EXIT

for attempt in {1..100}; do
    if xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id'; then
        "$@"
        exit
    fi
    if ! kill -0 "$window_manager_pid" 2>/dev/null; then
        echo "Openbox exited before the window manager was ready" >&2
        exit 1
    fi
    sleep 0.05
done

echo "Timed out waiting for the X11 window manager" >&2
exit 1
