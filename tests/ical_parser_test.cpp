/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later

    Parser regression tests. Deliberately dependency-free (no QtTest) so the
    suite also builds with a minimal Qt installation.
*/

#include "icalparser.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QTimeZone>

#include <cstdio>

namespace
{
int g_failures = 0;
int g_checks = 0;

void check(bool condition, const QString &what)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::fprintf(stderr, "FAIL: %s\n", qPrintable(what));
    }
}

void checkEqual(const QDateTime &actual, const QDateTime &expected, const QString &what)
{
    check(actual.toUTC() == expected.toUTC(),
          QStringLiteral("%1 (got %2, expected %3)")
              .arg(what, actual.toUTC().toString(Qt::ISODate), expected.toUTC().toString(Qt::ISODate)));
}

QByteArray ics(const char *body)
{
    return QByteArray("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//tests//EN\r\n") + QByteArray(body)
        + QByteArray("END:VCALENDAR\r\n");
}

const Occurrence *findByUid(const QVector<Occurrence> &list, const QString &prefix)
{
    for (const Occurrence &occ : list) {
        if (occ.uid.startsWith(prefix)) {
            return &occ;
        }
    }
    return nullptr;
}

int countByUid(const QVector<Occurrence> &list, const QString &prefix)
{
    int n = 0;
    for (const Occurrence &occ : list) {
        if (occ.uid.startsWith(prefix)) {
            ++n;
        }
    }
    return n;
}

QDateTime utc(int y, int m, int d, int h, int min)
{
    return QDateTime(QDate(y, m, d), QTime(h, min), QTimeZone::UTC);
}

// ---------------------------------------------------------------------------

void testTimezoneConversion()
{
    const QByteArray feed = ics(R"(BEGIN:VTIMEZONE
TZID:Custom +05:30
BEGIN:STANDARD
DTSTART:19700101T000000
TZOFFSETFROM:+0530
TZOFFSETTO:+0530
TZNAME:CUSTOM
END:STANDARD
END:VTIMEZONE
BEGIN:VEVENT
UID:berlin-winter
SUMMARY:Berlin winter
DTSTART;TZID=Europe/Berlin:20261102T090000
DTEND;TZID=Europe/Berlin:20261102T100000
END:VEVENT
BEGIN:VEVENT
UID:berlin-summer
SUMMARY:Berlin summer
DTSTART;TZID=Europe/Berlin:20260702T090000
DTEND;TZID=Europe/Berlin:20260702T100000
END:VEVENT
BEGIN:VEVENT
UID:new-york
SUMMARY:NY
DTSTART;TZID=America/New_York:20261102T090000
DTEND;TZID=America/New_York:20261102T100000
END:VEVENT
BEGIN:VEVENT
UID:utc
SUMMARY:UTC
DTSTART:20261102T090000Z
DTEND:20261102T100000Z
END:VEVENT
BEGIN:VEVENT
UID:custom
SUMMARY:Custom offset
DTSTART;TZID=Custom +05:30:20261102T090000
DTEND;TZID=Custom +05:30:20261102T100000
END:VEVENT
BEGIN:VEVENT
UID:floating
SUMMARY:Floating
DTSTART:20261102T090000
DTEND:20261102T100000
END:VEVENT
)");

    const IcalParser::Result result = IcalParser::parse(feed, QDate(2026, 1, 1), QDate(2026, 12, 31));
    check(result.error.isEmpty(), QStringLiteral("timezone feed parses"));
    check(result.eventCount == 6, QStringLiteral("six events counted"));

    const Occurrence *winter = findByUid(result.occurrences, QStringLiteral("berlin-winter"));
    check(winter != nullptr, QStringLiteral("berlin winter found"));
    if (winter) {
        checkEqual(winter->startUtc, utc(2026, 11, 2, 8, 0), QStringLiteral("Berlin CET -> UTC"));
        checkEqual(winter->endUtc, utc(2026, 11, 2, 9, 0), QStringLiteral("Berlin CET end -> UTC"));
        check(!winter->allDay, QStringLiteral("Berlin winter is not all-day"));
    }

    const Occurrence *summer = findByUid(result.occurrences, QStringLiteral("berlin-summer"));
    check(summer != nullptr, QStringLiteral("berlin summer found"));
    if (summer) {
        checkEqual(summer->startUtc, utc(2026, 7, 2, 7, 0), QStringLiteral("Berlin CEST -> UTC (DST)"));
    }

    const Occurrence *ny = findByUid(result.occurrences, QStringLiteral("new-york"));
    check(ny != nullptr, QStringLiteral("new york found"));
    if (ny) {
        checkEqual(ny->startUtc, utc(2026, 11, 2, 14, 0), QStringLiteral("New York EST -> UTC"));
    }

    const Occurrence *u = findByUid(result.occurrences, QStringLiteral("utc"));
    check(u != nullptr, QStringLiteral("utc found"));
    if (u) {
        checkEqual(u->startUtc, utc(2026, 11, 2, 9, 0), QStringLiteral("UTC value stays UTC"));
    }

    const Occurrence *custom = findByUid(result.occurrences, QStringLiteral("custom"));
    check(custom != nullptr, QStringLiteral("custom zone found"));
    if (custom) {
        checkEqual(custom->startUtc, utc(2026, 11, 2, 3, 30), QStringLiteral("custom +05:30 -> UTC"));
    }

    const Occurrence *floating = findByUid(result.occurrences, QStringLiteral("floating"));
    check(floating != nullptr, QStringLiteral("floating event found"));
    if (floating) {
        const QDateTime expected = QDateTime(QDate(2026, 11, 2), QTime(9, 0), QTimeZone::LocalTime);
        checkEqual(floating->startUtc, expected, QStringLiteral("floating time uses local zone"));
    }
}

