# iCal Calendar for KDE Plasma 6

A standalone calendar plugin for the native **Digital Clock** popup. It shows
events from any **private or public iCal/ICS feed** — no OAuth, no Akonadi, no
background daemon, and no system-wide install.

## Features

- One or more `.ics` feeds, public or private, from any calendar provider.
- Asynchronous downloads and `libical` parsing: recurrence, all-day events,
  time zones, `EXDATE` exceptions, cancellations and modifications.
- Works offline from the last cached copy.
- Conditional refresh using `ETag` / `Last-Modified` (no full re-download when
  nothing changed).
- Feed addresses are stored in **KWallet**, never in the config file or logs.
- Read-only: it never creates, edits or deletes events.

## Requirements

Build:

- CMake ≥ 3.22, a C++17 compiler and Extra CMake Modules
- Qt 6: Core, Gui, Network, Qml, Concurrent
- KF6: KDeclarative (CalendarEvents API) and Wallet
- libical

Fedora:

```bash
sudo dnf install cmake gcc-c++ extra-cmake-modules \
  qt6-qtbase-devel qt6-qtdeclarative-devel \
  kf6-kdeclarative-devel kf6-kwallet-devel libical-devel
```

Runtime: Plasma 6 with the Digital Clock applet, KWallet (`kwalletd6`),
Kirigami, KCMUtils and `tzdata` (all normally present with Plasma).

## Quick start

```bash
# 1. Build and test
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

# 2. Install for the current user (no sudo)
./install.sh
```

`install.sh` builds, installs into `~/.local`, writes
`~/.config/environment.d/20-kde-ical-plugin.conf` so Qt finds the plugin and
QML module, and restarts `plasma-plasmashell.service`.

### Enable it

1. Right-click the **Digital Clock** → **Configure Digital Clock…**
2. **Appearance → Calendar plugins** (older Plasma versions: **Calendar**)
3. Move **iCal Calendar** to the enabled list and click **Apply**

### Add a feed

1. Open the clock popup → calendar section → **Configure** next to *iCal Calendar*
2. Paste the calendar's private or public `.ics` address
3. Press **Save** (Enter also works)

The first fetch populates the cache; the popup then renders from cache while
refreshing in the background.

### Uninstall

```bash
./uninstall.sh
```

This removes the installed plugin, QML module and environment file and
restarts plasmashell. Your config (`~/.config/icalcalendarrc`) and feed cache
(`~/.cache/icalcalendar/`) are left behind; delete them if you want a clean
state.

## Private and public feeds

- **Public** feeds (holidays, sports, community calendars, …) work as-is; the
  addon performs a plain HTTPS GET with no credentials.
- **Private** feeds (tokenized URLs) work too. Treat the URL as a secret: it is
  the only authentication. It is stored in KWallet and never shown again.
- Only `https://` is accepted (loopback `http://` is allowed for tests).
  `webcal://` and plain `http://` are rejected — replace `webcal://` with
  `https://`.
- Feeds that require an `Authorization` header, cookies or an interactive login
  are not supported; only URLs whose token is embedded in the address work.
- The plugin is read-only.

## Caching and refresh

- Each calendar has two files in `~/.cache/icalcalendar/`:
  - `<calendar-id>.ics` — the last downloaded feed body
  - `<calendar-id>.json` — `ETag`, `Last-Modified`, fetch time, and a SHA-256
    hash of the URL (the URL itself is never written)
- On startup, or when an address is set, the cached `.ics` is parsed
  immediately, so the popup is never blank and works offline.
- The refresh timer (default **15 minutes**, configurable in the settings page)
  issues a conditional GET:
  - **304 Not Modified** → reuse the cached body without reparsing
  - **200 OK** → save atomically (`QSaveFile`), update the validators, reparse
  - **error** → keep using the cached copy; the error is logged without the URL
- Opening the clock popup also refreshes any feed whose last successful fetch is
  older than the configured interval.
- Changing an address invalidates the stored body and validators (the URL hash
  no longer matches) and forces a fresh download.
- Responses larger than 32 MB are aborted. Redirects are followed only if they
  do not downgrade security (no `https` → `http`).
- The cache directory is owner-only (`0700`). Parsed events for the requested
  month are kept in memory; parsing runs on a single worker thread because
  libical's time-zone state is not fully thread-safe.
- Removing a calendar deletes its KWallet entry. Its cached `.ics`/`.json`
  files may remain on disk and are ignored.

## Development

```bash
./install.sh                 # build + install + restart
ctest --test-dir build       # unit tests
```

### Using a devroot instead of system development packages

If `libical-devel` and/or `kf6-kwallet-devel` are unavailable, point CMake at
a prefix containing their headers and CMake configs (for example a local shim
in `/tmp/devroot/usr`):

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/tmp/devroot/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The prefix must provide `LibIcalConfig.cmake` and `KF6WalletConfig.cmake`.
`/tmp` may be cleared on reboot; re-create the shim or install the packages.

### Build outputs

- `build/plasmacalendarplugins/icalcalendar.so` — the calendar plugin
- `build/qml/org/kde/plasma/icalcalendar/libicalcalendarsettings.so` — config-page backend
- `build/ical_parser_test`, `build/feed_test`, `build/config_settings_test`

### Architecture

| Component | Purpose |
|-----------|---------|
| `src/types.{h,cpp}` | `CalendarSource`, `Occurrence` and date helpers |
| `src/icalparser.{h,cpp}` | `.ics` → `Occurrence` list via libical |
| `src/settings.{h,cpp}` | INI config, paths and refresh clamp |
| `src/secretstore.{h,cpp}` | Async KWallet read/write wrapper |
| `src/feedfetcher.{h,cpp}` | Async HTTP, conditional GET, on-disk cache |
| `src/plugin/icalcalendarplugin.{h,cpp,json}` | `CalendarEventsPlugin` implementation |
| `src/qml/*` | Config-page backend and QML module |
| `tests/*.cpp` | Parser, feed and config unit tests |

## Limitations

- Requires libical and `KF6::Wallet`; on distros without the development
  packages use the devroot prefix described above.
- The config page opens inside the Digital Clock settings dialog; it does not
  launch System Settings.
- Time-zone correctness depends on `tzdata` being present (standard on Linux).
- KWallet must be available and unlocked: feed addresses live there, so without
  it no feed can be loaded.
- Event changes appear after the next refresh, the next popup open, or when the
  settings are applied.
