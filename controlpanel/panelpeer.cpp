/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelpeer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace ControlPanel
{
namespace
{
bool integer(const QJsonValue &value, qint64 minimum, qint64 maximum)
{
    const auto number = value.toDouble(-1);
    return value.isDouble() && std::isfinite(number) && std::floor(number) == number && number >= minimum && number <= maximum;
}

bool uuid(const QJsonValue &value)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\\z"));
    return value.isString() && pattern.match(value.toString()).hasMatch();
}
}

PanelPeer::PanelPeer(PanelRuntime *runtime, Executor executor, QObject *parent)
    : QObject(parent)
    , m_runtime(runtime)
    , m_executor(std::move(executor))
{
    connect(runtime->store(), &PanelStore::changed, this, &PanelPeer::sendConfiguration);
    connect(runtime, &PanelRuntime::stateChanged, this, &PanelPeer::sendState);
    connect(runtime->store(), &PanelStore::errorOccurred, this, [this] {
        sendResult({}, QStringLiteral("config_unavailable"));
    });
}

void PanelPeer::sendConfiguration()
{
    auto *store = m_runtime->store();
    if (!store->isUsable()) {
        sendResult({}, QStringLiteral("config_unavailable"));
        return;
    }
    Q_EMIT packetReady(QStringLiteral("kdeconnect.controlpanel.config"),
                       {{QStringLiteral("protocolVersion"), 1}, {QStringLiteral("config"), store->document().publicConfig()}});
    sendState();
}

void PanelPeer::sendState()
{
    if (m_runtime->store()->isUsable()) {
        Q_EMIT packetReady(QStringLiteral("kdeconnect.controlpanel.state"), m_runtime->state());
    }
}

void PanelPeer::sendResult(const QString &id, const QString &error)
{
    Q_EMIT packetReady(QStringLiteral("kdeconnect.controlpanel.result"),
                       {{QStringLiteral("protocolVersion"), 1},
                        {QStringLiteral("requestId"), id},
                        {QStringLiteral("success"), error.isEmpty()},
                        {QStringLiteral("error"), error}});
}

