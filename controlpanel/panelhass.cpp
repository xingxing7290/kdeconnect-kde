/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelhass.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QVector>
#include <QWebSocket>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ControlPanel
{
namespace
{
constexpr int ReconnectMinMs = 3000;
constexpr int ReconnectMaxMs = 60000;
constexpr int PollIntervalMs = 120000;
constexpr qsizetype MaxMessage = 8 * 1024 * 1024;

bool finite(const QJsonValue &value, double minimum, double maximum)
{
    const auto result = value.toDouble(std::numeric_limits<double>::quiet_NaN());
    return value.isDouble() && std::isfinite(result) && result >= minimum && result <= maximum;
}

QString domainOf(const QString &entityId)
{
    const auto dot = entityId.indexOf(QLatin1Char('.'));
    return dot > 0 ? entityId.left(dot) : QString();
}
}

PanelHass::PanelHass(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket)
{
    m_socket->setParent(this);
    m_socket->setMaxAllowedIncomingMessageSize(MaxMessage);

    connect(m_socket, &QWebSocket::connected, this, [this] {
        setStatus(QStringLiteral("authenticating"));
        QJsonObject auth{
            {QStringLiteral("type"), QStringLiteral("auth")},
            {QStringLiteral("access_token"), m_token},
        };
        sendJson(auth);
    });
    connect(m_socket, &QWebSocket::textMessageReceived, this, &PanelHass::handleMessage);
    connect(m_socket, &QWebSocket::disconnected, this, [this] {
        m_authenticated = false;
        m_subscribed = false;
        if (m_desired) {
            setStatus(QStringLiteral("error"), m_socket->errorString().isEmpty() ? QStringLiteral("disconnected") : m_socket->errorString());
            scheduleReconnect();
        }
    });

    m_reconnect.setSingleShot(true);
    connect(&m_reconnect, &QTimer::timeout, this, &PanelHass::connectSocket);

    m_poll.setInterval(PollIntervalMs);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (!m_authenticated)
            return;
        // Fallback full refresh in case a subscription was dropped silently.
        QJsonObject request{
            {QStringLiteral("id"), m_messageId++},
            {QStringLiteral("type"), QStringLiteral("get_states")},
        };
        sendJson(request);
    });

    recompose();
}

PanelHass::~PanelHass()
{
    m_desired = false;
    closeSocket();
}

bool PanelHass::validEntityId(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[a-z_]{1,32}\\.[a-z0-9_]{1,96}\\z"));
    return value.size() <= 128 && pattern.match(value).hasMatch();
}

bool PanelHass::validEntity(const QJsonObject &entity)
{
    const auto id = entity.value(QStringLiteral("id"));
    if (!id.isString() || !validEntityId(id.toString()))
        return false;
    const auto name = entity.value(QStringLiteral("name"));
    if (!name.isUndefined() && (!name.isString() || name.toString().size() > MaxNameLength))
        return false;
    const auto type = entity.value(QStringLiteral("type")).toString();
    if (type != QLatin1String("switch") && type != QLatin1String("light") && type != QLatin1String("sensor"))
        return false;
    const auto group = entity.value(QStringLiteral("group"));
    if (!group.isUndefined() && (!group.isString() || group.toString().size() > 60))
        return false;
    if (!entity.value(QStringLiteral("order")).isUndefined() && !finite(entity.value(QStringLiteral("order")), 0, MaxEntities))
        return false;
    return true;
}

QJsonObject PanelHass::state() const
{
    return m_snapshot;
}

