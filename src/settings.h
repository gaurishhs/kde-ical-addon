/*
    SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "types.h"

#include <QDateTime>
#include <QString>
#include <QVector>

/*!
 * Persisted configuration, stored as an INI file in the user's config dir.
 *
 * Private iCal URLs are *not* stored here; each calendar only records an id
 * which is used as the KWallet entry key. See SecretStore.
 */
class Settings
{
public:
    struct Snapshot {
        QVector<CalendarSource> calendars;
        int refreshMinutes = 15;
        bool demoMode = false;
        /*! Bumped on every save so the plugin can cheaply detect changes. */
        qint64 revision = 0;

        bool operator==(const Snapshot &other) const;
        bool operator!=(const Snapshot &other) const
        {
            return !(*this == other);
        }
        int enabledCount() const;
    };

    static QString configFilePath();
    static QString cacheDir();
    static QString newId();

    static Snapshot load();
    static void save(const Snapshot &snapshot);

    /*! Clamps the configurable refresh interval to something sane. */
    static int clampRefreshMinutes(int minutes);
};
