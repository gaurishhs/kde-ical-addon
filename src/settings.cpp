/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "settings.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace
{
constexpr auto kGroupPrefix = "Calendar_";
constexpr auto kKeyName = "name";
constexpr auto kKeyEnabled = "enabled";
constexpr auto kKeyColor = "color";
constexpr auto kKeyOrder = "order";

const QColor kDefaultColors[] = {
    QColor(QStringLiteral("#3daee9")), QColor(QStringLiteral("#2ecc71")), QColor(QStringLiteral("#e67e22")),
    QColor(QStringLiteral("#9b59b6")), QColor(QStringLiteral("#e74c3c")), QColor(QStringLiteral("#16a085")),
};
} // namespace

bool Settings::Snapshot::operator==(const Snapshot &other) const
{
    if (refreshMinutes != other.refreshMinutes || demoMode != other.demoMode || revision != other.revision) {
        return false;
    }
    if (calendars.size() != other.calendars.size()) {
        return false;
    }
    for (int i = 0; i < calendars.size(); ++i) {
        const CalendarSource &a = calendars.at(i);
        const CalendarSource &b = other.calendars.at(i);
        if (a.id != b.id || a.name != b.name || a.color != b.color || a.enabled != b.enabled) {
            return false;
        }
    }
    return true;
}

int Settings::Snapshot::enabledCount() const
{
    int n = 0;
    for (const CalendarSource &c : calendars) {
        if (c.enabled) {
            ++n;
        }
    }
    return n;
}

QString Settings::configFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/icalcalendarrc");
}

QString Settings::cacheDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.cache");
    }
    const QString dir = base + QStringLiteral("/icalcalendar");
    QDir().mkpath(dir);
    // The cached feed is calendar data; keep the directory owner-only.
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return dir;
}

QString Settings::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int Settings::clampRefreshMinutes(int minutes)
{
    if (minutes < 1) {
        return 1;
    }
    if (minutes > 24 * 60) {
        return 24 * 60;
    }
    return minutes;
}

Settings::Snapshot Settings::load()
{
    Snapshot snapshot;
    QSettings settings(configFilePath(), QSettings::IniFormat);

    settings.beginGroup(QStringLiteral("General"));
    snapshot.refreshMinutes = clampRefreshMinutes(settings.value(QStringLiteral("refreshMinutes"), 15).toInt());
    snapshot.demoMode = settings.value(QStringLiteral("demoMode"), false).toBool();
    snapshot.revision = settings.value(QStringLiteral("revision"), 0).toLongLong();
    settings.endGroup();

    // Collect all calendar groups, then restore the user's ordering.
    struct Entry {
        int order = 0;
        CalendarSource source;
    };
    QVector<Entry> entries;

    const QStringList groups = settings.childGroups();
    for (const QString &group : groups) {
        if (!group.startsWith(QLatin1String(kGroupPrefix))) {
            continue;
        }
        settings.beginGroup(group);
        Entry entry;
        entry.source.id = group.mid(int(qstrlen(kGroupPrefix)));
        entry.source.name = settings.value(QLatin1String(kKeyName), QStringLiteral("Calendar")).toString();
        entry.source.enabled = settings.value(QLatin1String(kKeyEnabled), true).toBool();
        entry.source.color = settings.value(QLatin1String(kKeyColor)).toString();
        entry.order = settings.value(QLatin1String(kKeyOrder), 0).toInt();
        settings.endGroup();

        if (entry.source.id.isEmpty()) {
            continue;
        }
        if (entry.source.color.isEmpty()) {
            entry.source.color = kDefaultColors[entries.size() % int(std::size(kDefaultColors))].name();
        }
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.order < b.order;
    });
    for (const Entry &entry : std::as_const(entries)) {
        snapshot.calendars.append(entry.source);
    }
    // QSettings caches per file; sync so an external save is always visible.
    settings.sync();
    return snapshot;
}

void Settings::save(const Snapshot &snapshot)
{
    const QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSettings settings(path, QSettings::IniFormat);

    // Start from a clean slate so that removed calendars really disappear.
    for (const QString &group : settings.childGroups()) {
        settings.remove(group);
    }

    settings.beginGroup(QStringLiteral("General"));
    settings.setValue(QStringLiteral("refreshMinutes"), clampRefreshMinutes(snapshot.refreshMinutes));
    settings.setValue(QStringLiteral("demoMode"), snapshot.demoMode);
    // Always advance the revision so watchers notice a save even when the
    // contents happen to be identical.
    settings.setValue(QStringLiteral("revision"), QDateTime::currentMSecsSinceEpoch());
    settings.endGroup();

    int order = 0;
    for (const CalendarSource &source : snapshot.calendars) {
        settings.beginGroup(QLatin1String(kGroupPrefix) + source.id);
        settings.setValue(QLatin1String(kKeyName), source.name);
        settings.setValue(QLatin1String(kKeyEnabled), source.enabled);
        settings.setValue(QLatin1String(kKeyColor), source.color);
        settings.setValue(QLatin1String(kKeyOrder), order++);
        settings.endGroup();
    }
    settings.sync();
}
