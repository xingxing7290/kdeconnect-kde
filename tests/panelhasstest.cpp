/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>
#include <panelhass.h>

using namespace ControlPanel;

namespace
{
void send(QWebSocket *socket, const QJsonObject &message)
{
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

QJsonObject rawState(const QString &id, const QString &state, const QString &friendlyName, const QJsonObject &extraAttributes = {})
{
    QJsonObject attributes{{QStringLiteral("friendly_name"), friendlyName}};
    for (auto it = extraAttributes.constBegin(); it != extraAttributes.constEnd(); ++it)
        attributes.insert(it.key(), it.value());
    return QJsonObject{{QStringLiteral("entity_id"), id},
                       {QStringLiteral("state"), state},
                       {QStringLiteral("attributes"), attributes}};
}

QJsonObject findEntity(const QJsonArray &entities, const QString &id)
{
    for (const QJsonValue &value : entities) {
        const QJsonObject entity = value.toObject();
        if (entity.value(QStringLiteral("id")).toString() == id)
            return entity;
    }
    return {};
}

/**
 * Minimal Home Assistant WebSocket endpoint: handshake, full state read, subscription and
 * service calls. It deliberately records the last call so tests can assert the wire format.
 */
class FakeHaServer : public QObject
{
    Q_OBJECT
public:
    explicit FakeHaServer(QObject *parent = nullptr)
        : QObject(parent)
        , m_server(QStringLiteral("fake-hass"), QWebSocketServer::NonSecureMode, this)
    {
        connect(&m_server, &QWebSocketServer::newConnection, this, &FakeHaServer::onNewConnection);
    }

    bool start()
    {
        return m_server.listen(QHostAddress::LocalHost);
    }
    QString url() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
    }
    void emitStateChanged(const QJsonObject &state)
    {
        const QJsonObject event{{QStringLiteral("type"), QStringLiteral("event")},
                                {QStringLiteral("id"), 1},
                                {QStringLiteral("event"),
                                 QJsonObject{{QStringLiteral("event_type"), QStringLiteral("state_changed")},
                                             {QStringLiteral("data"),
                                              QJsonObject{{QStringLiteral("entity_id"), state.value(QStringLiteral("entity_id"))},
                                                          {QStringLiteral("new_state"), state}}}}}};
        for (QWebSocket *socket : m_sockets)
            send(socket, event);
    }

    QJsonArray states;
    bool rejectAuth = false;
    bool failCallService = false;
    QJsonObject lastCall;
    int connections = 0;

private Q_SLOTS:
    void onNewConnection()
    {
        while (QWebSocket *socket = m_server.nextPendingConnection()) {
            ++connections;
            m_sockets.append(socket);
            connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString &text) {
                handle(socket, text);
            });
            connect(socket, &QWebSocket::disconnected, this, [this, socket] {
                m_sockets.removeAll(socket);
                socket->deleteLater();
            });
            send(socket, QJsonObject{{QStringLiteral("type"), QStringLiteral("auth_required")}});
        }
    }

private:
    void handle(QWebSocket *socket, const QString &text)
    {
        const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8());
        if (!document.isObject())
            return;
        const QJsonObject message = document.object();
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("auth")) {
            if (rejectAuth)
                send(socket, QJsonObject{{QStringLiteral("type"), QStringLiteral("auth_invalid")}});
            else
                send(socket, QJsonObject{{QStringLiteral("type"), QStringLiteral("auth_ok")}});
            return;
        }
        if (type == QLatin1String("get_states") || type == QLatin1String("subscribe_events")) {
            const QJsonValue result = type == QLatin1String("get_states") ? QJsonValue(states) : QJsonValue(QJsonObject());
            send(socket, QJsonObject{{QStringLiteral("id"), message.value(QStringLiteral("id"))},
                                     {QStringLiteral("type"), QStringLiteral("result")},
                                     {QStringLiteral("success"), true},
                                     {QStringLiteral("result"), result}});
            return;
        }
        if (type == QLatin1String("call_service")) {
            lastCall = message;
            QJsonObject reply{{QStringLiteral("id"), message.value(QStringLiteral("id"))},
                              {QStringLiteral("type"), QStringLiteral("result")},
                              {QStringLiteral("success"), !failCallService}};
            if (failCallService)
                reply.insert(QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), QStringLiteral("hass_unknown_action")}});
            send(socket, reply);
        }
    }

    QWebSocketServer m_server;
    QList<QWebSocket *> m_sockets;
};

