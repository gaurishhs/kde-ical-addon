/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "icalcalendarplugin.h"

#include "feedfetcher.h"
#include "icalparser.h"
#include "secretstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QSet>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace
{
/*! Prefix for every uid handed to Plasma, so we never collide with another
 *  calendar plugin (the API asks plugins to do this). */
const QString kUidPrefix = QStringLiteral("icalcalendar-");
const QString kDemoUidPrefix = QStringLiteral("icalcalendar-demo-");

constexpr int kConfigDebounceMs = 400;
} // namespace

ICalCalendarPlugin::ICalCalendarPlugin(QObject *parent)
    : CalendarEvents::CalendarEventsPlugin(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_secrets(new SecretStore(this))
    , m_refreshTimer(new QTimer(this))
    , m_configDebounce(new QTimer(this))
    , m_configWatcher(new QFileSystemWatcher(this))
{
    // libical is not re-entrant across threads for all of its timezone state,
    // and parsing a month of events is cheap; one worker is plenty.
    m_parsePool.setMaxThreadCount(1);

    connect(m_secrets, &SecretStore::secretsReady, this, &ICalCalendarPlugin::onSecretsReady);
    connect(m_secrets, &SecretStore::availableChanged, this, [](bool available, const QString &reason) {
        if (!available) {
            qWarning("iCal Calendar: feed addresses are unavailable: %s", qPrintable(reason));
        }
    });

    m_refreshTimer->setTimerType(Qt::VeryCoarseTimer);
    connect(m_refreshTimer, &QTimer::timeout, this, &ICalCalendarPlugin::refreshAll);

    m_configDebounce->setSingleShot(true);
    m_configDebounce->setInterval(kConfigDebounceMs);
    connect(m_configDebounce, &QTimer::timeout, this, &ICalCalendarPlugin::reloadSettings);

    // The configuration file is replaced atomically by QSettings, so the file
    // watch can stop after a save; the directory watch keeps us informed and
    // installConfigWatch() re-adds the file.
    connect(m_configWatcher, &QFileSystemWatcher::fileChanged, this, [this] {
        installConfigWatch();
        m_configDebounce->start();
    });
    connect(m_configWatcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        installConfigWatch();
        m_configDebounce->start();
    });

    installConfigWatch();
    reloadSettings();
}

ICalCalendarPlugin::~ICalCalendarPlugin()
{
    // Wait for any in-flight parse before members disappear.
    m_parsePool.waitForDone();
}

void ICalCalendarPlugin::installConfigWatch()
{
    const QString path = Settings::configFilePath();
    const QString directory = QFileInfo(path).absolutePath();
    QDir().mkpath(directory);

    if (!m_configWatcher->directories().contains(directory)) {
        m_configWatcher->addPath(directory);
    }
    if (QFile::exists(path) && !m_configWatcher->files().contains(path)) {
        m_configWatcher->addPath(path);
    }
}