void PanelHass::configure(const QJsonObject &hass)
{
    const QString url = hass.value(QStringLiteral("url")).toString().trimmed();
    const QString token = hass.value(QStringLiteral("token")).toString().trimmed();
    const bool enabled = hass.value(QStringLiteral("enabled")).toBool(false);

    QHash<QString, QJsonObject> selection;
    QJsonArray configured = hass.value(QStringLiteral("entities")).toArray();
    for (const QJsonValue &value : configured) {
        const QJsonObject entity = value.toObject();
        if (!validEntity(entity) || selection.contains(entity.value(QStringLiteral("id")).toString()))
            continue;
        if (selection.size() >= MaxEntities)
            break;
        selection.insert(entity.value(QStringLiteral("id")).toString(), entity);
    }

    const bool connectionChanged = url != m_url || token != m_token || enabled != m_enabled;
    m_url = url;
    m_token = token;
    m_enabled = enabled;
    m_selection = selection;
    m_desired = enabled && !url.isEmpty() && !token.isEmpty();

    if (!m_desired) {
        m_entities.clear();
        m_status = QStringLiteral("disabled");
        m_error.clear();
        closeSocket();
        recompose();
        return;
    }
    if (connectionChanged) {
        closeSocket();
        m_attempt = 0;
        m_entities.clear();
        connectSocket();
    }
    recompose();
}

void PanelHass::refresh()
{
    if (!m_enabled || m_url.isEmpty() || m_token.isEmpty())
        return;
    if (!m_authenticated) {
        if (m_socket->state() == QAbstractSocket::UnconnectedState)
            connectSocket();
        return;
    }
    QJsonObject request{
        {QStringLiteral("id"), m_messageId++},
        {QStringLiteral("type"), QStringLiteral("get_states")},
    };
    sendJson(request);
}

QString PanelHass::callService(const QString &entityId, const QString &action, int brightness)
{
    const auto reject = [this, &entityId](const QString &code) {
        m_lastCommandEntity = entityId;
        m_lastCommandError = code;
        recompose();
        return code;
    };
    if (!m_enabled)
        return reject(QStringLiteral("hass_disabled"));
    if (!m_authenticated)
        return reject(QStringLiteral("hass_offline"));
    if (!validEntityId(entityId) || !m_selection.contains(entityId))
        return reject(QStringLiteral("hass_unknown_entity"));

    const QString domain = domainOf(entityId);
    if (domain != QLatin1String("switch") && domain != QLatin1String("light"))
        return reject(QStringLiteral("hass_unsupported_entity"));

    QString service = action;
    if (service != QLatin1String("turn_on") && service != QLatin1String("turn_off") && service != QLatin1String("toggle"))
        return reject(QStringLiteral("hass_unknown_action"));

    QJsonObject request{
        {QStringLiteral("id"), m_messageId++},
        {QStringLiteral("type"), QStringLiteral("call_service")},
        {QStringLiteral("domain"), domain},
        {QStringLiteral("service"), service},
        {QStringLiteral("target"), QJsonObject{{QStringLiteral("entity_id"), entityId}}},
    };
    if (domain == QLatin1String("light") && service == QLatin1String("turn_on") && brightness >= 0) {
        request.insert(QStringLiteral("service_data"),
                       QJsonObject{{QStringLiteral("brightness"), qBound(0, brightness, 255)}});
    }
    sendJson(request);
    m_lastCommandEntity = entityId;
    m_lastCommandError.clear();
    recompose();
    return {};
}

bool PanelHass::wanted(const QString &entityId) const
{
    return m_selection.contains(entityId);
}

void PanelHass::connectSocket()
{
    if (!m_desired)
        return;
    m_reconnect.stop();

    QUrl base(m_url);
    const QString scheme = base.scheme().toLower();
    if (!base.isValid() || base.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
        setStatus(QStringLiteral("error"), QStringLiteral("hass_invalid_url"));
        return;
    }

    QUrl endpoint;
    endpoint.setScheme(scheme == QLatin1String("https") ? QStringLiteral("wss") : QStringLiteral("ws"));
    endpoint.setHost(base.host());
    endpoint.setPort(base.port(8123));
    endpoint.setPath(QStringLiteral("/api/websocket"));

    m_authenticated = false;
    m_subscribed = false;
    setStatus(QStringLiteral("connecting"));
    m_socket->open(endpoint);
    m_poll.start();
}

void PanelHass::closeSocket()
{
    m_reconnect.stop();
    m_poll.stop();
    m_authenticated = false;
    m_subscribed = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
}