void testAllDayAndMultiDay()
{
    const QByteArray feed = ics(R"(BEGIN:VEVENT
UID:one-day
SUMMARY:One day
DTSTART;VALUE=DATE:20261105
DTEND;VALUE=DATE:20261106
END:VEVENT
BEGIN:VEVENT
UID:two-day
SUMMARY:Two days
DTSTART;VALUE=DATE:20261110
DTEND;VALUE=DATE:20261112
END:VEVENT
BEGIN:VEVENT
UID:no-end
SUMMARY:No end
DTSTART;VALUE=DATE:20261120
END:VEVENT
BEGIN:VEVENT
UID:long
SUMMARY:Crossing range start
DTSTART;VALUE=DATE:20261030
DTEND;VALUE=DATE:20261102
END:VEVENT
)");

    const IcalParser::Result result = IcalParser::parse(feed, QDate(2026, 11, 1), QDate(2026, 11, 30));
    check(result.error.isEmpty(), QStringLiteral("all-day feed parses"));

    const Occurrence *one = findByUid(result.occurrences, QStringLiteral("one-day"));
    check(one != nullptr, QStringLiteral("single all-day found"));
    if (one) {
        check(one->allDay, QStringLiteral("single all-day flag"));
        check(one->firstLocalDate() == QDate(2026, 11, 5), QStringLiteral("single all-day start date"));
        check(one->lastLocalDate() == QDate(2026, 11, 5), QStringLiteral("single all-day last date"));
    }

    const Occurrence *two = findByUid(result.occurrences, QStringLiteral("two-day"));
    check(two != nullptr, QStringLiteral("multi all-day found"));
    if (two) {
        check(two->allDay, QStringLiteral("multi all-day flag"));
        check(two->firstLocalDate() == QDate(2026, 11, 10), QStringLiteral("multi start"));
        check(two->lastLocalDate() == QDate(2026, 11, 11), QStringLiteral("multi covers DTEND-1"));
        check(two->endDateExclusive == QDate(2026, 11, 12), QStringLiteral("multi exclusive end"));
    }

    const Occurrence *noEnd = findByUid(result.occurrences, QStringLiteral("no-end"));
    check(noEnd != nullptr, QStringLiteral("all-day without DTEND found"));
    if (noEnd) {
        check(noEnd->firstLocalDate() == QDate(2026, 11, 20), QStringLiteral("no-end start"));
        check(noEnd->lastLocalDate() == QDate(2026, 11, 20), QStringLiteral("no-end is one day"));
    }

    const Occurrence *longEvent = findByUid(result.occurrences, QStringLiteral("long"));
    check(longEvent != nullptr, QStringLiteral("event overlapping range start included"));
    if (longEvent) {
        check(longEvent->firstLocalDate() == QDate(2026, 10, 30), QStringLiteral("overlap start date kept"));
        check(longEvent->intersects(QDate(2026, 11, 1), QDate(2026, 11, 30)),
              QStringLiteral("overlap intersects range"));
    }
}

