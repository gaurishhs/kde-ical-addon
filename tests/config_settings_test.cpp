/*
    SPDX-FileCopyrightText: 2026 iCal Calendar addon authors
    SPDX-License-Identifier: GPL-2.0-or-later

    Configuration tests: the INI format used by both the addon and the config
    page, and the list model backing the page.
*/

#include "calendarlistmodel.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QModelIndex>
#include <QSet>
#include <QTemporaryDir>

#include <cstdio>

namespace
{
int g_failures = 0;
int g_checks = 0;

void check(bool condition, const QString &what)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::fprintf(stderr, "FAIL: %s\n", qPrintable(what));
    }
}

CalendarSource makeSource(const QString &id, const QString &name, const QString &color, bool enabled)
{
    CalendarSource source;
    source.id = id;
    source.name = name;
    source.color = color;
    source.enabled = enabled;
    return source;
}

void testSettingsRoundTrip()
{
    Settings::Snapshot snapshot;
    snapshot.refreshMinutes = 42;
    snapshot.demoMode = true;
    snapshot.calendars = {
        makeSource(QStringLiteral("id-b"), QStringLiteral("Work"), QStringLiteral("#112233"), true),
        makeSource(QStringLiteral("id-a"), QStringLiteral("Personal"), QStringLiteral("#445566"), false),
    };

    Settings::save(snapshot);
    check(QFile::exists(Settings::configFilePath()), QStringLiteral("config file created"));

    const Settings::Snapshot loaded = Settings::load();
    check(loaded.refreshMinutes == 42, QStringLiteral("refresh interval round-trips"));
    check(loaded.demoMode, QStringLiteral("demo flag round-trips"));
    check(loaded.calendars.size() == 2, QStringLiteral("both calendars round-trip"));
    check(loaded.calendars.value(0).id == QStringLiteral("id-b"), QStringLiteral("order is preserved (1)"));
    check(loaded.calendars.value(1).id == QStringLiteral("id-a"), QStringLiteral("order is preserved (2)"));
    check(loaded.calendars.value(0).name == QStringLiteral("Work"), QStringLiteral("name round-trips"));
    check(loaded.calendars.value(0).color == QStringLiteral("#112233"), QStringLiteral("colour round-trips"));
    check(!loaded.calendars.value(1).enabled, QStringLiteral("enabled flag round-trips"));
    check(loaded.enabledCount() == 1, QStringLiteral("enabled count"));

    // Saving a smaller list must remove the dropped calendars, not leave them.
    snapshot.calendars.remove(0);
    Settings::save(snapshot);
    const Settings::Snapshot reduced = Settings::load();
    check(reduced.calendars.size() == 1, QStringLiteral("saving removes deleted calendars"));
    check(reduced.calendars.value(0).id == QStringLiteral("id-a"), QStringLiteral("remaining calendar kept"));

    check(Settings::clampRefreshMinutes(0) == 1, QStringLiteral("refresh interval lower bound"));
    check(Settings::clampRefreshMinutes(100000) == 24 * 60, QStringLiteral("refresh interval upper bound"));
    check(!Settings::newId().isEmpty(), QStringLiteral("new ids are generated"));
    check(Settings::newId() != Settings::newId(), QStringLiteral("new ids are unique"));
}

void testListModel()
{
    CalendarListModel model;
    QSet<QString> withUrl{QStringLiteral("id-1")};

    model.resetRows({makeSource(QStringLiteral("id-1"), QStringLiteral("One"), QStringLiteral("#111111"), true),
                     makeSource(QStringLiteral("id-2"), QStringLiteral("Two"), QStringLiteral("#222222"), false)},
                    withUrl);

    check(model.rowCount() == 2, QStringLiteral("model row count"));
    check(model.count() == 2, QStringLiteral("model count property"));
    check(model.data(model.index(0, 0), CalendarListModel::IdRole).toString() == QStringLiteral("id-1"),
          QStringLiteral("id role"));
    check(model.data(model.index(1, 0), CalendarListModel::NameRole).toString() == QStringLiteral("Two"),
          QStringLiteral("name role"));
    check(model.data(model.index(0, 0), CalendarListModel::UrlSetRole).toBool(),
          QStringLiteral("url set role true"));
    check(!model.data(model.index(1, 0), CalendarListModel::UrlSetRole).toBool(),
          QStringLiteral("url set role false"));

    // Role names must not collide with Item properties used by QML delegates.
    const QHash<int, QByteArray> roles = model.roleNames();
    check(roles.value(CalendarListModel::IdRole) == "calendarId", QStringLiteral("id role name"));
    check(roles.value(CalendarListModel::EnabledRole) == "calendarEnabled", QStringLiteral("enabled role name"));
    check(roles.value(CalendarListModel::UrlSetRole) == "calendarHasUrl", QStringLiteral("url role name"));

    model.setName(QStringLiteral("id-2"), QStringLiteral("Renamed"));
    check(model.sourceForId(QStringLiteral("id-2")).name == QStringLiteral("Renamed"),
          QStringLiteral("setName updates the row"));

    model.setEnabled(QStringLiteral("id-2"), true);
    check(model.sourceForId(QStringLiteral("id-2")).enabled, QStringLiteral("setEnabled updates the row"));

    model.setColor(QStringLiteral("id-1"), QStringLiteral("#abcdef"));
    check(model.sourceForId(QStringLiteral("id-1")).color == QStringLiteral("#abcdef"),
          QStringLiteral("setColor updates the row"));

    model.appendRow(makeSource(QStringLiteral("id-3"), QStringLiteral("Three"), QStringLiteral("#333333"), true));
    check(model.rowCount() == 3, QStringLiteral("appendRow adds a row"));

    model.removeRow(QStringLiteral("id-1"));
    check(model.rowCount() == 2, QStringLiteral("removeRow removes a row"));
    check(model.rowForId(QStringLiteral("id-1")) == -1, QStringLiteral("removed row is gone"));
    check(model.rowForId(QStringLiteral("id-3")) == 1, QStringLiteral("remaining rows shift"));

    // setUrlSet must not resurrect the removed id.
    model.setUrlSet({QStringLiteral("id-1")});
    check(!model.data(model.index(0, 0), CalendarListModel::UrlSetRole).toBool(),
          QStringLiteral("url flags only apply to existing rows"));
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    auto *sandbox = new QTemporaryDir();
    qputenv("XDG_CONFIG_HOME", sandbox->filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_CACHE_HOME", sandbox->filePath(QStringLiteral("cache")).toUtf8());
    QDir().mkpath(sandbox->filePath(QStringLiteral("config")));

    testSettingsRoundTrip();
    testListModel();

    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
