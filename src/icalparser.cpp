/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "icalparser.h"

#include <QHash>
#include <QSet>
#include <QTime>
#include <QTimeZone>

#include <libical/ical.h>

#include <cstring>

namespace
{

/*!
 * Hard limits so that a malformed or hostile feed cannot lock up the process
 * (for example FREQ=SECONDLY with no UNTIL or COUNT).
 */
constexpr int kMaxInstancesPerEvent = 5000;
constexpr int kMaxOccurrencesTotal = 20000;

/*! Wall-clock identity of a DATE or DATE-TIME, used to match EXDATE and
 *  RECURRENCE-ID against expanded instances. Two values with the same key mean
 *  "the same slot in the event's own timezone", independent of UTC offsets. */
QString naiveKey(const icaltimetype &t)
{
    if (icaltime_is_date(t)) {
        return QString::asprintf("%04d%02d%02d", t.year, t.month, t.day);
    }
    return QString::asprintf("%04d%02d%02dT%02d%02d%02d", t.year, t.month, t.day, t.hour, t.minute, t.second);
}

/*!
 * libical resolves TZID into \c value.zone only for DTSTART/DTEND; for EXDATE
 * and RECURRENCE-ID it leaves the zone null and only records the parameter.
 * Resolving it here keeps those values convertible to an absolute instant.
 */
void resolveValueZone(icalproperty *prop, icaltimetype *value)
{
    if (value->zone || icaltime_is_utc(*value) || icaltime_is_date(*value)) {
        return;
    }
    const char *tzid = icalproperty_get_parameter_as_string(prop, "TZID");
    if (!tzid || !*tzid) {
        return;
    }
    if (icaltimezone *zone = icaltimezone_get_builtin_timezone(tzid)) {
        value->zone = zone;
    }
}

/*!
 * The timezone libical attached to a parsed value, or one resolved from the
 * DTSTART;TZID= parameter. Returns nullptr for floating times.
 *
 * libical normally resolves TZID while parsing (both for IANA names and for
 * inline VTIMEZONE definitions), but the fallback keeps feeds working when it
 * does not.
 */
icaltimezone *resolveZone(icalcomponent *comp, const icaltimetype &value)
{
    if (value.zone) {
        return const_cast<icaltimezone *>(value.zone);
    }
    icalproperty *prop = icalcomponent_get_first_property(comp, ICAL_DTSTART_PROPERTY);
    if (!prop) {
        return nullptr;
    }
    const char *tzid = icalproperty_get_parameter_as_string(prop, "TZID");
    if (!tzid || !*tzid) {
        return nullptr;
    }
    if (icaltimezone *zone = icaltimezone_get_builtin_timezone(tzid)) {
        return zone;
    }
    return icaltimezone_get_builtin_timezone_from_tzid(tzid);
}

/*!
 * Converts a parsed value to an absolute instant.
 *
 *  - DATE-TIME with Z      -> that UTC instant
 *  - DATE-TIME with TZID   -> converted to UTC (DST-aware)
 *  - floating DATE-TIME    -> interpreted in the user's local zone
 *
 * Note: icaltime_as_timet()/icaltime_as_timet_with_zone() are deliberately not
 * used here; icaltime_as_timet() silently ignores the timezone, which shifts
 * every zoned event by its UTC offset.
 */
QDateTime instantOf(const icaltimetype &value, icaltimezone *zone)
{
    const QDate date(value.year, value.month, value.day);
    const QTime time(value.hour, value.minute, value.second);

    if (icaltime_is_utc(value)) {
        return QDateTime(date, time, QTimeZone::UTC);
    }
    if (zone) {
        icaltimetype local = value;
        local.zone = zone;
        const icaltimetype utc = icaltime_convert_to_zone(local, icaltimezone_get_utc_timezone());
        return QDateTime(QDate(utc.year, utc.month, utc.day), QTime(utc.hour, utc.minute, utc.second), QTimeZone::UTC);
    }
    return QDateTime(date, time, QTimeZone::LocalTime);
}

/*! Converts a value into the master event's wall clock. */
QString slotKey(const icaltimetype &value, icaltimezone *masterZone)
{
    if (masterZone && !icaltime_is_date(value) && (value.zone || icaltime_is_utc(value))) {
        icaltimetype converted = icaltime_convert_to_zone(value, masterZone);
        return naiveKey(converted);
    }
    return naiveKey(value);
}

struct RawEvent {
    icalcomponent *comp = nullptr;
    QString uid;
    QString title;
    QString description;
    QString location;
    bool cancelled = false;
    bool allDay = false;
    icaltimetype dtstart{};
    icaltimezone *zone = nullptr;
    int durationSeconds = 0;
    int durationDays = 1;
    bool hasRrule = false;
    icalrecurrencetype rrule{};
    QSet<QString> exdates;
    /*!
     * Kept raw so that it can be normalised into the master event's timezone
     * once the master is known (RECURRENCE-ID and DTSTART may use different
     * zones when an occurrence was moved).
     */
    icaltimetype recurrenceId{};
    bool hasRecurrenceId = false;
};

bool extractEvent(icalcomponent *comp, RawEvent *out)
{
    out->comp = comp;

    icaltimetype dtstart = icalcomponent_get_dtstart(comp);
    if (icaltime_is_null_time(dtstart)) {
        return false;
    }
    out->dtstart = dtstart;
    out->allDay = icaltime_is_date(dtstart) != 0;
    out->zone = resolveZone(comp, dtstart);

    const char *uid = icalcomponent_get_uid(comp);
    if (uid && *uid) {
        out->uid = QString::fromUtf8(uid);
    } else {
        // RFC 5545 requires UID. Synthesise a stable one rather than dropping
        // the event: identity only needs to be stable within this feed.
        out->uid = QStringLiteral("noid-%1").arg(qHash(naiveKey(dtstart) + QStringLiteral("|") + QString::fromUtf8(
            icalcomponent_get_summary(comp) ? icalcomponent_get_summary(comp) : "")));
    }
    if (const char *summary = icalcomponent_get_summary(comp)) {
        out->title = QString::fromUtf8(summary);
    }
    if (const char *description = icalcomponent_get_description(comp)) {
        out->description = QString::fromUtf8(description);
    }
    if (const char *location = icalcomponent_get_location(comp)) {
        out->location = QString::fromUtf8(location);
    }
    out->cancelled = icalcomponent_get_status(comp) == ICAL_STATUS_CANCELLED;

    icaltimetype dtend = icalcomponent_get_dtend(comp);
    if (icaltime_is_null_time(dtend)) {
        dtend = dtstart;
    }
    if (out->allDay) {
        const QDate start(dtstart.year, dtstart.month, dtstart.day);
        const QDate end(dtend.year, dtend.month, dtend.day);
        // DTEND is exclusive for all-day events, so a single day yields 1.
        out->durationDays = qMax(1, int(start.daysTo(end)));
        if (out->durationDays < 1) {
            out->durationDays = 1;
        }
    } else {
        const QDateTime start = instantOf(dtstart, out->zone);
        const QDateTime end = instantOf(dtend, out->zone);
        const qint64 seconds = start.secsTo(end);
        out->durationSeconds = seconds > 0 ? int(qMin<qint64>(seconds, 7 * 24 * 60 * 60)) : 0;
    }

    if (icalproperty *rr = icalcomponent_get_first_property(comp, ICAL_RRULE_PROPERTY)) {
        out->hasRrule = true;
        out->rrule = icalproperty_get_rrule(rr);
    }

    for (icalproperty *p = icalcomponent_get_first_property(comp, ICAL_EXDATE_PROPERTY); p;
         p = icalcomponent_get_next_property(comp, ICAL_EXDATE_PROPERTY)) {
        icaltimetype value = icalproperty_get_exdate(p);
        resolveValueZone(p, &value);
        out->exdates.insert(slotKey(value, out->zone));
    }

    if (icalproperty *rid = icalcomponent_get_first_property(comp, ICAL_RECURRENCEID_PROPERTY)) {
        out->hasRecurrenceId = true;
        out->recurrenceId = icalproperty_get_recurrenceid(rid);
        resolveValueZone(rid, &out->recurrenceId);
    }

    return true;
}

Occurrence makeOccurrence(const RawEvent &event, const QString &color)
{
    Occurrence occ;
    occ.uid = event.uid;
    occ.title = event.title;
    occ.description = event.description;
    occ.location = event.location;
    occ.color = color;
    occ.allDay = event.allDay;

    if (event.allDay) {
        const QDate start(event.dtstart.year, event.dtstart.month, event.dtstart.day);
        occ.startDate = start;
        occ.endDateExclusive = start.addDays(event.durationDays);
        occ.startUtc = QDateTime(start, QTime(0, 0), QTimeZone::LocalTime);
        // The last covered day, end-of-day, so that anything reading endUtc
        // sees the real end rather than the exclusive boundary.
        occ.endUtc = QDateTime(occ.endDateExclusive.addDays(-1), QTime(23, 59, 59), QTimeZone::LocalTime);
    } else {
        occ.startUtc = instantOf(event.dtstart, event.zone);
        occ.endUtc = occ.startUtc.addSecs(event.durationSeconds);
        occ.startDate = occ.startUtc.toLocalTime().date();
        occ.endDateExclusive = occ.endUtc.toLocalTime().date().addDays(1);
    }

    // Recurring instances need distinct uids so that Plasma can tell them
    // apart (and so eventRemoved() removes exactly one instance). The slot
    // time identifies the instance; overrides reuse the original slot so a
    // moved occurrence replaces the instance it was moved from.
    if (event.hasRrule || event.hasRecurrenceId) {
        occ.uid += QLatin1Char('#')
            + QString::asprintf("%04d%02d%02dT%02d%02d", event.dtstart.year, event.dtstart.month, event.dtstart.day,
                                event.dtstart.hour, event.dtstart.minute);
    }
    return occ;
}

/*! Variant for an expanded instance: the slot time comes from the iterator. */
Occurrence makeInstance(const RawEvent &event, const icaltimetype &slot, const QString &color)
{
    RawEvent instance = event;
    instance.dtstart = slot;
    instance.hasRrule = true; // force the per-instance uid suffix
    return makeOccurrence(instance, color);
}

} // namespace

