/*
    SPDX-FileCopyrightText: 2026 iCal Calendar addon authors
    SPDX-License-Identifier: GPL-2.0-or-later

    FeedFetcher tests: conditional requests, offline cache and URL policy.

    The HTTP server is implemented in-process with QTcpServer so the test has no
    external dependencies and never touches the network.
*/

#include "feedfetcher.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

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

const QByteArray kIcs =
    "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:test-1\r\n"
    "SUMMARY:Test event\r\nDTSTART:20261102T090000Z\r\nDTEND:20261102T100000Z\r\n"
    "END:VEVENT\r\nEND:VCALENDAR\r\n";

class TestServer : public QTcpServer
{
public:
    explicit TestServer(QObject *parent = nullptr)
        : QTcpServer(parent)
    {
    }

    int requestCount = 0;
    QByteArray lastIfNoneMatch;
    QByteArray lastIfModifiedSince;
    QByteArray lastPath;

protected:
    void incomingConnection(qintptr socketDescriptor) override
    {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(socketDescriptor);

        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
            QByteArray request = socket->property("request").toByteArray();
            request += socket->readAll();
            socket->setProperty("request", request);

            const int headerEnd = request.indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                return;
            }

            ++requestCount;

            const QList<QByteArray> lines = request.left(headerEnd).split('\n');
            lastPath = lines.value(0).split(' ').value(1).trimmed();

            lastIfNoneMatch.clear();
            lastIfModifiedSince.clear();
            for (const QByteArray &line : lines) {
                const int colon = line.indexOf(':');
                if (colon < 0) {
                    continue;
                }
                const QByteArray name = line.left(colon).trimmed().toLower();
                const QByteArray value = line.mid(colon + 1).trimmed();
                if (name == "if-none-match") {
                    lastIfNoneMatch = value;
                } else if (name == "if-modified-since") {
                    lastIfModifiedSince = value;
                }
            }

            QByteArray response;
            if (lastPath.endsWith("/fail")) {
                response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            } else if (lastPath.endsWith("/lm")) {
                // A route that only supports Last-Modified validation.
                if (lastIfModifiedSince == "Mon, 02 Nov 2026 09:00:00 GMT") {
                    response = "HTTP/1.1 304 Not Modified\r\nConnection: close\r\n\r\n";
                } else {
                    response = "HTTP/1.1 200 OK\r\nContent-Type: text/calendar; charset=utf-8\r\n"
                               "Last-Modified: Mon, 02 Nov 2026 09:00:00 GMT\r\nContent-Length: "
                        + QByteArray::number(kIcs.size()) + "\r\nConnection: close\r\n\r\n" + kIcs;
                }
            } else if (lastIfNoneMatch == "\"v1\"") {
                response = "HTTP/1.1 304 Not Modified\r\nETag: \"v1\"\r\nConnection: close\r\n\r\n";
            } else {
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/calendar; charset=utf-8\r\n"
                           "ETag: \"v1\"\r\nLast-Modified: Mon, 02 Nov 2026 09:00:00 GMT\r\n"
                           "Content-Length: "
                    + QByteArray::number(kIcs.size()) + "\r\nConnection: close\r\n\r\n" + kIcs;
            }

            socket->write(response);
            socket->disconnectFromHost();
        });

        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
};

