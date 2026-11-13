# KDE Google Calendar Plugin

A standalone KDE Plasma 6 plugin that shows events from **private Google Calendar iCal feeds** directly in the Digital Clock popup — no OAuth, no Akonadi, no system-wide install.

## How It Works

- Accepts one or more private iCal URLs (the `basic.ics` link from Google Calendar Settings → Integrate calendar).
- Downloads each feed asynchronously using Qt Network; never blocks the UI.
- Parses `.ics` with libical — handles recurrence, all-day events, timezone conversion, EXDATE overrides, cancellations, and modifications.
- Caches feeds locally (`~/.cache/google-calendar-*`) for offline use; refreshes on interval with ETag/Last-Modified conditional requests.
- Stores private URLs only in KWallet (never in config files or logs).

## Dependencies

System packages (one-time):

```bash
sudo dnf install libical-devel kf6-kwallet-devel \
  qt6-qtbase-devel qt6-qtdeclarative-devel \
  extra-cmake-modules kf6-kcoreaddons-devel kf6-kwallet-devel \
  kf6-kcalcore-devel kf6-ki18n-devel kirigami-devel kcmutils-devel
```

No other external dependencies are needed at build time.

## Quick Start (build with local devroot shim)

If you cannot or do not want to install system packages, this repo can be built against a small devroot in `/tmp/devroot`:

```bash
# Set up the shim once (requires root)
sudo mkdir -p /tmp/devroot/usr/lib64/cmake/LibIcal \
              /tmp/devroot/usr/lib64/cmake/KF6Wallet
# Copy the system cmake configs there and fix paths (see scripts/setup_devroot.sh)

# Build
cmake -B build -DCMAKE_PREFIX_PATH=/tmp/devroot/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/tests/ical_parser_test   # 70/70 tests should pass

# Install into ~/.local
./install.sh
```

## Manual Install

```bash
# After building
mkdir -p ~/.local/lib64/qt6/plugins/plasmacalendarplugins/googlecalendar
mkdir -p ~/.local/lib64/qt6/qml/org/kde/plasma/googlecalendar

cp build/plasmacalendarplugins/googlecalendar.so \
   ~/.local/lib64/qt6/plugins/plasmacalendarplugins/
cp src/qml/CalendarConfig.qml \
   ~/.local/lib64/qt6/plugins/plasmacalendarplugins/googlecalendar/
cp build/qml/org/kde/plasma/googlecalendar/libgooglecalendarsettings.so \
   ~/.local/lib64/qt6/qml/org/kde/plasma/googlecalendar/
cp build/qml/org/kde/plasma/googlecalendar/qmldir \
   ~/.local/lib64/qt6/qml/org/kde/plasma/googlecalendar/

# Tell systemd user to pick them up
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/20-kde-gcal-plugin.conf <<'EOF'
QT_PLUGIN_PATH=${HOME}/.local/lib64/qt6/plugins
QML_IMPORT_PATH=${HOME}/.local/lib64/qt6/qml
EOF

systemctl --user daemon-reload
systemctl --user restart plasma-plasmashell
```

## Using the Plugin

1. Open the **Digital Clock** applet.
2. Right‑click → **Configure Digital Clock** → **Appearance** → **Calendar plugins**.
3. Move **Google Calendar (iCal)** to the enabled list and click **Apply**.
4. Click the clock, open the calendar section, click the **Configure** button next to Google Calendar.
5. Add calendars by pasting your private iCal URL (Settings → Calendar name → Integrate calendar → *Secret address in iCal format*).
6. Optionally toggle **Demo mode** to see sample events without any configured calendars.

## Architecture

| Component | Purpose |
|-----------|---------|
| `src/types.{h,cpp}` | Shared types (`Calendar`, `Event`, `FeedCache`) |
| `src/icalparser.{h,cpp}` | Parse `.ics` → `Calendar` objects via libical |
| `src/settings.{h,cpp}` | Read/write config JSON + KWallet access |
| `src/secretstore.{h,cpp}` | Wrap KWallet async read/write calls |
| `src/feedfetcher.{h,cpp}` | Async HTTP with ETag/Last‑Modified + cache |
| `src/plugin/googlecalendarplugin.{h,cpp,json}` | Implements `CalendarEventsPlugin` |
| `src/qml/{calendarsettings,calendarlistmodel}.{h,cpp}, qmlplugin.cpp` | Config‑page backend & QML module |
| `tests/{ical_parser_test,feed_test,config_settings_test}.cpp` | Unit tests (all pass) |

### Build Outputs

- `build/plasmacalendarplugins/googlecalendar.so` — the discovered plugin
- `build/qml/org/kde/plasma/googlecalendar/libgooglecalendarsettings.so` — QML module
- `build/*.so` and test binaries

## Limitations & Known Issues

- Requires libical and KF6::Wallet. If not available on your distro, use the devroot shim.
- Only supports *private* iCal links — public/shared calendars with `public-XXXX` URLs are rejected by design (no authentication means no security boundary).
- The config page opens inside the Digital Clock settings dialog; it does not launch System Settings.
- Timezone correctness depends on the `tzdata` package being present on the host (standard on all Linux distros).