QString IcalParser::decode(const QByteArray &feed)
{
    QByteArray data = feed;
    if (data.startsWith("\xEF\xBB\xBF")) {
        data.remove(0, 3);
    }
    QString text = QString::fromUtf8(data);
    if (text.contains(QChar::ReplacementCharacter)) {
        // Not valid UTF-8: some old exporters emit Latin-1.
        text = QString::fromLatin1(data);
    }
    return text;
}

IcalParser::Result IcalParser::parse(const QByteArray &feed, const QDate &rangeStart, const QDate &rangeEnd)
{
    Result result;
    if (feed.trimmed().isEmpty()) {
        result.error = QStringLiteral("The calendar feed is empty.");
        return result;
    }

    const QString text = decode(feed);
    QByteArray utf8 = text.toUtf8();
    // libical wants CRLF line endings; some servers emit bare LF.
    if (!utf8.contains("\r\n")) {
        utf8.replace("\n", "\r\n");
    }

    icalcomponent *root = icalparser_parse_string(utf8.constData());
    if (!root) {
        result.error = QStringLiteral("This does not look like an iCalendar feed (parse error: %1).")
                           .arg(QString::fromUtf8(icalerror_strerror(icalerrno)));
        return result;
    }

    // Feeds should be a VCALENDAR, but tolerate a bare VEVENT.
    QVector<icalcomponent *> events;
    const icalcomponent_kind rootKind = icalcomponent_isa(root);
    if (rootKind == ICAL_VEVENT_COMPONENT) {
        events.append(root);
    } else {
        for (icalcomponent *c = icalcomponent_get_first_component(root, ICAL_VEVENT_COMPONENT); c;
             c = icalcomponent_get_next_component(root, ICAL_VEVENT_COMPONENT)) {
            events.append(c);
        }
    }
    result.eventCount = events.size();

    QHash<QString, RawEvent> masters;
    QHash<QString, QVector<RawEvent>> overrides;

    for (icalcomponent *comp : std::as_const(events)) {
        RawEvent raw;
        if (!extractEvent(comp, &raw)) {
            continue;
        }
        if (raw.hasRecurrenceId) {
            overrides[raw.uid].append(raw);
        } else {
            // A cancelled master removes the whole series.
            masters.insert(raw.uid, raw);
        }
    }

    const QString color = QString(); // filled in by the caller

    for (auto it = masters.cbegin(); it != masters.cend(); ++it) {
        const RawEvent &master = it.value();
        if (master.cancelled) {
            continue;
        }

        const QVector<RawEvent> seriesOverrides = overrides.value(master.uid);
        QHash<QString, const RawEvent *> overrideBySlot;
        for (const RawEvent &ov : seriesOverrides) {
            overrideBySlot.insert(slotKey(ov.recurrenceId, master.zone), &ov);
        }
        QSet<QString> usedOverrides;

        if (!master.hasRrule) {
            Occurrence occ = makeOccurrence(master, color);
            if (occ.intersects(rangeStart, rangeEnd)) {
                result.occurrences.append(occ);
            }
            continue;
        }

        icalrecur_iterator *iter = icalrecur_iterator_new(master.rrule, master.dtstart);
        if (!iter) {
            // Unparseable RRULE: show at least the first instance.
            Occurrence occ = makeOccurrence(master, color);
            if (occ.intersects(rangeStart, rangeEnd)) {
                result.occurrences.append(occ);
            }
            continue;
        }

        // Skip the (possibly decades long) prefix of an unbounded series.
        // icalrecur_iterator_set_start() cannot be combined with COUNT, and
        // that is fine: a COUNT rule is bounded anyway.
        if (master.rrule.count == 0) {
            const QDate from = rangeStart.addDays(-qMax(2, master.durationDays + 1));
            icaltimetype jump = master.dtstart;
            jump.year = from.year();
            jump.month = from.month();
            jump.day = from.day();
            if (!master.allDay) {
                jump.hour = 0;
                jump.minute = 0;
                jump.second = 0;
            }
            if (icaltime_compare(jump, master.dtstart) > 0) {
                icalrecur_iterator_set_start(iter, jump);
            }
        }

        for (int i = 0; i < kMaxInstancesPerEvent; ++i) {
            const icaltimetype slot = icalrecur_iterator_next(iter);
            if (icaltime_is_null_time(slot)) {
                break;
            }

            const QString slotKeyValue = naiveKey(slot);
            const RawEvent *override = overrideBySlot.value(slotKeyValue, nullptr);
            if (override) {
                usedOverrides.insert(slotKeyValue);
            } else if (master.exdates.contains(slotKeyValue)) {
                continue;
            }

            Occurrence occ;
            if (override) {
                if (override->cancelled) {
                    continue; // deleted single occurrence
                }
                occ = makeOccurrence(*override, color);
                // Overrides normally repeat SUMMARY; if not, keep the series title.
                if (occ.title.isEmpty()) {
                    occ.title = master.title;
                }
                occ.uid = master.uid + QLatin1Char('#')
                    + QString::asprintf("%04d%02d%02dT%02d%02d", slot.year, slot.month, slot.day, slot.hour, slot.minute);
            } else {
                occ = makeInstance(master, slot, color);
            }

            // Instances are generated in ascending order, so once one starts
            // after the range we are done.
            if (occ.firstLocalDate() > rangeEnd) {
                break;
            }
            if (occ.intersects(rangeStart, rangeEnd)) {
                result.occurrences.append(occ);
                if (result.occurrences.size() >= kMaxOccurrencesTotal) {
                    break;
                }
            }
        }
        icalrecur_iterator_free(iter);

        // Overrides that were never reached by the expansion loop (for example
        // because they were moved into the range from an early slot).
        for (const RawEvent &ov : seriesOverrides) {
            if (usedOverrides.contains(slotKey(ov.recurrenceId, master.zone)) || ov.cancelled) {
                continue;
            }
            Occurrence occ = makeOccurrence(ov, color);
            if (occ.title.isEmpty()) {
                occ.title = master.title;
            }
            if (occ.intersects(rangeStart, rangeEnd) && result.occurrences.size() < kMaxOccurrencesTotal) {
                result.occurrences.append(occ);
            }
        }
        if (result.occurrences.size() >= kMaxOccurrencesTotal) {
            break;
        }
    }

    // Overrides whose master is missing (broken feed): still show them.
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        if (masters.contains(it.key())) {
            continue;
        }
        for (const RawEvent &ov : it.value()) {
            if (ov.cancelled) {
                continue;
            }
            Occurrence occ = makeOccurrence(ov, color);
            if (occ.intersects(rangeStart, rangeEnd)) {
                result.occurrences.append(occ);
            }
        }
    }

    icalcomponent_free(root);
    return result;
}