void ICalCalendarPlugin::reloadSettings()
{
    const Settings::Snapshot snapshot = Settings::load();
    const bool changed = !m_settingsLoaded || snapshot != m_settings;
    m_settings = snapshot;
    m_settingsLoaded = true;

    QSet<QString> wanted;
    wanted.reserve(snapshot.calendars.size());
    for (const CalendarSource &source : snapshot.calendars) {
        wanted.insert(source.id);
    }

    // Forget calendars that were removed from the configuration.
    for (auto it = m_calendars.begin(); it != m_calendars.end();) {
        if (!wanted.contains(it.key())) {
            if (it->fetcher) {
                it->fetcher->deleteLater();
            }
            it = m_calendars.erase(it);
        } else {
            ++it;
        }
    }

    for (const CalendarSource &source : snapshot.calendars) {
        CalendarState &state = m_calendars[source.id];
        if (!state.fetcher) {
            FeedFetcher *fetcher = new FeedFetcher(m_network, source.id, this);
            connect(fetcher, &FeedFetcher::cacheLoaded, this, [this, id = source.id](const QByteArray &data) {
                auto it = m_calendars.find(id);
                if (it == m_calendars.end() || data.isEmpty()) {
                    return;
                }
                it->feed = data;
                it->haveFeed = true;
                scheduleParse(id);
            });
            connect(fetcher, &FeedFetcher::fetched, this, [this, id = source.id](const QByteArray &data) {
                onFetched(id, data);
            });
            connect(fetcher, &FeedFetcher::failed, this, [this, id = source.id](const QString &message) {
                onFetchFailed(id, message);
            });
            connect(fetcher, &FeedFetcher::notModified, this, [this, id = source.id] {
                auto it = m_calendars.find(id);
                if (it != m_calendars.end()) {
                    // The cached body is still current. Make sure it is shown
                    // even if the cache was written by an earlier run.
                    if (!it->haveFeed) {
                        if (it->fetcher) {
                            it->fetcher->loadCache();
                        }
                    } else if (m_haveRange && it->occurrences.isEmpty()) {
                        scheduleParse(id);
                    }
                }
            });
            state.fetcher = fetcher;
        }
        state.source = source;
    }

    requestSecrets();
    restartRefreshTimer();

    if (!changed || !m_haveRange) {
        return;
    }

    // A save can enable/disable/rename/recolor calendars. Diff-emit so Plasma
    // drops events of disabled calendars and picks up the new colours.
    emitMap(m_settings.demoMode ? buildDemoMap() : buildMap(), false);
}

void ICalCalendarPlugin::requestSecrets()
{
    QStringList ids;
    ids.reserve(m_calendars.size());
    for (const CalendarSource &source : std::as_const(m_settings.calendars)) {
        ids.append(source.id);
    }
    m_secrets->request(ids);
}

void ICalCalendarPlugin::onSecretsReady(const QHash<QString, QString> &urls)
{
    bool changed = false;

    for (auto it = m_calendars.begin(); it != m_calendars.end(); ++it) {
        const QString url = urls.value(it.key());
        if (url == it->url) {
            continue;
        }
        changed = true;

        it->url = url;
        // The previous body (and its event set) belongs to the old URL.
        it->feed.clear();
        it->haveFeed = false;
        it->occurrences.clear();
        it->parsedFrom = QDate();
        it->parsedTo = QDate();
        it->parseSeq++; // invalidate results of a parse started before the change
        if (it->fetcher) {
            it->fetcher->setUrl(url);
            if (!url.isEmpty()) {
                it->fetcher->loadCache();
            }
        }
    }

    if (!changed) {
        return;
    }

    if (m_haveRange) {
        emitMap(m_settings.demoMode ? buildDemoMap() : buildMap(), false);
    }
    if (!m_settings.demoMode) {
        refreshAll();
    }
}

void ICalCalendarPlugin::restartRefreshTimer()
{
    const int minutes = Settings::clampRefreshMinutes(m_settings.refreshMinutes);
    m_refreshTimer->start(minutes * 60 * 1000);
}

void ICalCalendarPlugin::refreshAll()
{
    if (m_settings.demoMode) {
        return;
    }
    for (CalendarState &state : m_calendars) {
        if (state.source.enabled && state.fetcher && state.fetcher->hasUrl()) {
            state.fetcher->fetch();
        }
    }
}

void ICalCalendarPlugin::onFetched(const QString &id, const QByteArray &data)
{
    auto it = m_calendars.find(id);
    if (it == m_calendars.end() || data.isEmpty()) {
        return;
    }
    it->feed = data;
    it->haveFeed = true;
    it->lastError.clear();
    scheduleParse(id);
}

void ICalCalendarPlugin::onFetchFailed(const QString &id, const QString &message)
{
    auto it = m_calendars.find(id);
    if (it == m_calendars.end()) {
        return;
    }
    // The message never contains the URL (FeedFetcher strips it), so it is
    // safe to log.
    if (it->lastError != message) {
        it->lastError = message;
        qWarning("iCal Calendar: %s", qPrintable(message));
    }
    // Keep showing the cached copy, if any.
}

