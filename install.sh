#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_DIR="$HOME/.local/lib64/qt6/plugins/plasmacalendarplugins"
QML_DIR="$HOME/.local/lib64/qt6/qml/org/kde/plasma/icalcalendar"

# Files are installed with an atomic rename. plasmashell mmaps the plugin .so;
# truncating and rewriting that inode in place crashes the running process.
atomic_install() {
    local src="$1" dst="$2"
    [[ -f "$src" ]] || { echo "missing: $src" >&2; exit 1; }
    mkdir -p "$(dirname "$dst")"
    cp -f -- "$src" "$dst.tmp.$$"
    mv -f -- "$dst.tmp.$$" "$dst"
}

echo "=== KDE iCal Calendar — Installer ==="

echo "==> Building..."
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_PREFIX_PATH=/tmp/devroot/usr -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" -j "$(nproc)"

atomic_install "$ROOT/build/plasmacalendarplugins/icalcalendar.so" "$PLUGIN_DIR/icalcalendar.so"
atomic_install "$ROOT/build/qml/org/kde/plasma/icalcalendar/libicalcalendarsettings.so" \
               "$QML_DIR/libicalcalendarsettings.so"
atomic_install "$ROOT/build/qml/org/kde/plasma/icalcalendar/qmldir" "$QML_DIR/qmldir"
atomic_install "$ROOT/src/qml/CalendarConfig.qml" "$PLUGIN_DIR/icalcalendar/ICalCalendarConfig.qml"

echo "==> Installed plugin: $PLUGIN_DIR/icalcalendar.so"
echo "==> Installed QML module: $QML_DIR/libicalcalendarsettings.so"

# Ensure environment is set
ENV_FILE="$HOME/.config/environment.d/20-kde-ical-plugin.conf"
cat > "$ENV_FILE" <<'EOF'
QT_PLUGIN_PATH=${HOME}/.local/lib64/qt6/plugins
QML_IMPORT_PATH=${HOME}/.local/lib64/qt6/qml
EOF
systemctl --user daemon-reload
systemctl --user import-environment QT_PLUGIN_PATH QML_IMPORT_PATH || true

# Restart plasmashell so the new plugin is loaded
echo "==> Restarting plasmashell..."
systemctl --user reset-failed plasma-plasmashell.service 2>/dev/null || true
systemctl --user restart plasma-plasmashell.service
sleep 3

echo ""
echo "=== Done ==="
echo "To enable in the Digital Clock:"
echo "  1. Right-click the clock → Configure Digital Clock → Appearance → Calendar plugins"
echo "  2. Move 'iCal Calendar' from left list to right list"
echo "  3. Click Apply"
echo ""
echo "To add your calendar:"
echo "  Click the clock → Calendar plugins → iCal Calendar → Configure"
echo "  Paste the calendar's private or public iCal address (the .ics URL)"
