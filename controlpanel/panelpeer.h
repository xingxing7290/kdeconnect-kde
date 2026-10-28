/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include "panelruntime.h"
#include <QHash>
#include <QQueue>
#include <QSet>
#include <functional>

namespace ControlPanel
{
class PanelPeer : public QObject
{
    Q_OBJECT
public:
    using Executor = std::function<QString(const QJsonObject &)>;
    explicit PanelPeer(PanelRuntime *runtime, Executor executor, QObject *parent = nullptr);
    void receive(const QJsonObject &request);
    void sendConfiguration();

Q_SIGNALS:
    void packetReady(const QString &type, const QJsonObject &body);

private:
    struct Completed {
        QJsonObject request;
        QJsonObject result;
    };
    void sendState();
    void sendResult(const QString &id, const QString &error);
    PanelRuntime *m_runtime;
    Executor m_executor;
    QString m_sessionId;
    QSet<QString> m_retiredSessions;
    qint64 m_sequence = 0;
    QHash<QString, Completed> m_completed;
    QQueue<QString> m_order;
};
}
