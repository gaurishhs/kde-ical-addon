/*
    SPDX-FileCopyrightText: 2026 Hello World Calendar Plugin authors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "helloworldcalendar.h"

#include <QDateTime>
#include <QTime>

HelloWorldCalendarPlugin::HelloWorldCalendarPlugin(QObject *parent)
    : CalendarEvents::CalendarEventsPlugin(parent)
{
}

HelloWorldCalendarPlugin::~HelloWorldCalendarPlugin() = default;

void HelloWorldCalendarPlugin::loadEventsForDateRange(const QDate &startDate, const QDate &endDate)
{
    // Serve the cache when Plasma asks for the same range again (it does so on
    // every month change and on some internal refreshes).
    if (!m_lastData.isEmpty() && m_lastStartDate == startDate && m_lastEndDate == endDate) {
        Q_EMIT dataReady(m_lastData);
        return;
    }

    m_lastStartDate = startDate;
    m_lastEndDate = endDate;
    m_lastData.clear();

    // This is a *test* plugin: instead of fixed absolute dates (which would only
    // show up in one particular month) the demo events are anchored to the start
    // of the requested range. That way something is always visible in the
    // Digital Clock calendar no matter which month the user navigates to.
    const QDate anchor = startDate;

    auto addEvent = [this, &startDate, &endDate](const CalendarEvents::EventData &event) {
        // Key every event on the day(s) it actually occurs on. DaysModel only
        // picks up entries whose key is a visible day, so keying on the range
        // start would hide events in the middle of the month.
        QDate day = event.startDateTime().date();
        if (!day.isValid()) {
            return;
        }
        // For all-day events that span several days, register the event on each
        // covered day (same approach as the upstream holidays plugin).
        QDate last = event.isAllDay() && event.endDateTime().isValid() ? event.endDateTime().date() : day;
        if (last < day) {
            last = day;
        }
        for (QDate d = day; d <= last; d = d.addDays(1)) {
            if (d >= startDate && d <= endDate) {
                m_lastData.insert(d, event);
            }
        }
    };

    // 1) Timed event: team meeting, two days into the range, 09:00-10:00.
    CalendarEvents::EventData meeting;
    meeting.setTitle(QStringLiteral("Team Meeting"));
    meeting.setDescription(QStringLiteral("Weekly team sync"));
    meeting.setStartDateTime(QDateTime(anchor.addDays(2), QTime(9, 0)));
    meeting.setEndDateTime(QDateTime(anchor.addDays(2), QTime(10, 0)));
    meeting.setEventType(CalendarEvents::EventData::Event);
    meeting.setIsAllDay(false);
    meeting.setEventColor(QStringLiteral("#3498db"));
    meeting.setUid(QStringLiteral("helloworldcalendar-meeting"));
    addEvent(meeting);

    // 2) All-day event: company holiday, five days into the range.
    CalendarEvents::EventData holiday;
    holiday.setTitle(QStringLiteral("Company Holiday"));
    holiday.setDescription(QStringLiteral("Office is closed"));
    holiday.setStartDateTime(QDateTime(anchor.addDays(5), QTime(0, 0)));
    holiday.setEndDateTime(QDateTime(anchor.addDays(5), QTime(23, 59, 59)));
    holiday.setEventType(CalendarEvents::EventData::Holiday);
    holiday.setIsAllDay(true);
    holiday.setEventColor(QStringLiteral("#e74c3c"));
    holiday.setUid(QStringLiteral("helloworldcalendar-holiday"));
    addEvent(holiday);

    // 3) Same day as the meeting: project presentation, 14:00-15:00.
    CalendarEvents::EventData presentation;
    presentation.setTitle(QStringLiteral("Project Presentation"));
    presentation.setDescription(QStringLiteral("Demo the new features"));
    presentation.setStartDateTime(QDateTime(anchor.addDays(2), QTime(14, 0)));
    presentation.setEndDateTime(QDateTime(anchor.addDays(2), QTime(15, 0)));
    presentation.setEventType(CalendarEvents::EventData::Event);
    presentation.setIsAllDay(false);
    presentation.setEventColor(QStringLiteral("#2ecc71"));
    presentation.setUid(QStringLiteral("helloworldcalendar-presentation"));
    addEvent(presentation);

    // 4) Same day as well: birthday party, 17:00-18:00.
    CalendarEvents::EventData birthday;
    birthday.setTitle(QStringLiteral("Birthday Party"));
    birthday.setDescription(QStringLiteral("Cake in the kitchen"));
    birthday.setStartDateTime(QDateTime(anchor.addDays(2), QTime(17, 0)));
    birthday.setEndDateTime(QDateTime(anchor.addDays(2), QTime(18, 0)));
    birthday.setEventType(CalendarEvents::EventData::Event);
    birthday.setIsAllDay(false);
    birthday.setEventColor(QStringLiteral("#9b59b6"));
    birthday.setUid(QStringLiteral("helloworldcalendar-birthday"));
    addEvent(birthday);

    // 5) Later in the month: gym workout, 10:00-11:00.
    CalendarEvents::EventData workout;
    workout.setTitle(QStringLiteral("Gym Workout"));
    workout.setDescription(QStringLiteral("Leg day"));
    workout.setStartDateTime(QDateTime(anchor.addDays(7), QTime(10, 0)));
    workout.setEndDateTime(QDateTime(anchor.addDays(7), QTime(11, 0)));
    workout.setEventType(CalendarEvents::EventData::Event);
    workout.setIsAllDay(false);
    workout.setEventColor(QStringLiteral("#f39c12"));
    workout.setUid(QStringLiteral("helloworldcalendar-workout"));
    addEvent(workout);

    // 6) Multi-day all-day event to exercise the "spans several days" path.
    CalendarEvents::EventData conference;
    conference.setTitle(QStringLiteral("Developer Conference"));
    conference.setDescription(QStringLiteral("Two-day conference"));
    conference.setStartDateTime(QDateTime(anchor.addDays(10), QTime(0, 0)));
    conference.setEndDateTime(QDateTime(anchor.addDays(11), QTime(23, 59, 59)));
    conference.setEventType(CalendarEvents::EventData::Event);
    conference.setIsAllDay(true);
    conference.setEventColor(QStringLiteral("#16a085"));
    conference.setUid(QStringLiteral("helloworldcalendar-conference"));
    addEvent(conference);

    Q_EMIT dataReady(m_lastData);
}