QJsonArray sampleStates()
{
    QJsonArray array;
    array.append(rawState(QStringLiteral("switch.kitchen"), QStringLiteral("on"), QStringLiteral("厨房灯")));
    array.append(rawState(QStringLiteral("light.living"), QStringLiteral("off"), QStringLiteral("客厅灯"),
                          QJsonObject{{QStringLiteral("brightness"), 128}}));
    array.append(rawState(QStringLiteral("sensor.temp"), QStringLiteral("23.5"), QStringLiteral("温度"),
                          QJsonObject{{QStringLiteral("unit_of_measurement"), QStringLiteral("°C")}}));
    // A controllable-but-unsupported domain must appear in the picker but never in the snapshot.
    array.append(rawState(QStringLiteral("climate.bedroom"), QStringLiteral("heat"), QStringLiteral("卧室空调")));
    return array;
}

QJsonObject config(const QString &url)
{
    return QJsonObject{{QStringLiteral("enabled"), true},
                       {QStringLiteral("url"), url},
                       {QStringLiteral("token"), QStringLiteral("test-token")},
                       {QStringLiteral("entities"), QJsonArray{
                                                        QJsonObject{{QStringLiteral("id"), QStringLiteral("switch.kitchen")},
                                                                    {QStringLiteral("type"), QStringLiteral("switch")},
                                                                    {QStringLiteral("name"), QStringLiteral("厨房主灯")},
                                                                    {QStringLiteral("group"), QStringLiteral("一楼")},
                                                                    {QStringLiteral("order"), 0}},
                                                        QJsonObject{{QStringLiteral("id"), QStringLiteral("light.living")},
                                                                    {QStringLiteral("type"), QStringLiteral("light")},
                                                                    {QStringLiteral("order"), 1}},
                                                        QJsonObject{{QStringLiteral("id"), QStringLiteral("sensor.temp")},
                                                                    {QStringLiteral("type"), QStringLiteral("sensor")},
                                                                    {QStringLiteral("order"), 2}},
                                                    }}};
}
} // namespace

class PanelHassTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sharedFixtureIsWireCompatible()
    {
        QFile file(QFINDTESTDATA("../docs/control-panel/fixtures/hass-snapshot.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        QVERIFY(document.isObject());
        const QJsonObject body = document.object();
        QCOMPARE(body.value(QStringLiteral("protocolVersion")).toInt(), 1);
        QCOMPARE(body.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
        QCOMPARE(body.value(QStringLiteral("entities")).toArray().size(), 3);
        QVERIFY(body.value(QStringLiteral("lastCommand")).toObject().value(QStringLiteral("error")).toString().isEmpty());
        QVERIFY(!QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact)).contains(QStringLiteral("token")));
    }

    void validatorsRejectMalformedInput()
    {
        QVERIFY(PanelHass::validEntityId(QStringLiteral("switch.kitchen")));
        QVERIFY(PanelHass::validEntityId(QStringLiteral("sensor.outside_temp_2")));
        QVERIFY(!PanelHass::validEntityId(QStringLiteral("Switch.Kitchen")));
        QVERIFY(!PanelHass::validEntityId(QStringLiteral("switch.")));
        QVERIFY(!PanelHass::validEntityId(QStringLiteral("switch.kitchen light")));
        QVERIFY(!PanelHass::validEntityId(QString(130, QLatin1Char('a'))));

        const QJsonObject good{{QStringLiteral("id"), QStringLiteral("switch.kitchen")},
                               {QStringLiteral("type"), QStringLiteral("switch")},
                               {QStringLiteral("name"), QStringLiteral("厨房灯")}};
        QVERIFY(PanelHass::validEntity(good));
        QJsonObject bad = good;
        bad.insert(QStringLiteral("type"), QStringLiteral("climate"));
        QVERIFY(!PanelHass::validEntity(bad));
        bad = good;
        bad.insert(QStringLiteral("name"), QString(PanelHass::MaxNameLength + 1, QLatin1Char('x')));
        QVERIFY(!PanelHass::validEntity(bad));
        bad = good;
        bad.insert(QStringLiteral("order"), PanelHass::MaxEntities + 1);
        QVERIFY(!PanelHass::validEntity(bad));
    }

    void fullSnapshotIsTrimmedAndCredentialFree()
    {
        FakeHaServer server;
        QVERIFY(server.start());
        server.states = sampleStates();
        PanelHass hass;
        hass.configure(config(server.url()));
        QTRY_COMPARE_WITH_TIMEOUT(hass.state().value(QStringLiteral("status")).toString(), QStringLiteral("ready"), 3000);

        const QJsonObject snapshot = hass.state();
        const QJsonArray entities = snapshot.value(QStringLiteral("entities")).toArray();
        QCOMPARE(entities.size(), 3); // climate is not selected
        QVERIFY(!snapshot.contains(QStringLiteral("lastCommand")));

        const QJsonObject kitchen = findEntity(entities, QStringLiteral("switch.kitchen"));
        QCOMPARE(kitchen.value(QStringLiteral("name")).toString(), QStringLiteral("厨房主灯")); // configured name wins
        QCOMPARE(kitchen.value(QStringLiteral("group")).toString(), QStringLiteral("一楼"));
        QVERIFY(!kitchen.contains(QStringLiteral("attributes")));
        const QJsonObject living = findEntity(entities, QStringLiteral("light.living"));
        QCOMPARE(living.value(QStringLiteral("brightness")).toInt(), 128);
        const QJsonObject sensor = findEntity(entities, QStringLiteral("sensor.temp"));
        QCOMPARE(sensor.value(QStringLiteral("unit")).toString(), QStringLiteral("°C"));

        const QByteArray wire = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
        QVERIFY(!wire.contains(QByteArrayLiteral("test-token")));
        QVERIFY(!wire.contains(QByteArrayLiteral("access_token")));
        QCOMPARE(hass.availableEntities().size(), 3); // switch + light + sensor, climate filtered
    }

    void stateChangedUpdatesOnlySelectedEntity()
    {
        FakeHaServer server;
        QVERIFY(server.start());
        server.states = sampleStates();
        PanelHass hass;
        hass.configure(config(server.url()));
        QTRY_COMPARE_WITH_TIMEOUT(hass.state().value(QStringLiteral("status")).toString(), QStringLiteral("ready"), 3000);

        server.emitStateChanged(rawState(QStringLiteral("switch.kitchen"), QStringLiteral("off"), QStringLiteral("厨房灯")));
        QTRY_COMPARE_WITH_TIMEOUT(
            findEntity(hass.state().value(QStringLiteral("entities")).toArray(), QStringLiteral("switch.kitchen")).value(QStringLiteral("state")).toString(),
            QStringLiteral("off"), 2000);
        QCOMPARE(findEntity(hass.state().value(QStringLiteral("entities")).toArray(), QStringLiteral("sensor.temp")).value(QStringLiteral("state")).toString(),
                 QStringLiteral("23.5"));

        // An entity that is not selected must never leak into the snapshot.
        server.emitStateChanged(rawState(QStringLiteral("switch.other"), QStringLiteral("on"), QStringLiteral("其它")));
        QTest::qWait(300);
        QCOMPARE(hass.state().value(QStringLiteral("entities")).toArray().size(), 3);
    }

    void callServiceValidatesBeforeSending()
    {
        PanelHass hass;
        QCOMPARE(hass.callService(QStringLiteral("switch.kitchen"), QStringLiteral("toggle"), -1), QStringLiteral("hass_disabled"));

        hass.configure(config(QStringLiteral("http://127.0.0.1:1")));
        QCOMPARE(hass.callService(QStringLiteral("switch.kitchen"), QStringLiteral("toggle"), -1), QStringLiteral("hass_offline"));
    }

    void callServiceSendsRequestAndReportsRejection()
    {
        FakeHaServer server;
        QVERIFY(server.start());
        server.states = sampleStates();
        PanelHass hass;
        hass.configure(config(server.url()));
        QTRY_COMPARE_WITH_TIMEOUT(hass.state().value(QStringLiteral("status")).toString(), QStringLiteral("ready"), 3000);

        QCOMPARE(hass.callService(QStringLiteral("switch.unknown"), QStringLiteral("toggle"), -1), QStringLiteral("hass_unknown_entity"));
        QCOMPARE(hass.callService(QStringLiteral("switch.kitchen"), QStringLiteral("dance"), -1), QStringLiteral("hass_unknown_action"));
        QCOMPARE(server.lastCall.isEmpty(), true);

        QVERIFY(hass.callService(QStringLiteral("light.living"), QStringLiteral("turn_on"), 128).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!server.lastCall.isEmpty(), 2000);
        QCOMPARE(server.lastCall.value(QStringLiteral("domain")).toString(), QStringLiteral("light"));
        QCOMPARE(server.lastCall.value(QStringLiteral("service")).toString(), QStringLiteral("turn_on"));
        QCOMPARE(server.lastCall.value(QStringLiteral("service_data")).toObject().value(QStringLiteral("brightness")).toInt(), 128);
        QTRY_VERIFY_WITH_TIMEOUT(hass.state().contains(QStringLiteral("lastCommand")), 2000);
        QVERIFY(hass.state().value(QStringLiteral("lastCommand")).toObject().value(QStringLiteral("error")).toString().isEmpty());

        // A rejected service call must not mark the whole connection as failed.
        server.failCallService = true;
        QVERIFY(hass.callService(QStringLiteral("switch.kitchen"), QStringLiteral("toggle"), -1).isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(hass.state().value(QStringLiteral("lastCommand")).toObject().value(QStringLiteral("error")).toString(),
                                  QStringLiteral("hass_unknown_action"), 2000);
        QCOMPARE(hass.state().value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
    }

    void authInvalidStopsReconnecting()
    {
        FakeHaServer server;
        QVERIFY(server.start());
        server.rejectAuth = true;
        PanelHass hass;
        hass.configure(config(server.url()));
        QTRY_COMPARE_WITH_TIMEOUT(hass.state().value(QStringLiteral("error")).toString(), QStringLiteral("hass_auth_invalid"), 3000);
        const int attempts = server.connections;
        QVERIFY(attempts >= 1);
        QTest::qWait(3500); // longer than the minimum reconnect delay
        QCOMPARE(server.connections, attempts);
    }

    void hostileStringsAreCleanedBeforeGoingToThePhone()
    {
        FakeHaServer server;
        QVERIFY(server.start());
        QJsonArray states;
        states.append(rawState(QStringLiteral("switch.kitchen"), QStringLiteral("on"),
                               QString(300, QLatin1Char('x')) + QChar(0x0001) + QStringLiteral("尾巴")));
        server.states = states;
        PanelHass hass;
        hass.configure(config(server.url()));
        QTRY_COMPARE_WITH_TIMEOUT(hass.state().value(QStringLiteral("status")).toString(), QStringLiteral("ready"), 3000);

        const QJsonObject kitchen = findEntity(hass.state().value(QStringLiteral("entities")).toArray(), QStringLiteral("switch.kitchen"));
        const QString name = kitchen.value(QStringLiteral("name")).toString();
        QVERIFY(name.size() <= PanelHass::MaxNameLength);
        for (const QChar character : name)
            QVERIFY(character.isPrint());
    }
};

QTEST_GUILESS_MAIN(PanelHassTest)
#include "panelhasstest.moc"
