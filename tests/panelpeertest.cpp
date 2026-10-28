/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <panelpeer.h>

using namespace ControlPanel;

class PanelPeerTest : public QObject
{
    Q_OBJECT
private:
    QString uuid()
    {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    void initialize(PanelStore &store)
    {
        QVERIFY(store.load());
        QFile file(QFINDTESTDATA("../docs/control-panel/fixtures/config-v1.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto config = QJsonDocument::fromJson(file.readAll()).object();
        auto pages = config.value(QStringLiteral("pages")).toArray();
        auto page = pages[0].toObject();
        auto buttons = page.value(QStringLiteral("slots")).toArray();
        auto button = buttons[0].toObject();
        button.insert(QStringLiteral("target"), QJsonObject{{QStringLiteral("url"), QStringLiteral("https://example.com/private")}});
        buttons[0] = button;
        page.insert(QStringLiteral("slots"), buttons);
        pages[0] = page;
        config.insert(QStringLiteral("pages"), pages);
        QVERIFY(store.save(config, store.document().revision()));
    }
    QJsonObject handshake(const QString &session)
    {
        return {{QStringLiteral("protocolVersion"), 1}, {QStringLiteral("operation"), QStringLiteral("config")}, {QStringLiteral("sessionId"), session}};
    }
    QJsonObject action(const PanelStore &store, const QString &session, qint64 sequence)
    {
        auto request = handshake(session);
        request.insert(QStringLiteral("operation"), QStringLiteral("action"));
        request.insert(QStringLiteral("sequence"), sequence);
        request.insert(QStringLiteral("requestId"), uuid());
        request.insert(QStringLiteral("revision"), store.document().revision());
        request.insert(QStringLiteral("buttonId"), QStringLiteral("docs"));
        return request;
    }
    QJsonObject lastResult(const QSignalSpy &spy)
    {
        for (auto it = spy.crbegin(); it != spy.crend(); ++it) {
            if (it->at(0).toString() == QLatin1String("kdeconnect.controlpanel.result"))
                return it->at(1).toJsonObject();
        }
        return {};
    }

private Q_SLOTS:
    void configurationIsPublicAndRequiresVersion()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        PanelPeer peer(&runtime, [](const QJsonObject &) {
            return QString();
        });
        QSignalSpy packets(&peer, &PanelPeer::packetReady);
        peer.receive(handshake(uuid()));
        QCOMPARE(packets.size(), 2);
        const auto config = packets.first()[1].toJsonObject();
        QVERIFY(config.contains(QStringLiteral("config")));
        QVERIFY(!QJsonDocument(config).toJson().contains("private"));
        auto invalid = handshake(uuid());
        invalid.insert(QStringLiteral("protocolVersion"), QStringLiteral("1"));
        peer.receive(invalid);
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("unsupported_version"));
    }

    void duplicateActionsExecuteOnceAndRevisionMismatchResynchronizes()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        int calls = 0;
        PanelPeer peer(&runtime, [&calls](const QJsonObject &button) {
            ++calls;
            return button.value(QStringLiteral("target")).isObject() ? QString() : QStringLiteral("invalid_target");
        });
        QSignalSpy packets(&peer, &PanelPeer::packetReady);
        const auto session = uuid();
        peer.receive(handshake(session));
        const auto request = action(store, session, 1);
        peer.receive(request);
        const auto result = lastResult(packets);
        QVERIFY(result.value(QStringLiteral("success")).toBool());
        peer.receive(request);
        QCOMPARE(lastResult(packets), result);
        QCOMPARE(calls, 1);
        auto collision = request;
        collision.insert(QStringLiteral("buttonId"), QStringLiteral("save"));
        peer.receive(collision);
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("invalid_request"));
        auto stale = action(store, session, 2);
        stale.insert(QStringLiteral("revision"), 0);
        peer.receive(stale);
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("stale_revision"));
        QCOMPARE(packets.last()[0].toString(), QStringLiteral("kdeconnect.controlpanel.state"));
        QCOMPARE(calls, 1);
    }

    void injectedTargetsAndMalformedValuesNeverReachExecutor()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        int calls = 0;
        PanelPeer peer(&runtime, [&calls](const QJsonObject &) {
            ++calls;
            return QString();
        });
        QSignalSpy packets(&peer, &PanelPeer::packetReady);
        const auto session = uuid();
        peer.receive(handshake(session));
        auto request = action(store, session, 1);
        request.insert(QStringLiteral("target"), QJsonObject{{QStringLiteral("path"), QStringLiteral("untrusted.exe")}});
        peer.receive(request);
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("invalid_request"));
        request = action(store, session, 2);
        request.insert(QStringLiteral("revision"), QString::number(store.document().revision()));
        peer.receive(request);
        request = action(store, session, 3);
        request.insert(QStringLiteral("sequence"), 3.5);
        peer.receive(request);
        request = action(store, uuid(), 4);
        peer.receive(request);
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("invalid_session"));
        request = action(store, session, 5);
        request.insert(QStringLiteral("buttonId"), QStringLiteral("missing"));
        peer.receive(request);
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("unknown_button"));
        QCOMPARE(calls, 0);
    }

    void replayAfterCacheEvictionIsRejectedAndRetiredSessionsCannotReturn()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        int calls = 0;
        PanelPeer peer(&runtime, [&calls](const QJsonObject &) {
            ++calls;
            return QString();
        });
        QSignalSpy packets(&peer, &PanelPeer::packetReady);
        const auto session = uuid();
        peer.receive(handshake(session));
        const auto first = action(store, session, 1);
        peer.receive(first);
        for (int index = 2; index <= 300; ++index)
            peer.receive(action(store, session, index));
        QCOMPARE(calls, 300);
        peer.receive(first);
        QCOMPARE(calls, 300);
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("request_expired"));
        const auto next = uuid();
        peer.receive(handshake(next));
        peer.receive(first);
        peer.receive(handshake(session));
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("invalid_session"));
        peer.receive(action(store, next, 1));
        QCOMPARE(calls, 301);
    }

    void pairedControllersShareActivePageAndSingleModeUsesHome()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        PanelPeer first(&runtime, [](const QJsonObject &) {
            return QString();
        });
        PanelPeer second(&runtime, [](const QJsonObject &) {
            return QString();
        });
        QSignalSpy packets(&second, &PanelPeer::packetReady);
        const auto session = uuid();
        first.receive(handshake(session));
        QJsonObject page{{QStringLiteral("protocolVersion"), 1},
                         {QStringLiteral("operation"), QStringLiteral("page")},
                         {QStringLiteral("sessionId"), session},
                         {QStringLiteral("revision"), store.document().revision()},
                         {QStringLiteral("pageId"), QStringLiteral("editor")}};
        first.receive(page);
        QCOMPARE(runtime.state().value(QStringLiteral("activePageId")).toString(), QStringLiteral("editor"));
        QCOMPARE(packets.last()[1].toJsonObject().value(QStringLiteral("activePageId")).toString(), QStringLiteral("editor"));
        auto config = store.document().privateConfig();
        config.insert(QStringLiteral("mode"), QStringLiteral("single"));
        QVERIFY(store.save(config, store.document().revision()));
        QCOMPARE(runtime.state().value(QStringLiteral("activePageId")).toString(), QStringLiteral("home"));
        QVERIFY(!runtime.selectPage(QStringLiteral("editor")));
    }

    void unreadableConfigurationDisablesExecution()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        int calls = 0;
        PanelPeer peer(&runtime, [&calls](const QJsonObject &) {
            ++calls;
            return QString();
        });
        QSignalSpy packets(&peer, &PanelPeer::packetReady);
        const auto session = uuid();
        peer.receive(handshake(session));
        QFile file(store.filePath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("broken");
        file.close();
        QVERIFY(!store.load());
        peer.receive(action(store, session, 1));
        QCOMPARE(lastResult(packets).value(QStringLiteral("error")).toString(), QStringLiteral("config_unavailable"));
        QCOMPARE(calls, 0);
    }
};

QTEST_GUILESS_MAIN(PanelPeerTest)
#include "panelpeertest.moc"
