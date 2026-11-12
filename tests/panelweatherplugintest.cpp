/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QUuid>
#include <core/device.h>
#include <core/kdeconnectconfig.h>
#include <core/kdeconnectplugin.h>
#include <core/pluginloader.h>
#include <paneleditor.h>
#include <panelweather.h>

class WeatherPacketDevice : public Device
{
public:
    explicit WeatherPacketDevice(const QString &id)
        : Device(nullptr, id)
    {
    }
    bool isReachable() const override
    {
        return reachable;
    }
    bool sendPacket(NetworkPacket &packet) override
    {
        const auto body = QJsonObject::fromVariantMap(packet.body());
        packets.append(QJsonObject{{QStringLiteral("type"), packet.type()}, {QStringLiteral("body"), body}});
        if (body.value(QStringLiteral("sessionId")).toString() != session)
            return true;
        if (packet.type() == QLatin1String("kdeconnect.controlpanel.config")) {
            const auto config = body.value(QStringLiteral("config")).toObject();
            const auto nextCity = config.value(QStringLiteral("idle")).toObject().value(QStringLiteral("city")).toString();
            if (nextCity != city)
                acceptedWeather = {};
            city = nextCity;
            revision = config.value(QStringLiteral("revision")).toInteger();
        } else if (packet.type() == QLatin1String("kdeconnect.controlpanel.dial") && body.value(QStringLiteral("revision")).toInteger() == revision
                   && body.value(QStringLiteral("city")).toString() == city) {
            acceptedWeather = body;
        }
        return true;
    }
    bool reachable = true;
    QString session = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString city;
    qint64 revision = -1;
    QJsonObject acceptedWeather;
    QJsonArray packets;
};

class PanelWeatherPluginTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;
    std::unique_ptr<ControlPanel::PanelEditor> editor;
    QJsonObject observation;

    std::unique_ptr<KdeConnectPlugin> create(WeatherPacketDevice &device)
    {
        auto *plugin = PluginLoader::instance()->instantiatePluginForDevice(QStringLiteral("kdeconnect_controlpanel"), &device);
        if (!plugin)
            return {};
        NetworkPacket request(
            QStringLiteral("kdeconnect.controlpanel.request"),
            {{QStringLiteral("protocolVersion"), 1}, {QStringLiteral("operation"), QStringLiteral("config")}, {QStringLiteral("sessionId"), device.session}});
        plugin->receivePacket(request);
        return std::unique_ptr<KdeConnectPlugin>(plugin);
    }
    QByteArray packetTrace(const WeatherPacketDevice &device) const
    {
        return QJsonDocument(device.packets).toJson(QJsonDocument::Compact);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        qputenv("KDECONNECT_CONFIG_DIR", QDir(directory.path()).filePath(QStringLiteral("identity")).toUtf8());
        qputenv("KDECONNECT_PANEL_CONFIG_DIR", QDir(directory.path()).filePath(QStringLiteral("panel")).toUtf8());
        // Device only records in-process packets; do not join the user's D-Bus.
        qputenv("DBUS_SESSION_BUS_ADDRESS", "disabled:");
        editor = std::make_unique<ControlPanel::PanelEditor>();
        QVERIFY(editor->error().isEmpty());
        QVERIFY(editor->setIdleOptions({{QStringLiteral("enabled"), false}, {QStringLiteral("city"), QString()}}));
        const auto now = QDateTime::currentSecsSinceEpoch();
        const auto date = QDateTime::fromSecsSinceEpoch(now + 28800, QTimeZone::UTC).date();
        QJsonArray days;
        for (int index = 0; index < 3; ++index)
            days.append(QJsonObject{{QStringLiteral("date"), date.addDays(index).toString(Qt::ISODate)},
                                    {QStringLiteral("minimum"), 10.0},
                                    {QStringLiteral("maximum"), 20.0},
                                    {QStringLiteral("weatherCode"), 3}});
        observation = {{QStringLiteral("city"), QStringLiteral("Packet Order Test City")},
                       {QStringLiteral("resolvedCity"), QStringLiteral("Synthetic packet-order fixture")},
                       {QStringLiteral("source"), QStringLiteral("open-meteo")},
                       {QStringLiteral("status"), QStringLiteral("ready")},
                       {QStringLiteral("fetchedAt"), now},
                       {QStringLiteral("observedAt"), now - 300},
                       {QStringLiteral("temperature"), 17.25},
                       {QStringLiteral("weatherCode"), 3},
                       {QStringLiteral("timezone"), QStringLiteral("Test/City")},
                       {QStringLiteral("utcOffsetSeconds"), 28800},
                       {QStringLiteral("forecast"), days}};
        QVERIFY(ControlPanel::PanelWeather::validObservation(observation, now));
        QFile cache(QDir(qEnvironmentVariable("KDECONNECT_PANEL_CONFIG_DIR")).filePath(QStringLiteral("weather.json")));
        QVERIFY(cache.open(QIODevice::WriteOnly));
        const auto bytes = QJsonDocument(observation).toJson(QJsonDocument::Compact);
        QCOMPARE(cache.write(bytes), bytes.size());
    }
    void reloadedAndSecondPeersReceiveCachedWeatherAfterTheirConfiguration()
    {
        auto &config = KdeConnectConfig::instance();
        const auto firstId = QUuid::createUuid().toString(QUuid::Id128);
        const auto secondId = QUuid::createUuid().toString(QUuid::Id128);
        config.addTrustedDevice(DeviceInfo(firstId, QSslCertificate(), QStringLiteral("Synthetic first peer"), DeviceType::Phone));
        config.addTrustedDevice(DeviceInfo(secondId, QSslCertificate(), QStringLiteral("Synthetic second peer"), DeviceType::Phone));
        WeatherPacketDevice first(firstId), second(secondId);
        QVERIFY(first.isPaired());
        QVERIFY(second.isPaired());
        auto firstPlugin = create(first);
        QVERIFY(firstPlugin);
        QVERIFY(editor->setIdleOptions({{QStringLiteral("enabled"), true}, {QStringLiteral("city"), observation.value(QStringLiteral("city")).toString()}}));
        QTRY_COMPARE(first.revision, editor->config().value(QStringLiteral("revision")).toLongLong());
        QCOMPARE(first.acceptedWeather.value(QStringLiteral("temperature")).toDouble(), 17.25);
        auto secondPlugin = create(second);
        QVERIFY(secondPlugin);
        QCOMPARE(second.acceptedWeather.value(QStringLiteral("temperature")).toDouble(), 17.25);
        // The weather singleton survives a reconnect, but the first peer does not.
        firstPlugin.reset();
        first.session = QUuid::createUuid().toString(QUuid::WithoutBraces);
        firstPlugin = create(first);
        QVERIFY(firstPlugin);
        QCOMPARE(first.acceptedWeather.value(QStringLiteral("temperature")).toDouble(), 17.25);
        QVERIFY(editor->setIdleOptions({{QStringLiteral("enabled"), false}, {QStringLiteral("city"), QString()}}));
        QTRY_COMPARE(first.city, QString());
        QTRY_COMPARE(second.city, QString());
        first.packets = {};
        second.packets = {};
        QVERIFY(editor->setIdleOptions({{QStringLiteral("enabled"), true}, {QStringLiteral("city"), observation.value(QStringLiteral("city")).toString()}}));
        const auto revision = editor->config().value(QStringLiteral("revision")).toLongLong();
        QTRY_COMPARE(first.revision, revision);
        QTRY_COMPARE(second.revision, revision);
        auto trace = packetTrace(first);
        trace.append('\n');
        trace.append(packetTrace(second));
        QVERIFY2(first.acceptedWeather.value(QStringLiteral("revision")).toInteger(-1) == revision
                     && second.acceptedWeather.value(QStringLiteral("revision")).toInteger(-1) == revision,
                 trace.constData());
        QCOMPARE(first.acceptedWeather.value(QStringLiteral("temperature")).toDouble(), 17.25);
        QCOMPARE(second.acceptedWeather.value(QStringLiteral("temperature")).toDouble(), 17.25);
        for (int index = 0; index < 3; ++index) {
            QVERIFY(editor->setIdle(QStringLiteral("message"), QStringLiteral("Presentation change %1").arg(index)));
            const auto changedRevision = editor->config().value(QStringLiteral("revision")).toLongLong();
            QTRY_COMPARE(first.revision, changedRevision);
            QTRY_COMPARE(second.revision, changedRevision);
            QCOMPARE(first.acceptedWeather.value(QStringLiteral("revision")).toInteger(), changedRevision);
            QCOMPARE(second.acceptedWeather.value(QStringLiteral("revision")).toInteger(), changedRevision);
            QCOMPARE(first.acceptedWeather.value(QStringLiteral("fetchedAt")), observation.value(QStringLiteral("fetchedAt")));
            QCOMPARE(second.acceptedWeather.value(QStringLiteral("fetchedAt")), observation.value(QStringLiteral("fetchedAt")));
        }
        first.reachable = false;
        first.packets = {};
        QVERIFY(editor->setIdle(QStringLiteral("message"), QStringLiteral("Unreachable peer")));
        QTRY_COMPARE(second.revision, editor->config().value(QStringLiteral("revision")).toLongLong());
        firstPlugin->connected();
        QVERIFY(first.packets.isEmpty());
        first.reachable = true;
        firstPlugin->connected();
        QCOMPARE(first.acceptedWeather.value(QStringLiteral("revision")).toInteger(), second.revision);
        first.packets = {};
        NetworkPacket invalid(QStringLiteral("kdeconnect.controlpanel.request"),
                              {{QStringLiteral("protocolVersion"), 1}, {QStringLiteral("operation"), QStringLiteral("config")}});
        firstPlugin->receivePacket(invalid);
        QCOMPARE(first.packets.size(), 1);
        QCOMPARE(first.packets[0].toObject().value(QStringLiteral("type")).toString(), QStringLiteral("kdeconnect.controlpanel.result"));
        first.unpair();
        first.packets = {};
        firstPlugin->connected();
        firstPlugin->receivePacket(invalid);
        QVERIFY(editor->setIdle(QStringLiteral("message"), QStringLiteral("Unpaired peer")));
        QTRY_COMPARE(second.revision, editor->config().value(QStringLiteral("revision")).toLongLong());
        QVERIFY(first.packets.isEmpty());
    }
};
QTEST_GUILESS_MAIN(PanelWeatherPluginTest)
#include "panelweatherplugintest.moc"
