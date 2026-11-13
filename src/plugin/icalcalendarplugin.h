/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <CalendarEvents/CalendarEventsPlugin>

#include <QDate>
#include <QHash>
#include <QMultiHash>
#include <QObject>
#include <QThreadPool>
#include <QVector>

#include "settings.h"
#include "types.h"

class FeedFetcher;
class SecretStore;
class QFileSystemWatcher;
class QNetworkAccessManager;
class QTimer;

/*!
 * Plasma calendar plugin backed by private or public iCal feeds.
 *
 * The plugin never talks to a provider API and never needs Akonadi.
 * Each configured calendar is downloaded as a plain .ics document, parsed with
 * libical on a worker thread, and turned into Plasma's EventData.
 *
 * Threading contract:
 *  - everything in this class runs on the GUI thread;
 *  - network I/O is asynchronous (QNetworkAccessManager);
 *  - libical parsing runs on a dedicated single-thread pool (libical keeps
 *    process-wide state, so one parser thread is both safe and enough).
 */
class ICalCalendarPlugin : public CalendarEvents::CalendarEventsPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.CalendarEventsPlugin" FILE "icalcalendar.json")
    Q_INTERFACES(CalendarEvents::CalendarEventsPlugin)

public:
    explicit ICalCalendarPlugin(QObject *parent = nullptr);
    ~ICalCalendarPlugin() override;

    void loadEventsForDateRange(const QDate &startDate, const QDate &endDate) override;

private:
    struct EmittedItem {
        QDate date;
        CalendarEvents::EventData data;
    };

    struct CalendarState {
        CalendarSource source;
        QString url;
        QByteArray feed;
        bool haveFeed = false;
        QVector<Occurrence> occurrences;
        QDate parsedFrom;
        QDate parsedTo;
        bool parseInFlight = false;
        bool parsePending = false;
        quint64 parseSeq = 0;
        FeedFetcher *fetcher = nullptr;
        QString lastError;
    };

    void reloadSettings();
    void installConfigWatch();
    void requestSecrets();
    void onSecretsReady(const QHash<QString, QString> &urls);
    void restartRefreshTimer();
    void refreshAll();
    void scheduleParse(const QString &id);
    void onFetched(const QString &id, const QByteArray &data);
    void onFetchFailed(const QString &id, const QString &message);

    QMultiHash<QDate, CalendarEvents::EventData> buildMap() const;
    QMultiHash<QDate, CalendarEvents::EventData> buildDemoMap() const;
    CalendarEvents::EventData makeEventData(const Occurrence &occurrence, const CalendarState &state) const;

    void emitMap(const QMultiHash<QDate, CalendarEvents::EventData> &map, bool fullReplace);

    static QString signatureFor(const QVector<EmittedItem> &items);

    Settings::Snapshot m_settings;
    bool m_settingsLoaded = false;

    QHash<QString, CalendarState> m_calendars;

    QNetworkAccessManager *m_network = nullptr;
    SecretStore *m_secrets = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QTimer *m_configDebounce = nullptr;
    QFileSystemWatcher *m_configWatcher = nullptr;
    QThreadPool m_parsePool;

    QDate m_rangeStart;
    QDate m_rangeEnd;
    bool m_haveRange = false;

    /*! What Plasma currently shows, keyed by the uid we handed out. */
    QHash<QString, QVector<EmittedItem>> m_emitted;
    QHash<QString, QString> m_emittedSignatures;
};