void ICalCalendarPlugin::scheduleParse(const QString &id)
{
    auto it = m_calendars.find(id);
    if (it == m_calendars.end() || !it->haveFeed || !m_haveRange) {
        return;
    }
    if (it->parseInFlight) {
        it->parsePending = true;
        return;
    }

    const QDate from = m_rangeStart;
    const QDate to = m_rangeEnd;
    const quint64 seq = ++it->parseSeq;
    const QByteArray feed = it->feed;
    it->parseInFlight = true;

    auto *watcher = new QFutureWatcher<IcalParser::Result>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id, seq, from, to] {
        const IcalParser::Result result = watcher->result();
        watcher->deleteLater();

        auto state = m_calendars.find(id);
        if (state == m_calendars.end() || state->parseSeq != seq) {
            return; // superseded by a newer request or a removed calendar
        }
        state->parseInFlight = false;
        state->parsePending = false;

        if (result.error.isEmpty()) {
            state->occurrences = result.occurrences;
            state->parsedFrom = from;
            state->parsedTo = to;
            state->lastError.clear();
        } else {
            state->lastError = result.error;
            qWarning("iCal Calendar: could not parse a feed: %s", qPrintable(result.error));
        }

        if (!m_haveRange) {
            return;
        }
        if (from != m_rangeStart || to != m_rangeEnd) {
            // The requested month changed while we were parsing.
            scheduleParse(id);
            return;
        }

        emitMap(m_settings.demoMode ? buildDemoMap() : buildMap(), false);
    });

    watcher->setFuture(QtConcurrent::run(&m_parsePool, [feed, from, to] {
        return IcalParser::parse(feed, from, to);
    }));
}

CalendarEvents::EventData ICalCalendarPlugin::makeEventData(const Occurrence &occurrence, const CalendarState &state) const
{
    CalendarEvents::EventData data;
    data.setUid(kUidPrefix + state.source.id + QLatin1Char('-') + occurrence.uid);

    const QString title = occurrence.title.trimmed();
    data.setTitle(title.isEmpty() ? QStringLiteral("(No title)") : occurrence.title);
    if (!occurrence.description.isEmpty()) {
        data.setDescription(occurrence.description);
    }
    if (!occurrence.location.isEmpty()) {
        data.setLocation(occurrence.location);
    }

    data.setEventType(CalendarEvents::EventData::Event);
    data.setEventColor(occurrence.color.isEmpty() ? state.source.color : occurrence.color);

    if (occurrence.allDay) {
        data.setStartDateTime(QDateTime(occurrence.startDate, QTime(0, 0)));
        data.setEndDateTime(QDateTime(occurrence.endDateExclusive.addDays(-1), QTime(23, 59, 59)));
        data.setIsAllDay(true);
    } else {
        data.setStartDateTime(occurrence.startUtc.toLocalTime());
        data.setEndDateTime(occurrence.endUtc.toLocalTime());
        data.setIsAllDay(false);
    }

    return data;
}

QMultiHash<QDate, CalendarEvents::EventData> ICalCalendarPlugin::buildMap() const
{
    QMultiHash<QDate, CalendarEvents::EventData> map;
    if (!m_haveRange) {
        return map;
    }

    for (const CalendarState &state : m_calendars) {
        if (!state.source.enabled || state.url.isEmpty() || state.occurrences.isEmpty()) {
            continue;
        }
        for (const Occurrence &occurrence : state.occurrences) {
            if (!occurrence.intersects(m_rangeStart, m_rangeEnd)) {
                continue;
            }

            const CalendarEvents::EventData data = makeEventData(occurrence, state);

            QDate first = occurrence.allDay ? occurrence.startDate : occurrence.firstLocalDate();
            QDate last = occurrence.allDay ? occurrence.endDateExclusive.addDays(-1) : occurrence.lastLocalDate();
            first = qMax(first, m_rangeStart);
            last = qMin(last, m_rangeEnd);

            for (QDate day = first; day.isValid() && day <= last; day = day.addDays(1)) {
                map.insert(day, data);
            }
        }
    }

    return map;
}