void PanelHass::scheduleReconnect()
{
    if (!m_desired)
        return;
    const int delay = qMin(ReconnectMaxMs, ReconnectMinMs * (1 << qMin(m_attempt, 4)));
    ++m_attempt;
    m_reconnect.start(delay);
}

void PanelHass::sendJson(const QJsonObject &message)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void PanelHass::setStatus(const QString &status, const QString &error)
{
    const bool statusChanged = status != m_status || error != m_error;
    m_status = status;
    m_error = error;
    if (statusChanged)
        recompose();
}

void PanelHass::handleMessage(const QString &text)
{
    const auto document = QJsonDocument::fromJson(text.toUtf8());
    if (!document.isObject())
        return;
    const QJsonObject message = document.object();
    const QString type = message.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("auth_required")) {
        QJsonObject auth{
            {QStringLiteral("type"), QStringLiteral("auth")},
            {QStringLiteral("access_token"), m_token},
        };
        sendJson(auth);
        return;
    }
    if (type == QLatin1String("auth_invalid")) {
        setStatus(QStringLiteral("error"), QStringLiteral("hass_auth_invalid"));
        closeSocket();
        return;
    }
    if (type == QLatin1String("auth_ok")) {
        m_authenticated = true;
        m_attempt = 0;
        setStatus(QStringLiteral("ready"));
        QJsonObject states{
            {QStringLiteral("id"), m_messageId++},
            {QStringLiteral("type"), QStringLiteral("get_states")},
        };
        sendJson(states);
        if (!m_subscribed) {
            QJsonObject subscribe{
                {QStringLiteral("id"), m_messageId++},
                {QStringLiteral("type"), QStringLiteral("subscribe_events")},
                {QStringLiteral("event_type"), QStringLiteral("state_changed")},
            };
            sendJson(subscribe);
            m_subscribed = true;
        }
        return;
    }
    if (type == QLatin1String("result")) {
        handleResult(message);
        return;
    }
    if (type == QLatin1String("event")) {
        handleEvent(message);
    }
}

void PanelHass::handleResult(const QJsonObject &message)
{
    if (!message.value(QStringLiteral("success")).toBool(false)) {
        const QJsonObject error = message.value(QStringLiteral("error")).toObject();
        setStatus(QStringLiteral("error"), error.value(QStringLiteral("code")).toString(QStringLiteral("hass_request_failed")));
        return;
    }
    const QJsonValue result = message.value(QStringLiteral("result"));
    if (result.isArray()) {
        applyStates(result.toArray());
        m_fetchedAt = QDateTime::currentSecsSinceEpoch();
        recompose();
    }
}

void PanelHass::handleEvent(const QJsonObject &message)
{
    const QJsonObject event = message.value(QStringLiteral("event")).toObject();
    if (event.value(QStringLiteral("event_type")).toString() != QLatin1String("state_changed"))
        return;
    const QJsonObject data = event.value(QStringLiteral("data")).toObject();
    const QJsonObject newState = data.value(QStringLiteral("new_state")).toObject();
    if (newState.isEmpty())
        return;
    const QString entityId = newState.value(QStringLiteral("entity_id")).toString();
    if (!wanted(entityId))
        return;
    applyState(newState);
    m_fetchedAt = QDateTime::currentSecsSinceEpoch();
    recompose();
}

void PanelHass::applyStates(const QJsonArray &states)
{
    m_entities.clear();
    m_available = QJsonArray();
    for (const QJsonValue &value : states) {
        const QJsonObject raw = value.toObject();
        const QString id = raw.value(QStringLiteral("entity_id")).toString();
        const QString domain = domainOf(id);
        if (domain == QLatin1String("switch") || domain == QLatin1String("light") || domain == QLatin1String("sensor")) {
            const QJsonObject attributes = raw.value(QStringLiteral("attributes")).toObject();
            if (m_available.size() < 500) {
                QJsonObject entry{
                    {QStringLiteral("id"), id},
                    {QStringLiteral("name"), attributes.value(QStringLiteral("friendly_name")).toString()},
                    {QStringLiteral("type"), domain},
                };
                if (domain == QLatin1String("sensor")) {
                    const QString unit = attributes.value(QStringLiteral("unit_of_measurement")).toString();
                    if (!unit.isEmpty() && unit.size() <= 16)
                        entry.insert(QStringLiteral("unit"), unit);
                }
                m_available.append(entry);
            }
        }
        if (wanted(id))
            m_entities.insert(id, raw);
    }
}

