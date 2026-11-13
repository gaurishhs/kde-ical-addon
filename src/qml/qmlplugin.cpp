/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "calendarlistmodel.h"
#include "calendarsettings.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QUrl>

class ICalCalendarQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterType<CalendarSettings>(uri, 1, 0, "CalendarSettings");
        qmlRegisterUncreatableType<CalendarListModel>(uri, 1, 0, "CalendarListModel",
                                                      QStringLiteral("Provided by CalendarSettings.calendars"));
    }
};

#include "qmlplugin.moc"