QMultiHash<QDate, CalendarEvents::EventData> ICalCalendarPlugin::buildDemoMap() const
{
    QMultiHash<QDate, CalendarEvents::EventData> map;
    if (!m_haveRange) {
        return map;
    }

    const QDate anchor = m_rangeStart;

    auto add = [&map, this](const CalendarEvents::EventData &event) {
        QDate day = event.startDateTime().date();
        if (!day.isValid()) {
            return;
        }
        QDate last = event.isAllDay() && event.endDateTime().isValid() ? event.endDateTime().date() : day;
        if (last < day) {
            last = day;
        }
        day = qMax(day, m_rangeStart);
        last = qMin(last, m_rangeEnd);
        for (QDate d = day; d.isValid() && d <= last; d = d.addDays(1)) {
            map.insert(d, event);
        }
    };

    auto make = [](const QString &id, const QString &title, const QString &description, const QDateTime &start,
                   const QDateTime &end, bool allDay, const QString &color, CalendarEvents::EventData::EventType type) {
        CalendarEvents::EventData event;
        event.setUid(kDemoUidPrefix + id);
        event.setTitle(title);
        event.setDescription(description);
        event.setStartDateTime(start);
        event.setEndDateTime(end);
        event.setIsAllDay(allDay);
        event.setEventType(type);
        event.setEventColor(color);
        return event;
    };

    add(make(QStringLiteral("meeting"),
             QStringLiteral("Team Meeting"),
             QStringLiteral("Weekly team sync"),
             QDateTime(anchor.addDays(2), QTime(9, 0)),
             QDateTime(anchor.addDays(2), QTime(10, 0)),
             false,
             QStringLiteral("#3498db"),
             CalendarEvents::EventData::Event));

    add(make(QStringLiteral("holiday"),
             QStringLiteral("Company Holiday"),
             QStringLiteral("Office is closed"),
             QDateTime(anchor.addDays(5), QTime(0, 0)),
             QDateTime(anchor.addDays(5), QTime(23, 59, 59)),
             true,
             QStringLiteral("#e74c3c"),
             CalendarEvents::EventData::Holiday));

    add(make(QStringLiteral("presentation"),
             QStringLiteral("Project Presentation"),
             QStringLiteral("Demo the new features"),
             QDateTime(anchor.addDays(2), QTime(14, 0)),
             QDateTime(anchor.addDays(2), QTime(15, 0)),
             false,
             QStringLiteral("#2ecc71"),
             CalendarEvents::EventData::Event));

    add(make(QStringLiteral("birthday"),
             QStringLiteral("Birthday Party"),
             QStringLiteral("Cake in the kitchen"),
             QDateTime(anchor.addDays(2), QTime(17, 0)),
             QDateTime(anchor.addDays(2), QTime(18, 0)),
             false,
             QStringLiteral("#9b59b6"),
             CalendarEvents::EventData::Event));

    add(make(QStringLiteral("workout"),
             QStringLiteral("Gym Workout"),
             QStringLiteral("Leg day"),
             QDateTime(anchor.addDays(7), QTime(10, 0)),
             QDateTime(anchor.addDays(7), QTime(11, 0)),
             false,
             QStringLiteral("#f39c12"),
             CalendarEvents::EventData::Event));

    add(make(QStringLiteral("conference"),
             QStringLiteral("Developer Conference"),
             QStringLiteral("Two-day conference"),
             QDateTime(anchor.addDays(10), QTime(0, 0)),
             QDateTime(anchor.addDays(11), QTime(23, 59, 59)),
             true,
             QStringLiteral("#16a085"),
             CalendarEvents::EventData::Event));

    return map;
}

