/*
    SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "feedfetcher.h"

#include "settings.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>

namespace
{
constexpr int kTransferTimeoutMs = 30000;
/*! Refuse absurd bodies; a month-rich Google feed is well under 10 MB. */
constexpr qint64 kMaxBodyBytes = 32 * 1024 * 1024;
} // namespace

FeedFetcher::FeedFetcher(QNetworkAccessManager *network, QString calendarId, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_id(std::move(calendarId))
{
}

FeedFetcher::~FeedFetcher() = default;

bool FeedFetcher::isAllowedUrl(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty()) {
        return false;
    }
    if (url.scheme() == QLatin1String("https")) {
        return true;
    }
    // Loopback http is allowed so the test suite can run a local server.
    if (url.scheme() == QLatin1String("http")) {
        const QString host = url.host();
        return host == QLatin1String("127.0.0.1") || host == QLatin1String("::1") || host == QLatin1String("localhost");
    }
    return false;
}

void FeedFetcher::setUrl(const QString &url)
{
    m_url = url;
    m_metaLoaded = false;
    m_etag.clear();
    m_lastModified.clear();
    m_fetchedAt = QDateTime();
}

QString FeedFetcher::url() const
{
    return m_url;
}

bool FeedFetcher::hasUrl() const
{
    return !m_url.isEmpty();
}

QString FeedFetcher::cacheFilePath() const
{
    return Settings::cacheDir() + QLatin1Char('/') + m_id + QStringLiteral(".ics");
}

QString FeedFetcher::metaFilePath() const
{
    return Settings::cacheDir() + QLatin1Char('/') + m_id + QStringLiteral(".json");
}

void FeedFetcher::readMeta()
{
    if (m_metaLoaded) {
        return;
    }
    m_metaLoaded = true;

    QFile file(metaFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();

    // If the URL changed, the stored validators no longer apply and the cache
    // body belongs to a different document.
    const QString storedHash = obj.value(QStringLiteral("urlHash")).toString();
    const QString currentHash =
        QString::fromLatin1(QCryptographicHash::hash(m_url.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (!storedHash.isEmpty() && storedHash != currentHash) {
        m_etag.clear();
        m_lastModified.clear();
        m_fetchedAt = QDateTime();
        return;
    }

    m_etag = obj.value(QStringLiteral("etag")).toString();
    m_lastModified = obj.value(QStringLiteral("lastModified")).toString();
    m_fetchedAt = QDateTime::fromString(obj.value(QStringLiteral("fetchedAt")).toString(), Qt::ISODate);
}

void FeedFetcher::writeMeta(const QDateTime &fetchedAt)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("etag"), m_etag);
    obj.insert(QStringLiteral("lastModified"), m_lastModified);
    obj.insert(QStringLiteral("fetchedAt"), fetchedAt.toUTC().toString(Qt::ISODate));
    // Store only a hash of the URL: enough to detect a change, useless to an
    // attacker reading the cache directory.
    obj.insert(QStringLiteral("urlHash"),
               QString::fromLatin1(QCryptographicHash::hash(m_url.toUtf8(), QCryptographicHash::Sha256).toHex()));

    QSaveFile file(metaFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.commit();
}

QDateTime FeedFetcher::lastFetched() const
{
    return m_fetchedAt;
}

bool FeedFetcher::isBusy() const
{
    return m_busy;
}

void FeedFetcher::loadCache()
{
    QFile file(cacheFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    readMeta();
    const QByteArray data = file.readAll();
    if (!data.isEmpty()) {
        Q_EMIT cacheLoaded(data);
    }
}

void FeedFetcher::fetch()
{
    if (m_busy || m_url.isEmpty()) {
        return;
    }
    const QUrl url(m_url);
    if (!isAllowedUrl(url)) {
        Q_EMIT failed(QStringLiteral("Only https:// feed URLs are accepted."));
        return;
    }

    readMeta();

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kTransferTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("PlasmaGoogleCalendar/2.0 (KDE Plasma calendar plugin)"));
    if (!m_etag.isEmpty()) {
        request.setRawHeader("If-None-Match", m_etag.toUtf8());
    }
    if (!m_lastModified.isEmpty()) {
        request.setRawHeader("If-Modified-Since", m_lastModified.toUtf8());
    }

    m_busy = true;
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleReply(reply);
        reply->deleteLater();
    });
    // Cap the body size without buffering everything in memory first.
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (reply->bytesAvailable() > kMaxBodyBytes) {
            reply->abort();
        }
    });
}

void FeedFetcher::handleReply(QNetworkReply *reply)
{
    m_busy = false;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError error = reply->error();

    if (error == QNetworkReply::NoError && status == 304) {
        Q_EMIT notModified();
        return;
    }
    // Some servers send 304 with an empty body and NoError; others make it an
    // error. Treat either as "unchanged".
    if (status == 304) {
        Q_EMIT notModified();
        return;
    }
    if (error != QNetworkReply::NoError) {
        // Deliberately do not include reply->url(): it carries the secret.
        // Strip any URLs from the error message to avoid exposing private URLs.
        QString errorString = reply->errorString();
        // Remove URLs in the format "http://host/path" or "https://host/path"
        errorString.remove(QRegularExpression(QStringLiteral("https?://[^\\s\"']+")));
        Q_EMIT failed(QStringLiteral("Could not download the calendar feed: %1").arg(errorString));
        return;
    }
    if (status != 200) {
        Q_EMIT failed(QStringLiteral("The feed server returned HTTP %1.").arg(status));
        return;
    }

    const QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        Q_EMIT failed(QStringLiteral("The feed server returned an empty response."));
        return;
    }

    m_etag = QString::fromUtf8(reply->rawHeader("ETag"));
    m_lastModified = QString::fromUtf8(reply->rawHeader("Last-Modified"));
    m_fetchedAt = QDateTime::currentDateTimeUtc();

    QSaveFile file(cacheFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        if (!file.commit()) {
            // Not fatal: the in-memory data is still good.
        }
    }
    writeMeta(m_fetchedAt);

    Q_EMIT fetched(data);
}
