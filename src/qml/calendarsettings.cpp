/*
    SPDX-FileCopyrightText: 2026 iCal Calendar addon authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "calendarsettings.h"

#include "calendarlistmodel.h"
#include "feedfetcher.h"
#include "secretstore.h"

#include <QColor>
#include <QStringList>
#include <QUrl>

#include <iterator>

namespace
{
const QString kDefaultColors[] = {
    QStringLiteral("#3daee9"), QStringLiteral("#2ecc71"), QStringLiteral("#e67e22"),
    QStringLiteral("#9b59b6"), QStringLiteral("#e74c3c"), QStringLiteral("#16a085"),
    QStringLiteral("#f1c40f"), QStringLiteral("#e91e63"),
};
} // namespace

CalendarSettings::CalendarSettings(QObject *parent)
    : QObject(parent)
    , m_model(new CalendarListModel(this))
    , m_secrets(new SecretStore(this))
{
    connect(m_secrets, &SecretStore::secretsReady, this, &CalendarSettings::onSecretsReady);
    connect(m_secrets, &SecretStore::stored, this, [this](const QString &id, bool ok, const QString &reason) {
        if (ok) {
            m_idsWithUrl.insert(id);
        } else {
            m_idsWithUrl.remove(id);
            Q_EMIT error(reason);
        }
        m_model->setUrlSet(m_idsWithUrl);
    });
    connect(m_secrets, &SecretStore::availableChanged, this, [this](bool available, const QString &reason) {
        m_walletAvailable = available;
        m_walletStatus = available ? QStringLiteral("Feed addresses are stored in KWallet.") : reason;
        Q_EMIT walletStatusChanged();
    });

    reload();
}

CalendarSettings::~CalendarSettings() = default;

CalendarListModel *CalendarSettings::calendars() const
{
    return m_model;
}

int CalendarSettings::refreshMinutes() const
{
    return m_refreshMinutes;
}

void CalendarSettings::setRefreshMinutes(int minutes)
{
    const int clamped = Settings::clampRefreshMinutes(minutes);
    if (m_refreshMinutes == clamped) {
        return;
    }
    m_refreshMinutes = clamped;
    markDirty();
    Q_EMIT refreshMinutesChanged();
}

bool CalendarSettings::demoMode() const
{
    return m_demoMode;
}

void CalendarSettings::setDemoMode(bool demoMode)
{
    if (m_demoMode == demoMode) {
        return;
    }
    m_demoMode = demoMode;
    markDirty();
    Q_EMIT demoModeChanged();
}

bool CalendarSettings::dirty() const
{
    return m_dirty;
}

bool CalendarSettings::walletAvailable() const
{
    return m_walletAvailable;
}

QString CalendarSettings::walletStatus() const
{
    return m_walletStatus;
}

void CalendarSettings::markDirty()
{
    setDirty(true);
}

void CalendarSettings::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        return;
    }
    m_dirty = dirty;
    Q_EMIT dirtyChanged();
}

void CalendarSettings::requestSecrets()
{
    QStringList ids;
    for (const CalendarSource &source : m_model->sources()) {
        ids.append(source.id);
    }
    m_secrets->request(ids);
}

void CalendarSettings::onSecretsReady(const QHash<QString, QString> &urls)
{
    // Only the *presence* of an entry is remembered; the value stays here and
    // is dropped immediately.
    m_idsWithUrl.clear();
    for (auto it = urls.cbegin(); it != urls.cend(); ++it) {
        if (!it.value().isEmpty()) {
            m_idsWithUrl.insert(it.key());
        }
    }
    m_model->setUrlSet(m_idsWithUrl);
}

void CalendarSettings::reload()
{
    const Settings::Snapshot snapshot = Settings::load();
    m_refreshMinutes = snapshot.refreshMinutes;
    m_demoMode = snapshot.demoMode;
    m_model->resetRows(snapshot.calendars, m_idsWithUrl);
    setDirty(false);
    Q_EMIT refreshMinutesChanged();
    Q_EMIT demoModeChanged();
    requestSecrets();
}

QString CalendarSettings::addCalendar()
{
    CalendarSource source;
    source.id = Settings::newId();
    source.name = QStringLiteral("Calendar %1").arg(m_model->count() + 1);
    source.color = kDefaultColors[m_model->count() % int(std::size(kDefaultColors))];
    source.enabled = true;

    m_model->appendRow(source);
    markDirty();
    return source.id;
}

void CalendarSettings::removeCalendar(const QString &id)
{
    if (id.isEmpty() || m_model->rowForId(id) < 0) {
        return;
    }
    m_model->removeRow(id);
    m_idsWithUrl.remove(id);
    // Drop the secret as well; otherwise a removed calendar would leave a
    // private URL behind in the wallet forever.
    m_secrets->remove(id);
    markDirty();
}

void CalendarSettings::setName(const QString &id, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (m_model->sourceForId(id).name == trimmed) {
        return;
    }
    m_model->setName(id, trimmed);
    markDirty();
}

void CalendarSettings::setEnabled(const QString &id, bool enabled)
{
    if (m_model->sourceForId(id).enabled == enabled) {
        return;
    }
    m_model->setEnabled(id, enabled);
    markDirty();
}

void CalendarSettings::setColor(const QString &id, const QString &color)
{
    if (!QColor::isValidColorName(color)) {
        return;
    }
    if (m_model->sourceForId(id).color == color) {
        return;
    }
    m_model->setColor(id, color);
    markDirty();
}

bool CalendarSettings::setUrl(const QString &id, const QString &url)
{
    if (m_model->rowForId(id) < 0) {
        Q_EMIT error(QStringLiteral("Unknown calendar."));
        return false;
    }

    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) {
        clearUrl(id);
        return true;
    }

    const QUrl parsed(trimmed);
    if (!FeedFetcher::isAllowedUrl(parsed)) {
        Q_EMIT error(QStringLiteral("Enter the iCal address exactly as your calendar provider shows it "
                                    "(it starts with https://)."));
        return false;
    }

    // Optimistically show the address as set; stored() corrects this if
    // KWallet rejects it.
    m_idsWithUrl.insert(id);
    m_model->setUrlSet(m_idsWithUrl);
    m_secrets->store(id, trimmed);
    markDirty();
    return true;
}

void CalendarSettings::clearUrl(const QString &id)
{
    if (id.isEmpty()) {
        return;
    }
    m_idsWithUrl.remove(id);
    m_model->setUrlSet(m_idsWithUrl);
    m_secrets->remove(id);
    markDirty();
}

void CalendarSettings::saveConfig()
{
    Settings::Snapshot snapshot;
    snapshot.calendars = m_model->sources();
    snapshot.refreshMinutes = m_refreshMinutes;
    snapshot.demoMode = m_demoMode;
    Settings::save(snapshot);
    setDirty(false);
    Q_EMIT saved();
}