QString ICalCalendarPlugin::signatureFor(const QVector<EmittedItem> &items)
{
    QStringList parts;
    parts.reserve(items.size());
    for (const EmittedItem &item : items) {
        const CalendarEvents::EventData &data = item.data;
        parts.append(QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
                         .arg(item.date.toString(Qt::ISODate),
                              data.uid(),
                              data.title(),
                              data.description(),
                              data.startDateTime().toString(Qt::ISODate),
                              data.endDateTime().toString(Qt::ISODate),
                              data.eventColor(),
                              data.isAllDay() ? QStringLiteral("1") : QStringLiteral("0"),
                              QString::number(int(data.type()))));
    }
    // Day order depends on hash iteration; sort so equal sets compare equal.
    parts.sort();
    return parts.join(QLatin1Char('\n'));
}

void ICalCalendarPlugin::emitMap(const QMultiHash<QDate, CalendarEvents::EventData> &map, bool fullReplace)
{
    QHash<QString, QVector<EmittedItem>> grouped;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        grouped[it.value().uid()].append(EmittedItem{it.key(), it.value()});
    }

    QHash<QString, QString> signatures;
    signatures.reserve(grouped.size());
    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
        signatures.insert(it.key(), signatureFor(it.value()));
    }

    if (fullReplace) {
        m_emitted = grouped;
        m_emittedSignatures = signatures;
        Q_EMIT dataReady(map);
        return;
    }

    // DaysModel::onDataReady() appends and onEventRemoved() clears everything
    // for a uid, so removals and changes must go through eventRemoved() before
    // the replacement entries are handed over.
    QStringList removed;
    for (auto it = m_emitted.cbegin(); it != m_emitted.cend(); ++it) {
        if (!grouped.contains(it.key())) {
            removed.append(it.key());
        }
    }

    QStringList changed;
    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) {
        const auto old = m_emittedSignatures.constFind(it.key());
        if (old == m_emittedSignatures.constEnd() || old.value() != signatures.value(it.key())) {
            changed.append(it.key());
        }
    }

    QMultiHash<QDate, CalendarEvents::EventData> toAdd;
    for (const QString &uid : std::as_const(removed)) {
        Q_EMIT eventRemoved(uid);
    }
    for (const QString &uid : std::as_const(changed)) {
        if (m_emitted.contains(uid)) {
            Q_EMIT eventRemoved(uid);
        }
        const QVector<EmittedItem> items = grouped.value(uid);
        for (const EmittedItem &item : items) {
            toAdd.insert(item.date, item.data);
        }
    }

    m_emitted = grouped;
    m_emittedSignatures = signatures;

    if (!toAdd.isEmpty()) {
        Q_EMIT dataReady(toAdd);
    }
}

void ICalCalendarPlugin::loadEventsForDateRange(const QDate &startDate, const QDate &endDate)
{
    m_rangeStart = startDate;
    m_rangeEnd = endDate;
    m_haveRange = true;

    if (m_settings.demoMode) {
        emitMap(buildDemoMap(), true);
        return;
    }

    // Answer immediately with whatever is already cached so the popup is never
    // blank while the network (or a parse) is still running.
    emitMap(buildMap(), true);

    const int refreshMinutes = Settings::clampRefreshMinutes(m_settings.refreshMinutes);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    bool needFetch = false;

    for (CalendarState &state : m_calendars) {
        if (!state.source.enabled || !state.fetcher || !state.fetcher->hasUrl()) {
            continue;
        }
        if (state.haveFeed && (state.parsedFrom > startDate || state.parsedTo < endDate)) {
            scheduleParse(state.source.id);
        }
        const QDateTime lastFetched = state.fetcher->lastFetched();
        if (!lastFetched.isValid() || lastFetched.secsTo(now) > qint64(refreshMinutes) * 60) {
            needFetch = true;
        }
    }

    if (needFetch) {
        refreshAll();
    }
}
