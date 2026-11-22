/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <panelsysinfo.h>

using namespace ControlPanel;

class PanelSysInfoTest : public QObject
{
    Q_OBJECT
private:
    static constexpr qint64 now = 1763200005LL;

    static QJsonObject raw()
    {
        return QJsonObject{{QStringLiteral("cpuUsage"), 23.456},
                           {QStringLiteral("cores"), 8},
                           {QStringLiteral("memUsed"), 8.0 * 1024 * 1024 * 1024},
                           {QStringLiteral("memTotal"), 16.0 * 1024 * 1024 * 1024},
                           {QStringLiteral("diskUsed"), 200.0 * 1024 * 1024 * 1024},
                           {QStringLiteral("diskTotal"), 500.0 * 1024 * 1024 * 1024},
                           {QStringLiteral("rxPerSec"), 12800},
                           {QStringLiteral("txPerSec"), 3200},
                           {QStringLiteral("uptimeSeconds"), 345600},
                           {QStringLiteral("processCount"), 214}};
    }

private Q_SLOTS:
    void sharedFixtureIsWireCompatible()
    {
        QFile file(QFINDTESTDATA("../docs/control-panel/fixtures/sysinfo-snapshot.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        QVERIFY(document.isObject());
        const QJsonObject body = document.object();
        QCOMPARE(body.value(QStringLiteral("protocolVersion")).toInt(), 1);
        QCOMPARE(body.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
        QVERIFY(body.value(QStringLiteral("cpu")).toObject().contains(QStringLiteral("usage")));
        QVERIFY(body.value(QStringLiteral("memory")).toObject().contains(QStringLiteral("usage")));
        const QByteArray wire = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QVERIFY(!wire.contains(QByteArrayLiteral("C:\\")));
        QVERIFY(!wire.contains(QByteArrayLiteral("hostname")));
    }

    void validSettingsAreChecked()
    {
        QCOMPARE(PanelSysInfo::DefaultIntervalMs, 5000);
        QVERIFY(PanelSysInfo::validSettings(QJsonObject{{QStringLiteral("enabled"), true}}));
        QVERIFY(PanelSysInfo::validSettings(QJsonObject{{QStringLiteral("enabled"), false},
                                                        {QStringLiteral("intervalMs"), 2000}}));
        QVERIFY(!PanelSysInfo::validSettings(QJsonObject{{QStringLiteral("enabled"), QStringLiteral("yes")}}));
        QVERIFY(!PanelSysInfo::validSettings(QJsonObject{{QStringLiteral("enabled"), true},
                                                         {QStringLiteral("intervalMs"), 500}}));
        QVERIFY(!PanelSysInfo::validSettings(QJsonObject{{QStringLiteral("enabled"), true},
                                                         {QStringLiteral("intervalMs"), 60000}}));
    }

    void normalizeRoundsAndClamps()
    {
        const QJsonObject result = PanelSysInfo::normalize(raw(), now);
        QCOMPARE(result.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
        QCOMPARE(result.value(QStringLiteral("fetchedAt")).toInteger(), now);
        QCOMPARE(result.value(QStringLiteral("cores")).toInt(), 8);
        QCOMPARE(result.value(QStringLiteral("cpu")).toObject().value(QStringLiteral("usage")).toDouble(), 23.5);
        QCOMPARE(result.value(QStringLiteral("memory")).toObject().value(QStringLiteral("usage")).toDouble(), 50.0);
        QCOMPARE(result.value(QStringLiteral("disk")).toObject().value(QStringLiteral("usage")).toDouble(), 40.0);
        QCOMPARE(result.value(QStringLiteral("network")).toObject().value(QStringLiteral("rxBytesPerSec")).toInteger(), 12800LL);
        QCOMPARE(result.value(QStringLiteral("uptimeSeconds")).toInteger(), 345600LL);
        QCOMPARE(result.value(QStringLiteral("processCount")).toInt(), 214);
        // No machine-identifying fields may be added to the wire snapshot.
        const QByteArray wire = QJsonDocument(result).toJson(QJsonDocument::Compact);
        QVERIFY(!wire.contains(QByteArrayLiteral("path")));
        QVERIFY(!wire.contains(QByteArrayLiteral("machine")));
    }

    void invalidFieldsDropOnlyTheirSection()
    {
        QJsonObject bad = raw();
        bad.insert(QStringLiteral("memUsed"), bad.value(QStringLiteral("memTotal")).toDouble() + 1);
        bad.insert(QStringLiteral("cpuUsage"), 150);
        bad.insert(QStringLiteral("uptimeSeconds"), -5);
        bad.insert(QStringLiteral("processCount"), 200000);
        const QJsonObject result = PanelSysInfo::normalize(bad, now);
        QVERIFY(!result.contains(QStringLiteral("cpu")));
        QVERIFY(!result.contains(QStringLiteral("memory")));
        QVERIFY(!result.contains(QStringLiteral("uptimeSeconds")));
        QVERIFY(!result.contains(QStringLiteral("processCount")));
        // Disk and network are still valid and must survive.
        QVERIFY(result.contains(QStringLiteral("disk")));
        QVERIFY(result.contains(QStringLiteral("network")));
        QCOMPARE(result.value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
    }

    void unavailableHasAllWireFields()
    {
        const QJsonObject result = PanelSysInfo::unavailable(now, QStringLiteral("sampling_unavailable"));
        QCOMPARE(result.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
        QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("sampling_unavailable"));
        QCOMPARE(result.value(QStringLiteral("fetchedAt")).toInteger(), now);
        for (const QString &key : {QStringLiteral("cpu"), QStringLiteral("memory"), QStringLiteral("disk"), QStringLiteral("network")})
            QVERIFY(result.contains(key));
    }

    void disabledStateIsPublishable()
    {
        PanelSysInfo info;
        info.configure(QJsonObject{{QStringLiteral("enabled"), false}});
        QCOMPARE(info.state().value(QStringLiteral("status")).toString(), QStringLiteral("disabled"));
        QVERIFY(!info.state().contains(QStringLiteral("cpuUsage")));
    }
};

QTEST_GUILESS_MAIN(PanelSysInfoTest)
#include "panelsysinfotest.moc"
