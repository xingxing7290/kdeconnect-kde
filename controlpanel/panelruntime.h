/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include "panelstore.h"

namespace ControlPanel
{
class PanelRuntime : public QObject
{
    Q_OBJECT
public:
    explicit PanelRuntime(PanelStore *store, QObject *parent = nullptr);
    PanelStore *store() const;
    QJsonObject state() const;
    bool selectPage(const QString &pageId);
    void setForegroundApplication(const QString &application);

Q_SIGNALS:
    void stateChanged();

private:
    QString followedPage(const QJsonObject &config) const;
    PanelStore *m_store;
    QString m_activePageId;
    QString m_foregroundApplication;
    QString m_manualPageId;
    QString m_mode;
};
}
