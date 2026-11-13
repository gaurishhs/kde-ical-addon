/*
    SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "types.h"

QDate Occurrence::firstLocalDate() const
{
    if (allDay) {
        return startDate;
    }
    return startUtc.toLocalTime().date();
}

QDate Occurrence::lastLocalDate() const
{
    if (allDay) {
        return endDateExclusive.addDays(-1);
    }
    // A timed event ending exactly at midnight does not cover the next day, so
    // subtract a second before taking the date.
    const QDateTime end = endUtc.toLocalTime();
    const QDate last = (end.time() == QTime(0, 0, 0)) ? end.date().addDays(-1) : end.date();
    return last < firstLocalDate() ? firstLocalDate() : last;
}

bool Occurrence::intersects(const QDate &from, const QDate &to) const
{
    if (!firstLocalDate().isValid()) {
        return false;
    }
    return !(lastLocalDate() < from || firstLocalDate() > to);
}
