/*
    SPDX-FileCopyrightText: 2026 iCal Calendar addon authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "types.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

/*!
 * The calendar list shown in the configuration page.
 *
 * The feed address is never part of this model; only whether an address is
 * stored (UrlSetRole) is exposed, so a QML binding can never leak it.
 */
class CalendarListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ColorRole,
        EnabledRole,
        UrlSetRole,
    };
    Q_ENUM(Roles)

    explicit CalendarListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    void resetRows(const QVector<CalendarSource> &sources, const QSet<QString> &idsWithUrl);
    void setUrlSet(const QSet<QString> &idsWithUrl);

    QVector<CalendarSource> sources() const;
    CalendarSource sourceForId(const QString &id) const;

    int rowForId(const QString &id) const;
    void setName(const QString &id, const QString &name);
    void setEnabled(const QString &id, bool enabled);
    void setColor(const QString &id, const QString &color);
    void appendRow(const CalendarSource &source);
    void removeRow(const QString &id);

Q_SIGNALS:
    void countChanged();

private:
    QVector<CalendarSource> m_sources;
    QSet<QString> m_idsWithUrl;
};