void testRecurrenceWithExdates()
{
    const QByteArray feed = ics(R"(BEGIN:VEVENT
UID:weekly
SUMMARY:Weekly
DTSTART;TZID=Europe/Berlin:20261005T090000
DTEND;TZID=Europe/Berlin:20261005T100000
RRULE:FREQ=WEEKLY;COUNT=8
EXDATE;TZID=Europe/Berlin:20261019T090000,20261026T090000
END:VEVENT
)");

    const IcalParser::Result result = IcalParser::parse(feed, QDate(2026, 10, 1), QDate(2026, 10, 31));
    check(result.error.isEmpty(), QStringLiteral("recurrence feed parses"));

    // 5, 12, 19, 26 October, 2 November: 19 and 26 are excluded, 2 Nov is out of range.
    check(countByUid(result.occurrences, QStringLiteral("weekly#")) == 2,
          QStringLiteral("EXDATE removes instances (got %1)").arg(countByUid(result.occurrences, QStringLiteral("weekly#"))));

    const Occurrence *first = findByUid(result.occurrences, QStringLiteral("weekly#20261005"));
    check(first != nullptr, QStringLiteral("first instance present"));
    if (first) {
        checkEqual(first->startUtc, utc(2026, 10, 5, 7, 0), QStringLiteral("first instance is CEST"));
        check(first->uid == QStringLiteral("weekly#20261005T0900"), QStringLiteral("instance uid carries slot"));
    }

    const Occurrence *second = findByUid(result.occurrences, QStringLiteral("weekly#20261012"));
    check(second != nullptr, QStringLiteral("second instance present"));

    const Occurrence *excluded = findByUid(result.occurrences, QStringLiteral("weekly#20261019"));
    check(excluded == nullptr, QStringLiteral("EXDATE instance absent"));
}

void testRecurrenceUntilAndDst()
{
    const QByteArray feed = ics(R"(BEGIN:VEVENT
UID:until
SUMMARY:Until
DTSTART;TZID=Europe/Berlin:20261020T090000
DTEND;TZID=Europe/Berlin:20261020T100000
RRULE:FREQ=WEEKLY;UNTIL=20261110T090000Z
END:VEVENT
)");

    const IcalParser::Result result = IcalParser::parse(feed, QDate(2026, 10, 1), QDate(2026, 12, 31));
    // 20 Oct, 27 Oct, 3 Nov, 10 Nov (UNTIL is inclusive).
    check(countByUid(result.occurrences, QStringLiteral("until#")) == 4,
          QStringLiteral("UNTIL is inclusive (got %1)").arg(countByUid(result.occurrences, QStringLiteral("until#"))));

    const Occurrence *before = findByUid(result.occurrences, QStringLiteral("until#20261020"));
    check(before != nullptr, QStringLiteral("occurrence before DST change"));
    if (before) {
        checkEqual(before->startUtc, utc(2026, 10, 20, 7, 0), QStringLiteral("pre-DST offset +02:00"));
    }
    const Occurrence *after = findByUid(result.occurrences, QStringLiteral("until#20261103"));
    check(after != nullptr, QStringLiteral("occurrence after DST change"));
    if (after) {
        checkEqual(after->startUtc, utc(2026, 11, 3, 8, 0), QStringLiteral("post-DST offset +01:00"));
    }
}

