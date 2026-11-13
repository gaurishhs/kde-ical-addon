/*
    SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "types.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QVector>

/*!
 * Turns an iCalendar (RFC 5545) feed into concrete occurrences for a date range.
 *
 * All the libical handling lives here so that the rest of the plugin never has
 * to touch C structures. parse() is pure and thread-safe: it takes the raw feed
 * bytes and returns plain Qt value types, so it can run on a worker thread.
 */
class IcalParser
{
public:
    struct Result {
        QVector<Occurrence> occurrences;
        /*! Non-empty only when the feed could not be understood at all. */
        QString error;
        /*! Number of VEVENT components seen (diagnostics). */
        int eventCount = 0;
    };

    /*!
     * \a rangeStart and \a rangeEnd are inclusive local dates.
     */
    static Result parse(const QByteArray &feed, const QDate &rangeStart, const QDate &rangeEnd);

    /*!
     * iCalendar is UTF-8 by specification, but broken exports exist; this falls
     * back to Latin-1 when the bytes are not valid UTF-8.
     */
    static QString decode(const QByteArray &feed);
};
