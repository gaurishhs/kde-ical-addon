/*
    SPDX-FileCopyrightText: 2026 Hello World Calendar Plugin authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <CalendarEvents/CalendarEventsPlugin>

#include <QDate>
#include <QMultiHash>
#include <QObject>

class HelloWorldCalendarPlugin : public CalendarEvents::CalendarEventsPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.CalendarEventsPlugin" FILE "helloworldcalendar.json")
    Q_INTERFACES(CalendarEvents::CalendarEventsPlugin)

public:
    explicit HelloWorldCalendarPlugin(QObject *parent = nullptr);
    ~HelloWorldCalendarPlugin() override;

    void loadEventsForDateRange(const QDate &startDate, const QDate &endDate) override;

private:
    QDate m_lastStartDate;
    QDate m_lastEndDate;
    QMultiHash<QDate, CalendarEvents::EventData> m_lastData;
};
