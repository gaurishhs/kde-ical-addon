/*
    SPDX-FileCopyrightText: 2026 iCal Calendar addon authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "calendarlistmodel.h"

#include <QModelIndex>

CalendarListModel::CalendarListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CalendarListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_sources.size());
}

QVariant CalendarListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sources.size()) {
        return {};
    }
    const CalendarSource &source = m_sources.at(index.row());
    switch (role) {
    case IdRole:
        return source.id;
    case NameRole:
        return source.name;
    case ColorRole:
        return source.color;
    case EnabledRole:
        return source.enabled;
    case UrlSetRole:
        return m_idsWithUrl.contains(source.id);
    default:
        return {};
    }
}

QHash<int, QByteArray> CalendarListModel::roleNames() const
{
    return {
        {IdRole, "calendarId"},
        {NameRole, "calendarName"},
        {ColorRole, "calendarColor"},
        {EnabledRole, "calendarEnabled"},
        {UrlSetRole, "calendarHasUrl"},
    };
}

int CalendarListModel::count() const
{
    return int(m_sources.size());
}

void CalendarListModel::resetRows(const QVector<CalendarSource> &sources, const QSet<QString> &idsWithUrl)
{
    beginResetModel();
    m_sources = sources;
    m_idsWithUrl = idsWithUrl;
    endResetModel();
    Q_EMIT countChanged();
}

void CalendarListModel::setUrlSet(const QSet<QString> &idsWithUrl)
{
    m_idsWithUrl = idsWithUrl;
    if (!m_sources.isEmpty()) {
        Q_EMIT dataChanged(index(0, 0), index(int(m_sources.size()) - 1, 0), {UrlSetRole});
    }
}

QVector<CalendarSource> CalendarListModel::sources() const
{
    return m_sources;
}

CalendarSource CalendarListModel::sourceForId(const QString &id) const
{
    const int row = rowForId(id);
    return row >= 0 ? m_sources.at(row) : CalendarSource();
}

int CalendarListModel::rowForId(const QString &id) const
{
    for (int row = 0; row < m_sources.size(); ++row) {
        if (m_sources.at(row).id == id) {
            return row;
        }
    }
    return -1;
}

void CalendarListModel::setName(const QString &id, const QString &name)
{
    const int row = rowForId(id);
    if (row < 0 || m_sources.at(row).name == name) {
        return;
    }
    m_sources[row].name = name;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), {NameRole});
}

void CalendarListModel::setEnabled(const QString &id, bool enabled)
{
    const int row = rowForId(id);
    if (row < 0 || m_sources.at(row).enabled == enabled) {
        return;
    }
    m_sources[row].enabled = enabled;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), {EnabledRole});
}

void CalendarListModel::setColor(const QString &id, const QString &color)
{
    const int row = rowForId(id);
    if (row < 0 || m_sources.at(row).color == color) {
        return;
    }
    m_sources[row].color = color;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), {ColorRole});
}

void CalendarListModel::appendRow(const CalendarSource &source)
{
    const int row = int(m_sources.size());
    beginInsertRows(QModelIndex(), row, row);
    m_sources.append(source);
    endInsertRows();
    Q_EMIT countChanged();
}

void CalendarListModel::removeRow(const QString &id)
{
    const int row = rowForId(id);
    if (row < 0) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_sources.remove(row);
    m_idsWithUrl.remove(id);
    endRemoveRows();
    Q_EMIT countChanged();
}
