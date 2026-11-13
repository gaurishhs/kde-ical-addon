#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

PLUGIN_DIR="$HOME/.local/lib64/qt6/plugins/plasmacalendarplugins"
QML_DIR="$HOME/.local/lib64/qt6/qml/org/kde/plasma/googlecalendar"

echo "=== KDE Google Calendar Plugin — Installer ==="

# Build (in-source for now)
if [[ ! -f build/plasmacalendarplugins/googlecalendar.so ]]; then
    echo "==> Building..."
    cmake -B build -DCMAKE_PREFIX_PATH=/tmp/devroot/usr -DCMAKE_BUILD_TYPE=Release .
    cmake --build build -j "$(nproc)"
fi

# Install files
mkdir -p "$PLUGIN_DIR/googlecalendar" "$QML_DIR"

cp build/plasmacalendarplugins/googlecalendar.so "$PLUGIN_DIR/"
cp build/qml/org/kde/plasma/googlecalendar/libgooglecalendarsettings.so "$QML_DIR/"
cp build/qml/org/kde/plasma/googlecalendar/qmldir "$QML_DIR/"
cp src/qml/CalendarConfig.qml "$PLUGIN_DIR/googlecalendar/GoogleCalendarConfig.qml"

echo "==> Installed plugin: $PLUGIN_DIR/googlecalendar.so"
echo "==> Installed QML module: $QML_DIR/libgooglecalendarsettings.so"

# Ensure environment is set
ENV_FILE="$HOME/.config/environment.d/20-kde-gcal-plugin.conf"
cat > "$ENV_FILE" <<'EOF'
QT_PLUGIN_PATH=${HOME}/.local/lib64/qt6/plugins
QML_IMPORT_PATH=${HOME}/.local/lib64/qt6/qml
EOF
systemctl --user daemon-reload
systemctl --user import-environment QT_PLUGIN_PATH QML_IMPORT_PATH || true

# Restart plasmashell so the new plugin is loaded
echo "==> Restarting plasmashell..."
systemctl --user restart plasma-plasmashell.service
sleep 3

echo ""
echo "=== Done ==="
echo "To enable in the Digital Clock:"
echo "  1. Right-click the clock → Configure Digital Clock → Appearance → Calendar plugins"
echo "  2. Move 'Google Calendar (iCal)' from left list to right list"
echo "  3. Click Apply"
echo ""
echo "To add your calendar:"
echo "  Click the clock → Calendar plugins → Google Calendar (iCal) → Configure"
echo "  Paste your private iCal URL (Settings → Calendar → Integrate calendar → Secret address in iCal format)"
