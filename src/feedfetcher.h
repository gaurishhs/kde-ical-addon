/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

/*!
 * Downloads one iCal feed and keeps a local copy for offline use.
 *
 * Responsibilities:
 *  - conditional GET using ETag / Last-Modified (RFC 9110)
 *  - store the body plus validators on disk
 *  - never log the URL: it may be a bearer secret
 *
 * All network work is asynchronous; nothing here blocks.
 */
class FeedFetcher : public QObject
{
    Q_OBJECT

public:
    FeedFetcher(QNetworkAccessManager *network, QString calendarId, QObject *parent = nullptr);
    ~FeedFetcher() override;

    void setUrl(const QString &url);
    QString url() const;
    bool hasUrl() const;

    /*! Emits cacheLoaded() when a local copy exists. */
    void loadCache();

    /*! Starts a conditional GET. Repeated calls while busy are ignored. */
    void fetch();

    bool isBusy() const;
    QDateTime lastFetched() const;

    /*! Only https, plus loopback http for tests. */
    static bool isAllowedUrl(const QUrl &url);

Q_SIGNALS:
    void cacheLoaded(const QByteArray &data);
    void fetched(const QByteArray &data);
    void notModified();
    void failed(const QString &message);

private:
    QString cacheFilePath() const;
    QString metaFilePath() const;
    void readMeta();
    void writeMeta(const QDateTime &fetchedAt);
    void handleReply(QNetworkReply *reply);

    QNetworkAccessManager *m_network;
    QString m_id;
    QString m_url;
    QString m_etag;
    QString m_lastModified;
    QDateTime m_fetchedAt;
    bool m_busy = false;
    bool m_metaLoaded = false;
};