void PanelPeer::receive(const QJsonObject &request)
{
    const QString id = uuid(request.value(QStringLiteral("requestId"))) ? request.value(QStringLiteral("requestId")).toString() : QString();
    if (QJsonDocument(request).toJson(QJsonDocument::Compact).size() > 4096) {
        sendResult(id, QStringLiteral("invalid_request"));
        return;
    }
    if (!integer(request.value(QStringLiteral("protocolVersion")), 1, 1)) {
        sendResult(id, QStringLiteral("unsupported_version"));
        return;
    }
    const QString operation = request.value(QStringLiteral("operation")).toString();
    QSet<QString> allowed{QStringLiteral("protocolVersion"), QStringLiteral("operation"), QStringLiteral("sessionId")};
    if (operation == QLatin1String("action")) {
        allowed.unite({QStringLiteral("requestId"), QStringLiteral("sequence"), QStringLiteral("revision"), QStringLiteral("buttonId")});
    } else if (operation == QLatin1String("asset")) {
        allowed.unite({QStringLiteral("revision"), QStringLiteral("assetId")});
    } else if (operation == QLatin1String("page")) {
        allowed.unite({QStringLiteral("revision"), QStringLiteral("pageId")});
    } else if (operation != QLatin1String("config")) {
        sendResult(id, QStringLiteral("invalid_request"));
        return;
    }
    for (auto it = request.begin(); it != request.end(); ++it) {
        if (!allowed.contains(it.key())) {
            sendResult(id, QStringLiteral("invalid_request"));
            return;
        }
    }
    if (!uuid(request.value(QStringLiteral("sessionId")))) {
        sendResult(id, QStringLiteral("invalid_session"));
        return;
    }
    const QString session = request.value(QStringLiteral("sessionId")).toString();
    if (operation == QLatin1String("config")) {
        if (m_sessionId != session) {
            if (m_retiredSessions.contains(session) || m_retiredSessions.size() >= 4096) {
                sendResult({}, QStringLiteral("invalid_session"));
                return;
            }
            if (!m_sessionId.isEmpty())
                m_retiredSessions.insert(m_sessionId);
            m_sessionId = session;
            m_sequence = 0;
            m_completed.clear();
            m_order.clear();
        }
        sendConfiguration();
        return;
    }
    if (session != m_sessionId) {
        sendResult(id, QStringLiteral("invalid_session"));
        return;
    }
    if (!integer(request.value(QStringLiteral("revision")), 0, PanelDocument::MaxRevision)) {
        sendResult(id, QStringLiteral("invalid_request"));
        return;
    }
    if (operation == QLatin1String("asset")) {
        const auto asset = request.value(QStringLiteral("assetId")).toString();
        const auto revision = request.value(QStringLiteral("revision")).toInteger();
        auto *store = m_runtime->store();
        QString error;
        bool referenced = false;
        for (const auto &page : store->document().privateConfig().value(QStringLiteral("pages")).toArray()) {
            for (const auto &button : page.toObject().value(QStringLiteral("slots")).toArray()) {
                if (!asset.isEmpty() && button.toObject().value(QStringLiteral("icon")).toString() == asset)
                    referenced = true;
            }
        }
        static const QRegularExpression hash(QStringLiteral("\\A[a-f0-9]{64}\\z"));
        if (!m_assetWindow.isValid() || m_assetWindow.elapsed() >= 1000) {
            m_assetWindow.start();
            m_assetRequests = 0;
        }
        if (!store->isUsable())
            error = QStringLiteral("config_unavailable");
        else if (revision != store->document().revision())
            error = QStringLiteral("stale_revision");
        else if (!request.value(QStringLiteral("assetId")).isString() || !hash.match(asset).hasMatch() || !referenced)
            error = QStringLiteral("unknown_asset");
        else if (++m_assetRequests > 16)
            error = QStringLiteral("asset_busy");
        if (error.isEmpty())
            Q_EMIT assetRequested(asset, revision, session);
        else
            Q_EMIT packetReady(QStringLiteral("kdeconnect.controlpanel.asset"),
                               {{QStringLiteral("protocolVersion"), 1},
                                {QStringLiteral("sessionId"), session},
                                {QStringLiteral("revision"), revision},
                                {QStringLiteral("assetId"), asset},
                                {QStringLiteral("size"), 0},
                                {QStringLiteral("error"), error}});
        if (error == QLatin1String("stale_revision"))
            sendConfiguration();
        return;
    }
    if (operation == QLatin1String("page")) {
        if (request.value(QStringLiteral("revision")).toInteger() != m_runtime->store()->document().revision()) {
            sendResult({}, QStringLiteral("stale_revision"));
            sendConfiguration();
        } else if (!request.value(QStringLiteral("pageId")).isString() || !m_runtime->selectPage(request.value(QStringLiteral("pageId")).toString())) {
            sendResult({}, QStringLiteral("unknown_page"));
        } else {
            sendState();
        }
        return;
    }
    if (id.isEmpty() || !integer(request.value(QStringLiteral("sequence")), 1, PanelDocument::MaxRevision)
        || !request.value(QStringLiteral("buttonId")).isString()) {
        sendResult(id, QStringLiteral("invalid_request"));
        return;
    }
    const auto completed = m_completed.constFind(id);
    if (completed != m_completed.constEnd()) {
        if (completed->request == request) {
            Q_EMIT packetReady(QStringLiteral("kdeconnect.controlpanel.result"), completed->result);
        } else {
            sendResult(id, QStringLiteral("invalid_request"));
        }
        return;
    }
    const qint64 sequence = request.value(QStringLiteral("sequence")).toInteger();
    if (sequence <= m_sequence) {
        sendResult(id, QStringLiteral("request_expired"));
        return;
    }
    m_sequence = sequence;
    auto *store = m_runtime->store();
    QString error;
    if (!store->isUsable()) {
        error = QStringLiteral("config_unavailable");
    } else if (request.value(QStringLiteral("revision")).toInteger() != store->document().revision()) {
        error = QStringLiteral("stale_revision");
    } else {
        const auto button = store->document().button(request.value(QStringLiteral("buttonId")).toString());
        error = button.isEmpty() ? QStringLiteral("unknown_button") : m_executor(button);
    }
    const QJsonObject result{{QStringLiteral("protocolVersion"), 1},
                             {QStringLiteral("requestId"), id},
                             {QStringLiteral("success"), error.isEmpty()},
                             {QStringLiteral("error"), error}};
    m_completed.insert(id, {request, result});
    m_order.enqueue(id);
    if (m_order.size() > 256) {
        m_completed.remove(m_order.dequeue());
    }
    Q_EMIT packetReady(QStringLiteral("kdeconnect.controlpanel.result"), result);
    if (error == QLatin1String("stale_revision")) {
        sendConfiguration();
    }
}
}

#include "moc_panelpeer.cpp"
