#!/usr/bin/env bash
# Verify installation, discovery, loading and event output.
set -uo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
PLUGIN_DIR="$PREFIX/lib64/qt6/plugins/plasmacalendarplugins"
PLUGIN="$PLUGIN_DIR/helloworldcalendar.so"
ENV_FILE="$HOME/.config/environment.d/20-helloworldcalendar-qt-plugin-path.conf"
BIN="$SRC_DIR/build/plugin_discover"

fail=0
ok()   { echo "  OK   $*"; }
bad()  { echo "  FAIL $*"; fail=1; }

echo "== Installed files"
[ -f "$PLUGIN" ] && ok "$PLUGIN" || bad "missing $PLUGIN"
[ -f "$ENV_FILE" ] && ok "$ENV_FILE" || bad "missing $ENV_FILE (re-run install.sh)"

echo
echo "== Embedded plugin metadata"
if [ -f "$PLUGIN" ]; then
    if strings "$PLUGIN" | grep -q "Hello World Calendar"; then
        ok "KPlugin metadata is embedded in the .so"
    else
        bad "no embedded KPlugin metadata found (AUTOMOC missing?)"
    fi
    if readelf -d "$PLUGIN" | grep -q "libKF6CalendarEvents.so.6"; then
        ok "links against libKF6CalendarEvents.so.6"
    else
        bad "does not link against libKF6CalendarEvents"
    fi
fi

echo
echo "== systemd user environment (this is what reaches plasmashell)"
if systemctl --user show-environment 2>/dev/null | grep -qx "QT_PLUGIN_PATH=$HOME/.local/lib64/qt6/plugins"; then
    ok "QT_PLUGIN_PATH is set in the systemd user manager environment"
else
    echo "  INFO QT_PLUGIN_PATH is not active yet; apply with:"
    echo "       systemctl --user daemon-reexec"
    echo "       systemctl --user restart plasma-plasmashell.service"
fi

echo
echo "== Discovery / loading test (same code path as EventPluginsManager)"
if [ -x "$BIN" ]; then
    # Run the harness from a neutral directory so Qt's "application dir" plugin
    # path does not pick up the copy in the build tree; this must resolve the
    # installed .so in $PREFIX.
    TMPDIR_RUN="$(mktemp -d)"
    cp "$BIN" "$TMPDIR_RUN/"
    QT_PLUGIN_PATH="$PREFIX/lib64/qt6/plugins" "$TMPDIR_RUN/plugin_discover"
    rc=$?
    rm -rf "$TMPDIR_RUN"
    [ $rc -eq 0 ] && ok "plugin_discover exited 0" || bad "plugin_discover exited $rc"
else
    bad "$BIN not built (run install.sh first)"
fi

echo
echo "== Running plasmashell"
SHELL_PID="$(pgrep -x plasmashell | head -1)"
if [ -n "$SHELL_PID" ]; then
    if grep -qi "helloworldcalendar" "/proc/$SHELL_PID/maps" 2>/dev/null; then
        ok "helloworldcalendar.so is loaded in plasmashell (pid $SHELL_PID)"
    else
        echo "  INFO not loaded in the running plasmashell yet."
        echo "       The Digital Clock loads calendar plugins when its popup is first opened."
    fi
else
    echo "  INFO plasmashell is not running"
fi

echo
echo "== Digital Clock configuration"
if grep -q "^enabledCalendarPlugins=.*helloworldcalendar" \
      "$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc" 2>/dev/null; then
    ok "helloworldcalendar is enabled in a Digital Clock applet"
else
    echo "  INFO not enabled yet - run ./enable-in-digital-clock.sh"
fi

echo
if [ $fail -eq 0 ]; then
    echo "RESULT: all checks passed."
else
    echo "RESULT: some checks failed (see above)."
fi
exit $fail
