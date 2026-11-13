/*
    SPDX-FileCopyrightText: 2026 Hello World Calendar Plugin authors

    SPDX-License-Identifier: GPL-2.0-or-later

    Reproduces, outside of plasmashell, exactly what EventPluginsManager does:

      1. KPluginMetaData::findPlugins("plasmacalendarplugins")
      2. QPluginLoader("plasmacalendarplugins/" + pluginId)
      3. qobject_cast<CalendarEvents::CalendarEventsPlugin *>(instance)
      4. call loadEventsForDateRange() and wait for dataReady()

    Exit codes:
      0 success
      2 plugin not found by KPluginMetaData
      3 QPluginLoader failed
      4 qobject_cast failed (interface mismatch)
      5 timed out waiting for dataReady()
      6 no events returned
*/

#include <CalendarEvents/CalendarEventsPlugin>

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QPluginLoader>
#include <QTimer>

#include <KPluginMetaData>

#include <cstdio>

static int fail(int code, const QString &message)
{
    std::fprintf(stderr, "FAIL: %s\n", qPrintable(message));
    return code;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("plugin_discover"));

    std::printf("Qt library paths:\n");
    const QStringList paths = QCoreApplication::libraryPaths();
    for (const QString &p : paths) {
        std::printf("  %s\n", qPrintable(p));
    }

    const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("plasmacalendarplugins"));
    std::printf("\nKPluginMetaData::findPlugins(\"plasmacalendarplugins\") -> %d plugin(s)\n", int(plugins.size()));

    bool found = false;
    for (const KPluginMetaData &meta : plugins) {
        std::printf("  id=%-24s name=%-30s file=%s\n",
                    qPrintable(meta.pluginId()),
                    qPrintable(meta.name()),
                    qPrintable(meta.fileName()));
        if (meta.pluginId() == QLatin1String("helloworldcalendar")) {
            found = true;
        }
    }

    if (!found) {
        return fail(2, QStringLiteral("helloworldcalendar was not discovered. "
                                     "Is the plugin installed and is its directory on QT_PLUGIN_PATH?"));
    }

    // Same loading call EventPluginsManager::loadPlugin() makes.
    QPluginLoader loader(QStringLiteral("plasmacalendarplugins/helloworldcalendar"));
    QObject *instance = loader.instance();
    if (!instance) {
        return fail(3, QStringLiteral("QPluginLoader failed: %1").arg(loader.errorString()));
    }

    auto *plugin = qobject_cast<CalendarEvents::CalendarEventsPlugin *>(instance);
    if (!plugin) {
        return fail(4, QStringLiteral("qobject_cast<CalendarEvents::CalendarEventsPlugin *> failed"));
    }

    std::printf("\nPlugin loaded and cast to CalendarEventsPlugin OK.\n");

    bool gotData = false;
    QObject::connect(plugin, &CalendarEvents::CalendarEventsPlugin::dataReady, &app,
                     [&gotData](const QMultiHash<QDate, CalendarEvents::EventData> &data) {
                         gotData = true;
                         std::printf("\ndataReady(): %d entr%s\n", int(data.size()), data.size() == 1 ? "y" : "ies");
                         QDate previous;
                         for (auto it = data.cbegin(); it != data.cend(); ++it) {
                             const CalendarEvents::EventData &e = it.value();
                             if (it.key() != previous) {
                                 std::printf("  %s\n", qPrintable(it.key().toString(Qt::ISODate)));
                                 previous = it.key();
                             }
                             std::printf("    - %-24s start=%s allDay=%s type=%d color=%s uid=%s\n",
                                         qPrintable(e.title()),
                                         qPrintable(e.startDateTime().toString(Qt::ISODate)),
                                         e.isAllDay() ? "yes" : "no",
                                         int(e.type()),
                                         qPrintable(e.eventColor()),
                                         qPrintable(e.uid()));
                         }
                         fflush(stdout);
                         QCoreApplication::quit();
                     });

    const QDate start = QDate::currentDate();
    const QDate end = start.addMonths(1).addDays(-1);

    // Kick off the load only once the event loop is running: dataReady() may be
    // emitted synchronously, and QCoreApplication::quit() before exec() is a
    // no-op.
    QTimer::singleShot(0, plugin, [plugin, start, end] {
        std::printf("\nCalling loadEventsForDateRange(%s, %s)\n",
                    qPrintable(start.toString(Qt::ISODate)),
                    qPrintable(end.toString(Qt::ISODate)));
        plugin->loadEventsForDateRange(start, end);
    });

    QTimer::singleShot(3000, &app, [] {
        std::fprintf(stderr, "FAIL: timed out waiting for dataReady()\n");
        QCoreApplication::exit(5);
    });

    const int rc = app.exec();
    if (rc != 0) {
        return rc;
    }
    if (!gotData) {
        return fail(6, QStringLiteral("dataReady() was emitted but contained no entries"));
    }

    std::printf("\nSUCCESS: plugin discovered, loaded, and returned events.\n");
    return 0;
}