template<typename Func>
bool waitForSignal(FeedFetcher *fetcher, Func signal)
{
    QEventLoop loop;
    bool fired = false;

    const QMetaObject::Connection connection =
        QObject::connect(fetcher, signal, &loop, [&] {
            fired = true;
            loop.quit();
        });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(connection);
    return fired;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // Keep the test away from the real configuration and cache.
    auto *sandbox = new QTemporaryDir();
    qputenv("XDG_CACHE_HOME", sandbox->filePath(QStringLiteral("cache")).toUtf8());
    qputenv("XDG_CONFIG_HOME", sandbox->filePath(QStringLiteral("config")).toUtf8());

    TestServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::fprintf(stderr, "FAIL: could not start the test server\n");
        return 1;
    }

    // The path deliberately looks like a secret bearer token: no error message
    // or log line may ever contain it.
    const QString secret = QStringLiteral("super-secret-token");
    const QString base = QStringLiteral("http://127.0.0.1:%1/%2/feed.ics").arg(server.serverPort()).arg(secret);

    QNetworkAccessManager network;

    check(FeedFetcher::isAllowedUrl(QUrl(QStringLiteral("https://example.com/x/basic.ics"))),
          QStringLiteral("https URLs are allowed"));
    check(FeedFetcher::isAllowedUrl(QUrl(base)), QStringLiteral("loopback http is allowed for tests"));
    check(!FeedFetcher::isAllowedUrl(QUrl(QStringLiteral("http://example.com/x.ics"))),
          QStringLiteral("non-loopback http is rejected"));
    check(!FeedFetcher::isAllowedUrl(QUrl(QStringLiteral("ftp://example.com/x.ics"))),
          QStringLiteral("other schemes are rejected"));
    check(!FeedFetcher::isAllowedUrl(QUrl(QStringLiteral("https:///nohost"))),
          QStringLiteral("URLs without a host are rejected"));

    FeedFetcher fetcher(&network, QStringLiteral("feed-test-1"));
    fetcher.setUrl(base);
    check(fetcher.hasUrl(), QStringLiteral("hasUrl after setUrl"));

    QByteArray received;
    QObject::connect(&fetcher, &FeedFetcher::fetched, &app, [&received](const QByteArray &data) {
        received = data;
    });

    fetcher.fetch();
    check(fetcher.isBusy(), QStringLiteral("fetch is asynchronous"));
    check(waitForSignal(&fetcher, &FeedFetcher::fetched), QStringLiteral("first fetch completes"));
    check(received == kIcs, QStringLiteral("first fetch returns the feed"));
    check(server.requestCount == 1, QStringLiteral("first request reached the server"));
    check(server.lastIfNoneMatch.isEmpty(), QStringLiteral("first request is unconditional"));
    check(fetcher.lastFetched().isValid(), QStringLiteral("lastFetched is set after a 200"));

    // Second request must be conditional and the server answers 304.
    fetcher.fetch();
    const bool notModified = waitForSignal(&fetcher, &FeedFetcher::notModified);
    check(notModified, QStringLiteral("conditional request yields 304"));
    check(server.requestCount == 2, QStringLiteral("second request reached the server"));
    check(server.lastIfNoneMatch == "\"v1\"", QStringLiteral("If-None-Match carries the ETag"));
    check(!fetcher.isBusy(), QStringLiteral("fetcher is idle after 304"));

    // A fresh instance with the same id must be able to serve the cached body.
    FeedFetcher cached(&network, QStringLiteral("feed-test-1"));
    cached.setUrl(base);
    QByteArray cachedData;
    QObject::connect(&cached, &FeedFetcher::cacheLoaded, &app, [&cachedData](const QByteArray &data) {
        cachedData = data;
    });
    cached.loadCache();
    check(cachedData == kIcs, QStringLiteral("offline cache is readable by a new instance"));
    check(cached.lastFetched().isValid(), QStringLiteral("cache metadata is restored"));

    // Last-Modified validation, used by servers that do not send ETag.
    const QString lmUrl = QStringLiteral("http://127.0.0.1:%1/%2/lm").arg(server.serverPort()).arg(secret);
    FeedFetcher modified(&network, QStringLiteral("feed-test-2"));
    modified.setUrl(lmUrl);
    QByteArray lmData;
    QObject::connect(&modified, &FeedFetcher::fetched, &app, [&lmData](const QByteArray &data) {
        lmData = data;
    });
    modified.fetch();
    check(waitForSignal(&modified, &FeedFetcher::fetched), QStringLiteral("Last-Modified route serves 200 first"));
    check(lmData == kIcs, QStringLiteral("Last-Modified route body is correct"));
    check(server.lastIfModifiedSince.isEmpty(), QStringLiteral("first Last-Modified request is unconditional"));

    modified.fetch();
    check(waitForSignal(&modified, &FeedFetcher::notModified), QStringLiteral("Last-Modified route yields 304"));
    check(server.lastIfModifiedSince == "Mon, 02 Nov 2026 09:00:00 GMT",
          QStringLiteral("If-Modified-Since carries the server value"));

    // Error path: no secret may leak into the message.
    FeedFetcher failing(&network, QStringLiteral("feed-test-3"));
    failing.setUrl(QStringLiteral("http://127.0.0.1:%1/%2/fail").arg(server.serverPort()).arg(secret));
    QString errorMessage;
    QObject::connect(&failing, &FeedFetcher::failed, &app, [&errorMessage](const QString &message) {
        errorMessage = message;
    });
    failing.fetch();
    check(waitForSignal(&failing, &FeedFetcher::failed), QStringLiteral("HTTP 500 reports a failure"));
    check(!errorMessage.contains(secret), QStringLiteral("error message does not contain the private URL"));
    check(!errorMessage.contains(QStringLiteral("127.0.0.1")),
          QStringLiteral("error message does not contain the feed host"));

    // Nothing is written for a rejected scheme.
    FeedFetcher rejected(&network, QStringLiteral("feed-test-4"));
    rejected.setUrl(QStringLiteral("ftp://example.com/x.ics"));
    QString rejectedMessage;
    QObject::connect(&rejected, &FeedFetcher::failed, &app, [&rejectedMessage](const QString &message) {
        rejectedMessage = message;
    });
    rejected.fetch();
    check(!rejectedMessage.isEmpty(), QStringLiteral("disallowed scheme reports a failure"));
    check(!QFile::exists(Settings::cacheDir() + QStringLiteral("/feed-test-4.ics")),
          QStringLiteral("no cache file for a rejected URL"));

    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
