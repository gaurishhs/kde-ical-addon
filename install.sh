#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 iCal Calendar addon authors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# One-step user-local installer: configure, build, test, install and restart
# plasmashell. It does not modify the Digital Clock configuration; enabling the
# addon is left to the user.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${HOME}/.local"
BUILD_DIR="${ROOT}/build"
DEVROOT=""
RUN_TESTS=1
RESTART=1
JOBS="$(nproc 2>/dev/null || echo 2)"

usage() {
    cat <<EOF
Usage: ./install.sh [options]

  --prefix DIR       Install prefix (default: \$HOME/.local)
  --build-dir DIR    Build directory (default: <repo>/build)
  --devroot DIR      CMake prefix containing LibIcal/KF6Wallet configs
                     (default: auto-detect /tmp/devroot/usr)
  --no-tests         Skip running the unit tests
  --no-restart       Do not restart plasmashell
  -j, --jobs N       Parallel build jobs (default: nproc)
  -h, --help         Show this help
EOF
}

while (($#)); do
    case "$1" in
        --prefix)     PREFIX="$2"; shift 2 ;;
        --build-dir)  BUILD_DIR="$2"; shift 2 ;;
        --devroot)    DEVROOT="$2"; shift 2 ;;
        --no-tests)   RUN_TESTS=0; shift ;;
        --no-restart) RESTART=0; shift ;;
        -j|--jobs)    JOBS="$2"; shift 2 ;;
        -h|--help)    usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

die() { echo "ERROR: $*" >&2; exit 1; }

command -v cmake >/dev/null || die "cmake is not installed."
command -v c++ >/dev/null || die "No C++ compiler found."

# ---------------------------------------------------------------------------
# Pick a CMake prefix. Prefer system development packages; fall back to a
# devroot that provides LibIcal and KF6Wallet configs.
# ---------------------------------------------------------------------------
if [[ -z "$DEVROOT" ]]; then
    need_devroot=0
    [[ -f /usr/lib64/cmake/LibIcal/LibIcalConfig.cmake || -f /usr/lib/cmake/LibIcal/LibIcalConfig.cmake ]] || need_devroot=1
    [[ -f /usr/lib64/cmake/KF6Wallet/KF6WalletConfig.cmake || -f /usr/lib/cmake/KF6Wallet/KF6WalletConfig.cmake ]] || need_devroot=1
    if (( need_devroot )); then
        for candidate in /tmp/devroot/usr "${HOME}/.local/devroot/usr"; do
            if [[ -f "${candidate}/lib64/cmake/LibIcal/LibIcalConfig.cmake" \
               && -f "${candidate}/lib64/cmake/KF6Wallet/KF6WalletConfig.cmake" ]]; then
                DEVROOT="$candidate"
                break
            fi
        done
    fi
fi

CMAKE_PREFIX_ARGS=()
if [[ -n "$DEVROOT" ]]; then
    echo "==> Using development prefix: $DEVROOT"
    CMAKE_PREFIX_ARGS=("-DCMAKE_PREFIX_PATH=${DEVROOT}")
fi

# ---------------------------------------------------------------------------
# Configure / build / test
# ---------------------------------------------------------------------------
echo "==> Configuring..."
if ! cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release "${CMAKE_PREFIX_ARGS[@]}"; then
    cat >&2 <<EOF

CMake configuration failed. Install the development packages, or provide a
prefix that contains LibIcalConfig.cmake and KF6WalletConfig.cmake:

  Fedora:
    sudo dnf install cmake gcc-c++ extra-cmake-modules \\
      qt6-qtbase-devel qt6-qtdeclarative-devel \\
      kf6-kdeclarative-devel kf6-kwallet-devel libical-devel

  Or:
    ./install.sh --devroot /path/to/prefix/usr
EOF
    exit 1
fi

echo "==> Building (${JOBS} jobs)..."
cmake --build "$BUILD_DIR" -j "$JOBS"

if (( RUN_TESTS )); then
    echo "==> Running tests..."
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

# ---------------------------------------------------------------------------
# Install (atomic rename: never rewrite an .so that plasmashell has mapped)
# ---------------------------------------------------------------------------
ADDON_DIR="${PREFIX}/lib64/qt6/plugins/plasmacalendarplugins"
QML_DIR="${PREFIX}/lib64/qt6/qml/org/kde/plasma/icalcalendar"

atomic_install() {
    local src="$1" dst="$2"
    [[ -f "$src" ]] || die "missing build output: $src"
    mkdir -p "$(dirname "$dst")"
    cp -f -- "$src" "${dst}.tmp.$$"
    mv -f -- "${dst}.tmp.$$" "$dst"
}

echo "==> Installing into ${PREFIX}..."
atomic_install "${BUILD_DIR}/plasmacalendarplugins/icalcalendar.so" "${ADDON_DIR}/icalcalendar.so"
atomic_install "${BUILD_DIR}/qml/org/kde/plasma/icalcalendar/libicalcalendarsettings.so" \
               "${QML_DIR}/libicalcalendarsettings.so"
atomic_install "${BUILD_DIR}/qml/org/kde/plasma/icalcalendar/qmldir" "${QML_DIR}/qmldir"
atomic_install "${ROOT}/src/qml/CalendarConfig.qml" \
               "${ADDON_DIR}/icalcalendar/ICalCalendarConfig.qml"

# Make Qt find the user-local addon and QML module.
ENV_DIR="${HOME}/.config/environment.d"
ENV_FILE="${ENV_DIR}/20-kde-ical-addon.conf"
mkdir -p "$ENV_DIR"
cat > "$ENV_FILE" <<EOF
QT_PLUGIN_PATH=${PREFIX}/lib64/qt6/plugins
QML_IMPORT_PATH=${PREFIX}/lib64/qt6/qml
EOF

# Refresh the user session so plasmashell sees the new paths.
if command -v systemctl >/dev/null; then
    systemctl --user daemon-reload 2>/dev/null || true
    systemctl --user import-environment QT_PLUGIN_PATH QML_IMPORT_PATH 2>/dev/null || true
fi

if (( RESTART )); then
    if command -v systemctl >/dev/null && systemctl --user is-active --quiet plasma-plasmashell.service; then
        echo "==> Restarting plasmashell..."
        systemctl --user reset-failed plasma-plasmashell.service 2>/dev/null || true
        systemctl --user restart plasma-plasmashell.service
        sleep 2
    else
        echo "==> plasmashell is not running under systemd; restart it manually."
    fi
fi

cat <<EOF

=== Installed ===
  addon:    ${ADDON_DIR}/icalcalendar.so
  QML:      ${QML_DIR}/libicalcalendarsettings.so
  env:      ${ENV_FILE}
  config:   ${HOME}/.config/icalcalendarrc
  cache:    ${HOME}/.cache/icalcalendar/

To enable it yourself:
  1. Right-click the Digital Clock → Configure Digital Clock…
  2. Appearance → Calendar plugins
  3. Move "iCal Calendar Addon" to the enabled list and click Apply

Then open the clock popup → calendar → Configure next to "iCal Calendar Addon"
and paste a private or public .ics address.

Uninstall with ./uninstall.sh
EOF
