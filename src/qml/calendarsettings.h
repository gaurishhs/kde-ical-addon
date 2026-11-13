/*
    SPDX-FileCopyrightText: 2026 Google Calendar Plasma plugin authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QSet>
#include <QString>

#include "settings.h"

class CalendarListModel;
class SecretStore;

/*!
 * QML-facing configuration object for the plugin's page in the Digital Clock
 * settings.
 *
 * The page runs in the configuration dialog, which is a different process
 * object from the plugin instance, so it writes the same config file
 * (~/.config/googlecalendarrc) and the same KWallet entries. The running
 * plugin watches the config file and reloads automatically.
 *
 * Secret handling: the URL never leaves this object once stored. It is not a
 * Q_PROPERTY, it is not returned by any invokable, and it is never logged.
 */
class CalendarSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CalendarListModel *calendars READ calendars CONSTANT)
    Q_PROPERTY(int refreshMinutes READ refreshMinutes WRITE setRefreshMinutes NOTIFY refreshMinutesChanged)
    Q_PROPERTY(bool demoMode READ demoMode WRITE setDemoMode NOTIFY demoModeChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool walletAvailable READ walletAvailable NOTIFY walletStatusChanged)
    Q_PROPERTY(QString walletStatus READ walletStatus NOTIFY walletStatusChanged)

public:
    explicit CalendarSettings(QObject *parent = nullptr);
    ~CalendarSettings() override;

    CalendarListModel *calendars() const;

    int refreshMinutes() const;
    void setRefreshMinutes(int minutes);

    bool demoMode() const;
    void setDemoMode(bool demoMode);

    bool dirty() const;
    bool walletAvailable() const;
    QString walletStatus() const;

public Q_SLOTS:
    /*! Adds an empty calendar and returns its id so the page can ask for a URL. */
    QString addCalendar();
    void removeCalendar(const QString &id);

    void setName(const QString &id, const QString &name);
    void setEnabled(const QString &id, bool enabled);
    void setColor(const QString &id, const QString &color);

    /*! Validates and stores a private iCal URL in KWallet. Never returns the URL. */
    bool setUrl(const QString &id, const QString &url);
    void clearUrl(const QString &id);

    /*! Writes the configuration file; the running plugin picks it up by itself. */
    void saveConfig();

    /*! Discards unsaved edits and reloads from disk + KWallet. */
    void reload();

Q_SIGNALS:
    void refreshMinutesChanged();
    void demoModeChanged();
    void dirtyChanged();
    void walletStatusChanged();
    void saved();
    void error(const QString &message);

private:
    void markDirty();
    void setDirty(bool dirty);
    void requestSecrets();
    void onSecretsReady(const QHash<QString, QString> &urls);

    CalendarListModel *m_model = nullptr;
    SecretStore *m_secrets = nullptr;

    int m_refreshMinutes = 15;
    bool m_demoMode = false;
    bool m_dirty = false;
    bool m_walletAvailable = false;
    QString m_walletStatus;
    QSet<QString> m_idsWithUrl;
};
