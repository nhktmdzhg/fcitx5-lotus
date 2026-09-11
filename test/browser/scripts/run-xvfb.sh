#!/usr/bin/env bash
set -euo pipefail

if [ -z "${TEST_HOME:-}" ]; then
    echo "error: TEST_HOME environment variable is not set. Run setup-fcitx.sh first or export TEST_HOME." >&2
    exit 1
fi

DISPLAY="${DISPLAY:-:99}"
export DISPLAY

PID_FILE="${TEST_HOME}/run-xvfb.pids"
XVFB_LOG="${TEST_HOME}/xvfb.log"
OPENBOX_LOG="${TEST_HOME}/openbox.log"
FCITX_LOG="${TEST_HOME}/fcitx5.log"

if [ "${1:-}" = "--stop" ]; then
    if [ -f "${PID_FILE}" ]; then
        while read -r pid; do
            kill "$pid" 2>/dev/null || true
        done < "${PID_FILE}"
        rm -f "${PID_FILE}"
    fi
    # Also gracefully terminate dbus session if we started it
    if [ -f "${TEST_HOME}/dbus.pid" ]; then
        kill "$(cat "${TEST_HOME}/dbus.pid")" 2>/dev/null || true
    fi
    exit 0
fi

touch "${PID_FILE}"

# Create an isolated, hermetic D-Bus session for our test environment.
# By omitting <standard_session_servicedirs/>, this private bus never scans
# /usr/share/dbus-1/services/ or auto-activates services behind our back.
# This prevents race conditions without modifying any system-wide files.
DBUS_CONF="${TEST_HOME}/dbus-session.conf"
cat <<EOF > "${DBUS_CONF}"
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=${TEST_HOME}</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOF

if ! command -v dbus-daemon >/dev/null 2>&1; then
    echo "error: dbus-daemon binary not found in PATH" >&2
    exit 1
fi

# Launch dbus-daemon with explicit file descriptors:
# --print-address 1 writes address to stdout (redirected to dbus.addr)
# --print-pid 3 writes PID to fd 3 (redirected to dbus.pid)
dbus-daemon --config-file="${DBUS_CONF}" --fork --print-address 1 --print-pid 3 > "${TEST_HOME}/dbus.addr" 3> "${TEST_HOME}/dbus.pid"

DBUS_SESSION_BUS_ADDRESS=$(cat "${TEST_HOME}/dbus.addr" 2>/dev/null || true)
DBUS_PID=$(cat "${TEST_HOME}/dbus.pid" 2>/dev/null || true)
export DBUS_SESSION_BUS_ADDRESS

if [ -z "${DBUS_SESSION_BUS_ADDRESS}" ] || [ -z "${DBUS_PID}" ] || ! kill -0 "${DBUS_PID}" 2>/dev/null; then
    echo "error: dbus-daemon failed to start or write valid address/PID" >&2
    [ -f "${TEST_HOME}/dbus.addr" ] && cat "${TEST_HOME}/dbus.addr" >&2
    [ -f "${TEST_HOME}/dbus.pid" ] && cat "${TEST_HOME}/dbus.pid" >&2
    exit 1
fi

echo "${DBUS_PID}" >> "${PID_FILE}"

if [ -n "${GITHUB_ENV:-}" ]; then
    echo "DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS}" >> "$GITHUB_ENV"
    echo "DISPLAY=${DISPLAY}" >> "$GITHUB_ENV"
fi
# Start Xvfb virtual framebuffer if not already running
if ! (command -v xdpyinfo >/dev/null 2>&1 && xdpyinfo -display "${DISPLAY}" >/dev/null 2>&1); then
    if ! command -v Xvfb >/dev/null 2>&1; then
        echo "error: Xvfb binary not found in PATH" >&2
        exit 1
    fi
    Xvfb "${DISPLAY}" -screen 0 1920x1080x24 -ac +extension GLX +render -noreset > "${XVFB_LOG}" 2>&1 &
    echo $! >> "${PID_FILE}"
    
    xvfb_ready=0
    for _ in $(seq 1 50); do
        if [ -S "/tmp/.X11-unix/X${DISPLAY#:}" ] || (command -v xdpyinfo >/dev/null 2>&1 && xdpyinfo -display "${DISPLAY}" >/dev/null 2>&1); then
            xvfb_ready=1
            break
        fi
        sleep 0.1
    done

    if [ "$xvfb_ready" -ne 1 ]; then
        echo "error: Xvfb failed to start on ${DISPLAY} within 5s" >&2
        [ -f "${XVFB_LOG}" ] && tail -n 50 "${XVFB_LOG}" >&2
        exit 1
    fi
