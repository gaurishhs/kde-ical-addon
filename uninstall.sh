#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

PLUGIN_SO="$HOME/.local/lib64/qt6/plugins/plasmacalendarplugins/icalcalendar.so"
PLUGIN_DIR="$HOME/.local/lib64/qt6/plugins/plasmacalendarplugins/icalcalendar"
QML_DIR="$HOME/.local/lib64/qt6/qml/org/kde/plasma/icalcalendar"
ENV_FILE="$HOME/.config/environment.d/20-kde-ical-plugin.conf"

echo "=== Uninstalling KDE iCal Calendar Plugin ==="

rm -f "$PLUGIN_SO"
rm -rf "$PLUGIN_DIR"
rm -rf "$QML_DIR"
rm -f "$ENV_FILE"

systemctl --user daemon-reload
systemctl --user restart plasma-plasmashell.service

echo "Done. Restart required if files are still visible in running session."
