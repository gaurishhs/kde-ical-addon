/*
    SPDX-FileCopyrightText: 2026 iCal Calendar addon authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>

/*!
 * One configured calendar. The feed URL is deliberately *not* in here:
 * it lives in KWallet and is addressed by \c id.
 */
struct CalendarSource {
    QString id;
    QString name;
    QString color;
    bool enabled = true;
};

/*!
 * One concrete event instance after recurrence expansion.
 *
 * Timed events are carried as UTC instants; all-day events as dates because
 * they are calendar days, not instants (a day is not 24 hours across DST).
 */
struct Occurrence {
    QString uid;
    QString title;
    QString description;
    QString location;
    QString color;

    bool allDay = false;
    QDateTime startUtc;
    QDateTime endUtc;
    QDate startDate;
    QDate endDateExclusive;

    /*! First local calendar day this occurrence covers. */
    QDate firstLocalDate() const;
    /*! Last local calendar day this occurrence covers (inclusive). */
    QDate lastLocalDate() const;
    /*! True if the occurrence covers at least one day inside [from, to]. */
    bool intersects(const QDate &from, const QDate &to) const;
};
