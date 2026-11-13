#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

PLUGIN_SO="$HOME/.local/lib64/qt6/plugins/plasmacalendarplugins/googlecalendar.so"
PLUGIN_DIR="$HOME/.local/lib64/qt6/plugins/plasmacalendarplugins/googlecalendar"
QML_DIR="$HOME/.local/lib64/qt6/qml/org/kde/plasma/googlecalendar"
ENV_FILE="$HOME/.config/environment.d/20-kde-gcal-plugin.conf"

echo "=== Uninstalling KDE Google Calendar Plugin ==="

rm -f "$PLUGIN_SO"
rm -rf "$PLUGIN_DIR"
rm -rf "$QML_DIR"
rm -f "$ENV_FILE"

systemctl --user daemon-reload
systemctl --user restart plasma-plasmashell.service

echo "Done. Restart required if files are still visible in running session."