void testOverridesAndCancellation()
{
    const QByteArray feed = ics(R"(BEGIN:VEVENT
UID:series
SUMMARY:Series
DTSTART;TZID=Europe/Berlin:20260105T090000
DTEND;TZID=Europe/Berlin:20260105T100000
RRULE:FREQ=WEEKLY
END:VEVENT
BEGIN:VEVENT
UID:series
RECURRENCE-ID;TZID=Europe/Berlin:20260112T090000
SUMMARY:Series moved
DTSTART;TZID=Europe/Berlin:20260112T140000
DTEND;TZID=Europe/Berlin:20260112T150000
END:VEVENT
BEGIN:VEVENT
UID:series
RECURRENCE-ID:20260119T080000Z
SUMMARY:Series moved again
DTSTART;TZID=Europe/Berlin:20260119T170000
DTEND;TZID=Europe/Berlin:20260119T180000
END:VEVENT
BEGIN:VEVENT
UID:series
RECURRENCE-ID;TZID=Europe/Berlin:20260126T090000
STATUS:CANCELLED
DTSTART;TZID=Europe/Berlin:20260126T090000
DTEND;TZID=Europe/Berlin:20260126T100000
END:VEVENT
BEGIN:VEVENT
UID:cancelled-master
SUMMARY:Whole series cancelled
DTSTART:20260105T120000Z
DTEND:20260105T130000Z
RRULE:FREQ=DAILY
STATUS:CANCELLED
END:VEVENT
BEGIN:VEVENT
UID:cancelled-single
SUMMARY:Cancelled single
DTSTART:20260107T120000Z
DTEND:20260107T130000Z
STATUS:CANCELLED
END:VEVENT
)");

    const IcalParser::Result result = IcalParser::parse(feed, QDate(2026, 1, 1), QDate(2026, 1, 31));
    check(result.error.isEmpty(), QStringLiteral("override feed parses"));

    const Occurrence *moved = findByUid(result.occurrences, QStringLiteral("series#20260112"));
    check(moved != nullptr, QStringLiteral("moved occurrence present"));
    if (moved) {
        check(moved->title == QStringLiteral("Series moved"), QStringLiteral("override title wins"));
        checkEqual(moved->startUtc, utc(2026, 1, 12, 13, 0), QStringLiteral("override time (14:00 CET)"));
        check(moved->uid == QStringLiteral("series#20260112T0900"),
              QStringLiteral("moved occurrence keeps original slot uid"));
    }

    // The original 09:00 slot must not survive alongside the moved occurrence.
    check(findByUid(result.occurrences, QStringLiteral("series#20260112T0900")) == moved,
          QStringLiteral("no duplicate at the original slot"));

    const Occurrence *movedAgain = findByUid(result.occurrences, QStringLiteral("series#20260119"));
    check(movedAgain != nullptr, QStringLiteral("UTC recurrence-id override present"));
    if (movedAgain) {
        checkEqual(movedAgain->startUtc, utc(2026, 1, 19, 16, 0), QStringLiteral("override time (17:00 CET)"));
    }

    check(findByUid(result.occurrences, QStringLiteral("series#20260126")) == nullptr,
          QStringLiteral("cancelled instance removed"));
    check(countByUid(result.occurrences, QStringLiteral("cancelled-master")) == 0,
          QStringLiteral("cancelled master removes series"));
    check(countByUid(result.occurrences, QStringLiteral("cancelled-single")) == 0,
          QStringLiteral("cancelled single event removed"));
}

void testLongRunningRecurrence()
{
    // An unbounded weekly series that started decades before the range: this
    // exercises the iterator fast-forward path.
    const QByteArray feed = ics(R"(BEGIN:VEVENT
UID:ancient
SUMMARY:Ancient weekly
DTSTART:20000103T090000Z
DTEND:20000103T100000Z
RRULE:FREQ=WEEKLY
END:VEVENT
)");

    const IcalParser::Result result = IcalParser::parse(feed, QDate(2026, 11, 1), QDate(2026, 11, 30));
    check(result.error.isEmpty(), QStringLiteral("long-running feed parses"));
    // 2, 9, 16, 23, 30 November 2026 are Mondays.
    check(countByUid(result.occurrences, QStringLiteral("ancient#")) == 5,
          QStringLiteral("fast-forwarded series has 5 November instances (got %1)")
              .arg(countByUid(result.occurrences, QStringLiteral("ancient#"))));
    const Occurrence *first = findByUid(result.occurrences, QStringLiteral("ancient#20261102"));
    check(first != nullptr, QStringLiteral("fast-forwarded first instance"));
    if (first) {
        checkEqual(first->startUtc, utc(2026, 11, 2, 9, 0), QStringLiteral("fast-forward keeps UTC time"));
    }
}