QJsonArray PanelHass::availableEntities() const
{
    return m_available;
}

void PanelHass::applyState(const QJsonObject &entity)
{
    const QString id = entity.value(QStringLiteral("entity_id")).toString();
    if (!id.isEmpty())
        m_entities.insert(id, entity);
}

QJsonObject PanelHass::trimmedEntity(const QJsonValue &raw) const
{
    const QJsonObject object = raw.toObject();
    const QString id = object.value(QStringLiteral("entity_id")).toString();
    const QJsonObject config = m_selection.value(id);
    if (config.isEmpty())
        return {};

    const QJsonObject attributes = object.value(QStringLiteral("attributes")).toObject();
    const QString type = config.value(QStringLiteral("type")).toString();
    const QString configuredName = config.value(QStringLiteral("name")).toString().trimmed();

    QJsonObject result{
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), type},
        {QStringLiteral("state"), object.value(QStringLiteral("state")).toString()},
        {QStringLiteral("name"), configuredName.isEmpty() ? attributes.value(QStringLiteral("friendly_name")).toString() : configuredName},
        {QStringLiteral("available"), !object.value(QStringLiteral("state")).toString().isEmpty()},
    };
    const QString group = config.value(QStringLiteral("group")).toString().trimmed();
    if (!group.isEmpty())
        result.insert(QStringLiteral("group"), group);
    const auto order = config.value(QStringLiteral("order"));
    if (order.isDouble())
        result.insert(QStringLiteral("order"), order.toInt());

    if (type == QLatin1String("light")) {
        const auto brightness = attributes.value(QStringLiteral("brightness"));
        if (finite(brightness, 0, 255))
            result.insert(QStringLiteral("brightness"), brightness.toInt());
    }
    if (type == QLatin1String("sensor")) {
        const QString unit = attributes.value(QStringLiteral("unit_of_measurement")).toString();
        if (!unit.isEmpty() && unit.size() <= 16)
            result.insert(QStringLiteral("unit"), unit);
    }
    return result;
}

void PanelHass::recompose()
{
    QJsonArray entities;
    QVector<QJsonObject> collected;
    collected.reserve(m_entities.size());
    for (auto it = m_entities.constBegin(); it != m_entities.constEnd(); ++it) {
        const QJsonObject trimmed = trimmedEntity(it.value());
        if (!trimmed.isEmpty())
            collected.append(trimmed);
    }
    std::sort(collected.begin(), collected.end(), [](const QJsonObject &left, const QJsonObject &right) {
        const int leftOrder = left.value(QStringLiteral("order")).toInt(1000);
        const int rightOrder = right.value(QStringLiteral("order")).toInt(1000);
        if (leftOrder != rightOrder)
            return leftOrder < rightOrder;
        return left.value(QStringLiteral("id")).toString() < right.value(QStringLiteral("id")).toString();
    });
    for (const QJsonObject &entity : collected)
        entities.append(entity);

    QJsonObject snapshot{
        {QStringLiteral("protocolVersion"), 1},
        {QStringLiteral("status"), m_status},
        {QStringLiteral("error"), m_error},
        {QStringLiteral("fetchedAt"), m_fetchedAt},
        {QStringLiteral("entities"), entities},
    };
    if (!m_lastCommandEntity.isEmpty()) {
        snapshot.insert(QStringLiteral("lastCommand"),
                        QJsonObject{{QStringLiteral("entityId"), m_lastCommandEntity},
                                    {QStringLiteral("error"), m_lastCommandError}});
    }
    if (snapshot != m_snapshot) {
        m_snapshot = snapshot;
        Q_EMIT changed();
    }
}
}
