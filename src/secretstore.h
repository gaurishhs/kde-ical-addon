/*
    SPDX-FileCopyrightText: 2026 iCal Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>

namespace KWallet
{
class Wallet;
}

/*!
 * Storage for the private iCal URLs.
 *
 * The URLs are bearer secrets: anybody who has one can read the calendar, so
 * they are kept in KWallet rather than in a plain config file, they are never
 * written to logs, and they are never put in error messages.
 *
 * Everything here is asynchronous; opening the wallet can involve a user
 * prompt and must never block the caller (which, inside plasmashell, is the
 * Plasma UI thread).
 */
class SecretStore : public QObject
{
    Q_OBJECT

public:
    explicit SecretStore(QObject *parent = nullptr);
    ~SecretStore() override;

    /*! Asynchronously read the URLs for \a ids. Always ends in secretsReady(). */
    void request(const QStringList &ids);

    /*! Asynchronously write one URL. */
    void store(const QString &id, const QString &url);

    /*! Asynchronously delete one URL. */
    void remove(const QString &id);

    /*! True when the wallet backend is known to be usable. */
    bool isAvailable() const;

Q_SIGNALS:
    /*! Emitted in reply to request(). Missing ids are simply absent. */
    void secretsReady(const QHash<QString, QString> &urls);
    void stored(const QString &id, bool ok, const QString &error);
    void availableChanged(bool available, const QString &reason);

private:
    void ensureOpen();
    void flush();
    void failPending(const QString &reason);

    KWallet::Wallet *m_wallet = nullptr;
    bool m_openRequested = false;
    bool m_available = false;
    QString m_unavailableReason;

    QStringList m_pendingReads;
    QQueue<QPair<QString, QString>> m_pendingWrites;
    QStringList m_pendingRemovals;
};