fi

# Start Openbox window manager
if ! command -v openbox >/dev/null 2>&1; then
    echo "error: openbox binary not found in PATH" >&2
    exit 1
fi
openbox --sm-disable > "${OPENBOX_LOG}" 2>&1 &
echo $! >> "${PID_FILE}"

openbox_ready=0
for _ in $(seq 1 30); do
    if kill -0 "$(tail -1 "${PID_FILE}")" 2>/dev/null; then
        openbox_ready=1
        break
    fi
    sleep 0.1
done

if [ "$openbox_ready" -ne 1 ]; then
    echo "error: openbox failed to start within 3s" >&2
    [ -f "${OPENBOX_LOG}" ] && tail -n 50 "${OPENBOX_LOG}" >&2
    exit 1
fi

# Isolated environment for Fcitx5
export HOME="${TEST_HOME}"
export XDG_CONFIG_HOME="${TEST_HOME}/.config"
export XDG_DATA_HOME="${TEST_HOME}/.local/share"

# Input method environment
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export SDL_IM_MODULE=fcitx

# Start Fcitx5 daemon (with retry for transient D-Bus issues)
if ! command -v fcitx5 >/dev/null 2>&1; then
    echo "error: fcitx5 binary not found in PATH" >&2
    exit 1
fi

echo "Starting fcitx5 with DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS}"
echo "XDG_CONFIG_HOME=${XDG_CONFIG_HOME}"

fcitx5_started=0
for attempt in 1 2 3; do
    if [ "${BROWSER_E2E_DEBUG:-0}" = "1" ]; then
        fcitx5 -r --disable=wayland,waylandim --verbose '*=5' > "${FCITX_LOG}" 2>&1 &
    else
        fcitx5 -r --disable=wayland,waylandim > "${FCITX_LOG}" 2>&1 &
    fi
    FCITX_PID=$!
    echo "$FCITX_PID" >> "${PID_FILE}"

    # Wait for Fcitx5 daemon to initialize
    fcitx_ready=0
    for _ in $(seq 1 30); do
        if ! kill -0 "$FCITX_PID" 2>/dev/null; then
            echo "warning: fcitx5 (PID $FCITX_PID) exited prematurely on attempt $attempt" >&2
            [ -f "${FCITX_LOG}" ] && tail -n 20 "${FCITX_LOG}" >&2
            break
        fi
        if command -v fcitx5-remote >/dev/null 2>&1 && fcitx5-remote >/dev/null 2>&1; then
            fcitx_ready=1
            break
        fi
        sleep 0.5
    done

    if [ "$fcitx_ready" -eq 1 ]; then
        fcitx5_started=1
        break
    fi

    # Kill the failed fcitx5 before retrying
    kill "$FCITX_PID" 2>/dev/null || true
    sleep 1
done

if [ "$fcitx5_started" -ne 1 ]; then
    echo "error: fcitx5 daemon failed to start after 3 attempts" >&2
    [ -f "${FCITX_LOG}" ] && tail -n 50 "${FCITX_LOG}" >&2
    exit 1
fi

# Verify lotus input method addon activates successfully
lotus_ready=0
for _ in $(seq 1 30); do
    fcitx5-remote -s lotus >/dev/null 2>&1 || true
    fcitx5-remote -o >/dev/null 2>&1 || true
    if [ "$(fcitx5-remote -n 2>/dev/null || true)" = "lotus" ]; then
        lotus_ready=1
        break
    fi
    sleep 0.2
done

if [ "$lotus_ready" -ne 1 ]; then
    echo "error: fcitx5 failed to activate lotus addon within 6s (current IM: $(fcitx5-remote -n 2>/dev/null || echo 'none'))" >&2
    [ -f "${FCITX_LOG}" ] && tail -n 50 "${FCITX_LOG}" >&2
    exit 1
fi

cat <<EOF > /tmp/x11-env.sh
export DISPLAY="${DISPLAY}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS}"
export HOME="${TEST_HOME}"
export XDG_CONFIG_HOME="${TEST_HOME}/.config"
export XDG_DATA_HOME="${TEST_HOME}/.local/share"
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
EOF

if [ -n "${GITHUB_ENV:-}" ]; then
    echo "HOME=${TEST_HOME}" >> "$GITHUB_ENV"
    echo "XDG_CONFIG_HOME=${TEST_HOME}/.config" >> "$GITHUB_ENV"
    echo "XDG_DATA_HOME=${TEST_HOME}/.local/share" >> "$GITHUB_ENV"
fi
