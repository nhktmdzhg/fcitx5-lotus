#!/usr/bin/env bash
set -euo pipefail

if [ -z "${TEST_HOME:-}" ]; then
    TEST_HOME="$(mktemp -d -t fcitx5-browser-e2e-XXXXXX)"
    echo "WARNING: TEST_HOME not set. Created temporary directory: $TEST_HOME" >&2
fi

export HOME="${TEST_HOME}"
export XDG_CONFIG_HOME="${TEST_HOME}/.config"
CONFIG_ROOT="${XDG_CONFIG_HOME}"
FCITX5_CONFIG_DIR="${CONFIG_ROOT}/fcitx5"
GTK3_CONFIG_DIR="${CONFIG_ROOT}/gtk-3.0"
GTK4_CONFIG_DIR="${CONFIG_ROOT}/gtk-4.0"
OPENBOX_CONFIG_DIR="${CONFIG_ROOT}/openbox"

mkdir -p "${FCITX5_CONFIG_DIR}/conf" "${GTK3_CONFIG_DIR}" "${GTK4_CONFIG_DIR}" "${OPENBOX_CONFIG_DIR}"

# Default to lotus with keyboard-us fallback in standard Fcitx5 order (layout at index 0)
cat <<'EOF' > "${FCITX5_CONFIG_DIR}/profile"
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=lotus

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=lotus
Layout=

[GroupOrder]
0=Default
EOF

# Production default: Telex in Preedit mode
cat <<'EOF' > "${FCITX5_CONFIG_DIR}/conf/lotus.conf"
[InputMethod]
InputMethod=Telex
Mode=Preedit
SpellCheck=True
AutoNonVnRestore=True
DdFreeStyle=True
EOF

# GTK IM module routing
cat <<'EOF' > "${GTK3_CONFIG_DIR}/settings.ini"
[Settings]
gtk-im-module=fcitx
EOF

cat <<'EOF' > "${GTK4_CONFIG_DIR}/settings.ini"
[Settings]
gtk-im-module=fcitx
EOF

# ShareInputState=All and ActiveByDefault ensure fcitx5-remote CLI commands
# immediately affect browser windows without focus-in state resets.
cat <<'EOF' > "${FCITX5_CONFIG_DIR}/config"
[Behavior]
ActiveByDefault=True
ShareInputState=All
resetStateWhenFocusIn=No

[Hotkey]
EnumerateForwardKeys=
EnumerateBackwardKeys=
EOF

# Ensure Openbox automatically focuses and raises new browser windows on Xvfb
cat <<'EOF' > "${OPENBOX_CONFIG_DIR}/rc.xml"
<?xml version="1.0" encoding="UTF-8"?>
<openbox_config xmlns="http://openbox.org/3.4/rc">
  <focus>
    <focusNew>yes</focusNew>
    <followMouse>no</followMouse>
    <focusLast>yes</focusLast>
    <underMouse>no</underMouse>
    <focusDelay>0</focusDelay>
    <raiseOnFocus>yes</raiseOnFocus>
  </focus>
</openbox_config>
EOF
