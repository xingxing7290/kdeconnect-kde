/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

class QWebSocket;

namespace ControlPanel
{
/**
 * Keeps a WebSocket session with a Home Assistant instance and publishes a display-ready
 * snapshot of the selected entities.
 *
 * The HA address and long-lived token are desktop-private settings: they are read from the
 * private configuration, never copied into the public document, and never sent to the phone.
 * The phone only receives the trimmed entity snapshot produced by state().
 */
class PanelHass : public QObject
{
    Q_OBJECT
public:
    explicit PanelHass(QObject *parent = nullptr);
    ~PanelHass() override;

    /** Display-ready snapshot for the phone; contains no credentials. */
    QJsonObject state() const;
    /** Controllable entities discovered on the instance, for the desktop picker. */
    QJsonArray availableEntities() const;
    /** Applies the private configuration block (url, token, entities). */
    void configure(const QJsonObject &hass);
    /** Forces a full state refresh; reconnects when the socket is down. */
    void refresh();
    /** Requests a service call for one entity; returns an error code or an empty string. */
    QString callService(const QString &entityId, const QString &action, int brightness);

    static constexpr int MaxEntities = 120;
    static constexpr int MaxUrlLength = 200;
    static constexpr int MaxTokenLength = 512;
    static constexpr int MaxNameLength = 120;

    /** Validates one configured entity entry. Exposed for the document validator and tests. */
    static bool validEntity(const QJsonObject &entity);
    /** Normalises a Home Assistant entity id such as "switch.kitchen_light". */
    static bool validEntityId(const QString &value);

Q_SIGNALS:
    void changed();

private:
    void connectSocket();
    void closeSocket();
    void scheduleReconnect();
    void sendJson(const QJsonObject &message);
    void handleMessage(const QString &text);
    void handleResult(const QJsonObject &message);
    void handleEvent(const QJsonObject &message);
    void applyStates(const QJsonArray &states);
    void applyState(const QJsonObject &entity);
    void setStatus(const QString &status, const QString &error = QString());
    QJsonObject trimmedEntity(const QJsonValue &raw) const;
    void recompose();
    bool wanted(const QString &entityId) const;

    QWebSocket *m_socket = nullptr;
    QTimer m_reconnect;
    QTimer m_poll;
    QJsonObject m_snapshot;
    QHash<QString, QJsonObject> m_entities;
    QJsonArray m_available;
    QHash<QString, QJsonObject> m_selection;
    QString m_url;
    QString m_token;
    QString m_status = QStringLiteral("disabled");
    QString m_error;
    QString m_lastCommandEntity;
    QString m_lastCommandError;
    bool m_authenticated = false;
    bool m_subscribed = false;
    bool m_enabled = false;
    bool m_desired = false;
    int m_messageId = 1;
    int m_attempt = 0;
    int m_pollIndex = 0;
    qint64 m_fetchedAt = 0;
};
}