void testRobustness()
{
    IcalParser::Result empty = IcalParser::parse(QByteArray(), QDate(2026, 1, 1), QDate(2026, 1, 31));
    check(!empty.error.isEmpty(), QStringLiteral("empty feed reports an error"));

    IcalParser::Result garbage = IcalParser::parse("not a calendar at all\n", QDate(2026, 1, 1), QDate(2026, 1, 31));
    check(!garbage.error.isEmpty(), QStringLiteral("garbage reports an error"));

    // LF-only line endings and a UTF-8 BOM are common in hand-made feeds.
    QByteArray lf = "\xEF\xBB\xBF"
                    "BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\nUID:lf\n"
                    "SUMMARY:Line\\nfeed\nDTSTART:20261102T090000Z\nDTEND:20261102T100000Z\n"
                    "END:VEVENT\nEND:VCALENDAR\n";
    IcalParser::Result lfResult = IcalParser::parse(lf, QDate(2026, 11, 1), QDate(2026, 11, 30));
    check(lfResult.error.isEmpty(), QStringLiteral("LF-only feed parses"));
    check(countByUid(lfResult.occurrences, QStringLiteral("lf")) == 1, QStringLiteral("LF-only event found"));

    // Folded lines (RFC 5545 continuation).
    QByteArray folded = ics("BEGIN:VEVENT\r\nUID:folded\r\nSUMMARY:A very long tit\r\n le that was folded\r\n"
                            "DTSTART:20261102T090000Z\r\nDTEND:20261102T100000Z\r\nEND:VEVENT\r\n");
    IcalParser::Result foldedResult = IcalParser::parse(folded, QDate(2026, 11, 1), QDate(2026, 11, 30));
    const Occurrence *f = findByUid(foldedResult.occurrences, QStringLiteral("folded"));
    check(f != nullptr, QStringLiteral("folded event found"));
    if (f) {
        check(f->title == QStringLiteral("A very long title that was folded"), QStringLiteral("folded line unfolded"));
    }
}

void testRangeFiltering()
{
    const QByteArray feed = ics(R"(BEGIN:VEVENT
UID:inside
SUMMARY:Inside
DTSTART:20261115T090000Z
DTEND:20261115T100000Z
END:VEVENT
BEGIN:VEVENT
UID:outside
SUMMARY:Outside
DTSTART:20270115T090000Z
DTEND:20270115T100000Z
END:VEVENT
BEGIN:VEVENT
UID:timed-multi
SUMMARY:Timed multi-day
DTSTART:20261110T230000Z
DTEND:20261111T010000Z
END:VEVENT
)");

    const IcalParser::Result result = IcalParser::parse(feed, QDate(2026, 11, 1), QDate(2026, 11, 30));
    check(findByUid(result.occurrences, QStringLiteral("inside")) != nullptr,
          QStringLiteral("in-range event included"));
    check(findByUid(result.occurrences, QStringLiteral("outside")) == nullptr,
          QStringLiteral("out-of-range event excluded"));
    const Occurrence *multi = findByUid(result.occurrences, QStringLiteral("timed-multi"));
    check(multi != nullptr, QStringLiteral("timed multi-day event included"));
    if (multi) {
        check(multi->firstLocalDate() == multi->startUtc.toLocalTime().date(),
              QStringLiteral("multi-day first local date"));
        check(multi->lastLocalDate() >= multi->firstLocalDate(), QStringLiteral("multi-day last date sane"));
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testTimezoneConversion();
    testAllDayAndMultiDay();
    testRecurrenceWithExdates();
    testRecurrenceUntilAndDst();
    testOverridesAndCancellation();
    testLongRunningRecurrence();
    testRobustness();
    testRangeFiltering();

    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
