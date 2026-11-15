/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <panelprogress.h>

using namespace ControlPanel;

class PanelProgressTest : public QObject
{
    Q_OBJECT
private:
    static constexpr qint64 now = 1763200000LL;

    static QJsonObject sample()
    {
        return QJsonObject{{QStringLiteral("status"), QStringLiteral("running")},
                           {QStringLiteral("title"), QStringLiteral("整理季度报告")},
                           {QStringLiteral("stage"), QStringLiteral("生成图表")},
                           {QStringLiteral("detail"), QStringLiteral("已完成 3/8 个文件")},
                           {QStringLiteral("percent"), 38},
                           {QStringLiteral("updatedAt"), now - 5}};
    }

    static QByteArray bytes(const QJsonObject &object)
    {
        return QJsonDocument(object).toJson(QJsonDocument::Compact);
    }

private Q_SLOTS:
    void validPayloadIsNormalized()
    {
        const QJsonObject result = PanelProgress::normalize(bytes(sample()), now, now - 7);
        QVERIFY(!result.isEmpty());
        QCOMPARE(result.value(QStringLiteral("source")).toString(), QStringLiteral("workbuddy"));
        QCOMPARE(result.value(QStringLiteral("status")).toString(), QStringLiteral("running"));
        QCOMPARE(result.value(QStringLiteral("stage")).toString(), QStringLiteral("生成图表"));
        QCOMPARE(result.value(QStringLiteral("percent")).toInt(), 38);
        QCOMPARE(result.value(QStringLiteral("updatedAt")).toInteger(), now - 5);
        QCOMPARE(result.value(QStringLiteral("fetchedAt")).toInteger(), now);
        QVERIFY(!result.contains(QStringLiteral("sequence"))); // assigned by the publisher, not by the source
    }

    void optionalFieldsAreOptional()
    {
        QJsonObject object = sample();
        object.remove(QStringLiteral("percent"));
        object.remove(QStringLiteral("updatedAt"));
        object.insert(QStringLiteral("title"), QString());
        object.insert(QStringLiteral("stage"), QString());
        object.insert(QStringLiteral("detail"), QString());
        const QJsonObject result = PanelProgress::normalize(bytes(object), now, now - 7);
        QVERIFY(!result.isEmpty());
        QVERIFY(!result.contains(QStringLiteral("percent")));
        QCOMPARE(result.value(QStringLiteral("updatedAt")).toInteger(), now - 7); // file mtime fallback
        QCOMPARE(result.value(QStringLiteral("title")).toString(), QString());
    }

    void invalidPayloadIsRejected_data()
    {
        QTest::addColumn<QByteArray>("payload");
        QJsonObject object = sample();
        object.insert(QStringLiteral("status"), QStringLiteral("flying"));
        QTest::newRow("unknown status") << bytes(object);

        object = sample();
        object.insert(QStringLiteral("percent"), 101);
        QTest::newRow("percent above range") << bytes(object);

        object = sample();
        object.insert(QStringLiteral("percent"), QStringLiteral("38"));
        QTest::newRow("coerced percent") << bytes(object);

        object = sample();
        object.insert(QStringLiteral("title"), QStringLiteral("line\u0001break"));
        QTest::newRow("control character") << bytes(object);

        object = sample();
        object.insert(QStringLiteral("detail"), QString(241, QLatin1Char('x')));
        QTest::newRow("detail too long") << bytes(object);

        object = sample();
        object.insert(QStringLiteral("updatedAt"), now + 3600);
        QTest::newRow("future timestamp") << bytes(object);

        object = sample();
        object.insert(QStringLiteral("updatedAt"), QStringLiteral("not a number"));
        QTest::newRow("string timestamp") << bytes(object);

        QTest::newRow("not json") << QByteArrayLiteral("{oops");
        QTest::newRow("array root") << QByteArrayLiteral("[]");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("too large") << QByteArray(65537, 'x');
    }

    void invalidPayloadIsRejected()
    {
        QFETCH(QByteArray, payload);
        QVERIFY(PanelProgress::normalize(payload, now, now - 7).isEmpty());
    }

    void unavailableSnapshotHasAllWireFields()
    {
        const QJsonObject result = PanelProgress::unavailable(now, QStringLiteral("source_missing"));
        QCOMPARE(result.value(QStringLiteral("source")).toString(), QStringLiteral("workbuddy"));
        QCOMPARE(result.value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(result.value(QStringLiteral("detail")).toString(), QStringLiteral("source_missing"));
        QCOMPARE(result.value(QStringLiteral("sequence")).toInteger(), 0LL);
        QCOMPARE(result.value(QStringLiteral("updatedAt")).toInteger(), now);
        QCOMPARE(result.value(QStringLiteral("fetchedAt")).toInteger(), now);
    }
};

QTEST_GUILESS_MAIN(PanelProgressTest)
#include "panelprogresstest.moc"
