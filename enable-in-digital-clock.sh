#!/usr/bin/env python3
# Add "helloworldcalendar" to the enabledCalendarPlugins list of every
# Digital Clock applet in the user's plasmashell configuration.
#
# The config group is discovered from the installed applet's main.xml instead of
# being assumed (in Plasma 6.8 "enabledCalendarPlugins" lives in the
# [Appearance] group, not [General]).
#
# This only edits the user's own ~/.config file (with a backup). It does not
# touch any system file. Plasma must be restarted afterwards to re-read it.

import os
import re
import shutil
import sys
import time
import xml.etree.ElementTree as ET

PLUGIN_ID = "helloworldcalendar"
CONFIG = os.environ.get(
    "APPLETSRC",
    os.path.expanduser("~/.config/plasma-org.kde.plasma.desktop-appletsrc"),
)
KEY = "enabledCalendarPlugins"

MAIN_XML_CANDIDATES = [
    "/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock/contents/config/main.xml",
    "/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock/metadata.json",
]


def find_config_group():
    for path in MAIN_XML_CANDIDATES:
        if not path.endswith(".xml") or not os.path.exists(path):
            continue
        try:
            tree = ET.parse(path)
        except ET.ParseError:
            continue
        root = tree.getroot()
        ns = {"k": "http://www.kde.org/standards/kcfg/1.0"}
        for group in root.findall("k:group", ns):
            for entry in group.findall("k:entry", ns):
                if entry.get("name") == KEY:
                    return group.get("name"), path
        # Entry at top level -> default group
        for entry in root.findall("k:entry", ns):
            if entry.get("name") == KEY:
                return "General", path
    # Plasma 6.8 ships the Digital Clock as a C++/QML applet, so its kcfg file
    # is compiled into the binary and not present on disk. Upstream
    # applets/digital-clock/main.xml at v6.8.2 defines this key inside the
    # <group name="Appearance"> block.
    return "Appearance", "(not on disk; Plasma 6.8 upstream default)"


GROUP, source = find_config_group()
print(f"config group for {KEY}: [{GROUP}]  (from {source})")

if not os.path.exists(CONFIG):
    sys.exit(f"error: {CONFIG} not found (is Plasma configured?)")

with open(CONFIG, "r", encoding="utf-8") as fh:
    lines = fh.read().splitlines()

# Find applet header sections: [Containments][N][Applets][M]
section_re = re.compile(r"^\[Containments\]\[(\d+)\]\[Applets\]\[(\d+)\]$")

applet_ids = []
current = None
for line in lines:
    m = section_re.match(line.strip())
    if m:
        current = (m.group(1), m.group(2))
        continue
    stripped = line.strip()
    if stripped.startswith("[") and stripped.endswith("]"):
        current = None
        continue
    if current and stripped == "plugin=org.kde.plasma.digitalclock":
        applet_ids.append(current)

if not applet_ids:
    sys.exit("error: no Digital Clock applet found in plasmashell config")

backup = CONFIG + ".bak-helloworldcalendar-" + time.strftime("%Y%m%d%H%M%S")
shutil.copy2(CONFIG, backup)
print(f"backup: {backup}")

changed = False

for containment, applet in applet_ids:
    header = f"[Containments][{containment}][Applets][{applet}][Configuration][{GROUP}]"

    start = None
    for i, line in enumerate(lines):
        if line.strip() == header:
            start = i
            break

    if start is None:
        if lines and lines[-1].strip() != "":
            lines.append("")
        lines.append(header)
        lines.append(f"{KEY}={PLUGIN_ID}")
        changed = True
        print(f"applet {containment}/{applet}: created [{GROUP}] with {KEY}={PLUGIN_ID}")
        continue

    end = len(lines)
    for i in range(start + 1, len(lines)):
        s = lines[i].strip()
        if s.startswith("[") and s.endswith("]"):
            end = i
            break

    body = lines[start + 1:end]

    idx = None
    for i, line in enumerate(body):
        if line.strip().startswith(KEY + "="):
            idx = i
            break

    if idx is None:
        body.append(f"{KEY}={PLUGIN_ID}")
        changed = True
        print(f"applet {containment}/{applet}: added {KEY}={PLUGIN_ID}")
    else:
        value = body[idx].split("=", 1)[1]
        ids = [x for x in value.split(",") if x]
        if PLUGIN_ID in ids:
            print(f"applet {containment}/{applet}: already enabled")
            continue
        ids.append(PLUGIN_ID)
        body[idx] = f"{KEY}=" + ",".join(ids)
        changed = True
        print(f"applet {containment}/{applet}: {KEY}={body[idx].split('=', 1)[1]}")

    lines[start + 1:end] = body

if not changed:
    print("Nothing to do.")
else:
    with open(CONFIG, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    print("\nSaved. Restart plasmashell for the change to take effect:")
    print("    systemctl --user restart plasma-plasmashell.service")
