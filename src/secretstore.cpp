/*
    SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "secretstore.h"

#include <KWallet>

#include <QDebug>

namespace
{
/*! KWallet folder holding this plugin's entries. */
const QString kFolder = QStringLiteral("googlecalendar");

/*!
 * Diagnostics must never contain the secret itself, so nothing derived from a
 * URL is ever logged. This helper exists to make that intent explicit at the
 * call sites.
 */
constexpr bool kNeverLogSecrets = true;
} // namespace

SecretStore::SecretStore(QObject *parent)
    : QObject(parent)
{
    Q_UNUSED(kNeverLogSecrets);
}

SecretStore::~SecretStore() = default;

bool SecretStore::isAvailable() const
{
    return m_available;
}

void SecretStore::ensureOpen()
{
    if (m_wallet) {
        return;
    }
    if (m_openRequested) {
        return; // already waiting for walletOpened
    }

    if (!KWallet::Wallet::isEnabled()) {
        m_unavailableReason = QStringLiteral("KWallet is disabled in System Settings.");
        m_available = false;
        Q_EMIT availableChanged(false, m_unavailableReason);
        failPending(m_unavailableReason);
        return;
    }

    m_openRequested = true;

    // Asynchronous: never blocks, and walletOpened() arrives via the event
    // loop. WId 0 because the plugin has no window of its own.
    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), 0, KWallet::Wallet::Asynchronous);
    if (!wallet) {
        m_openRequested = false;
        m_unavailableReason = QStringLiteral("The KWallet service is not available.");
        m_available = false;
        Q_EMIT availableChanged(false, m_unavailableReason);
        failPending(m_unavailableReason);
        return;
    }

    connect(wallet, &KWallet::Wallet::walletOpened, this, [this, wallet](bool ok) {
        m_openRequested = false;
        if (!ok) {
            m_unavailableReason = QStringLiteral("KWallet could not be opened (it may be locked).");
            m_available = false;
            Q_EMIT availableChanged(false, m_unavailableReason);
            delete wallet;
            failPending(m_unavailableReason);
            return;
        }

        m_wallet = wallet;
        if (!m_wallet->hasFolder(kFolder)) {
            m_wallet->createFolder(kFolder);
        }
        if (!m_wallet->setFolder(kFolder)) {
            m_unavailableReason = QStringLiteral("KWallet refused to open the '%1' folder.").arg(kFolder);
            m_available = false;
            Q_EMIT availableChanged(false, m_unavailableReason);
            failPending(m_unavailableReason);
            return;
        }

        m_available = true;
        Q_EMIT availableChanged(true, QString());
        flush();
    });

    connect(wallet, &KWallet::Wallet::walletClosed, this, [this] {
        m_wallet = nullptr;
    });
}

void SecretStore::failPending(const QString &reason)
{
    const QStringList reads = m_pendingReads;
    m_pendingReads.clear();

    QQueue<QPair<QString, QString>> writes = m_pendingWrites;
    m_pendingWrites.clear();
    m_pendingRemovals.clear();

    if (!reads.isEmpty()) {
        Q_EMIT secretsReady({});
    }
    while (!writes.isEmpty()) {
        Q_EMIT stored(writes.dequeue().first, false, reason);
    }
}

void SecretStore::flush()
{
    if (!m_wallet) {
        return;
    }

    if (!m_pendingReads.isEmpty()) {
        const QStringList ids = m_pendingReads;
        m_pendingReads.clear();
        QHash<QString, QString> result;
        for (const QString &id : ids) {
            QByteArray value;
            if (m_wallet->readEntry(id, value) == 0 && !value.isEmpty()) {
                result.insert(id, QString::fromUtf8(value));
            }
        }
        Q_EMIT secretsReady(result);
    }

    while (!m_pendingWrites.isEmpty()) {
        const auto [id, url] = m_pendingWrites.dequeue();
        const int rc = m_wallet->writeEntry(id, url.toUtf8(), KWallet::Wallet::Password);
        Q_EMIT stored(id, rc == 0,
                      rc == 0 ? QString() : QStringLiteral("KWallet rejected the entry (error %1).").arg(rc));
    }

    while (!m_pendingRemovals.isEmpty()) {
        const QString id = m_pendingRemovals.takeFirst();
        m_wallet->removeEntry(id);
    }
}

void SecretStore::request(const QStringList &ids)
{
    if (ids.isEmpty()) {
        Q_EMIT secretsReady({});
        return;
    }
    for (const QString &id : ids) {
        if (!m_pendingReads.contains(id)) {
            m_pendingReads.append(id);
        }
    }
    ensureOpen();
    flush();
}

void SecretStore::store(const QString &id, const QString &url)
{
    if (id.isEmpty()) {
        Q_EMIT stored(id, false, QStringLiteral("Missing calendar id."));
        return;
    }
    m_pendingWrites.enqueue({id, url});
    ensureOpen();
    flush();
}

void SecretStore::remove(const QString &id)
{
    if (id.isEmpty()) {
        return;
    }
    m_pendingRemovals.append(id);
    ensureOpen();
    flush();
}
