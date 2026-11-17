/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <cstring>
#include <panelprogress.h>

using namespace ControlPanel;

class ProgressReply : public QNetworkReply
{
public:
    ProgressReply(const QNetworkRequest &request, const QByteArray &body, int status, bool *aborted, QObject *parent)
        : QNetworkReply(parent)
        , m_body(body)
        , m_aborted(aborted)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status == 302)
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl(QStringLiteral("https://other.example/")));
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] {
            if (isFinished())
                return;
            Q_EMIT readyRead();
            if (!isFinished()) {
                setFinished(true);
                Q_EMIT finished();
            }
        });
    }
    void abort() override
    {
        if (!isFinished()) {
            *m_aborted = true;
            setError(OperationCanceledError, QStringLiteral("aborted"));
            setFinished(true);
            Q_EMIT finished();
        }
    }
    qint64 bytesAvailable() const override
    {
        return m_body.size() - m_offset + QNetworkReply::bytesAvailable();
    }
    bool isSequential() const override
    {
        return true;
    }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const auto count = std::min<qint64>(maximum, m_body.size() - m_offset);
        if (count <= 0)
            return -1;
        std::memcpy(data, m_body.constData() + m_offset, count);
        m_offset += count;
        return count;
    }

private:
    QByteArray m_body;
    qint64 m_offset = 0;
    bool *m_aborted;
};

class ProgressNetwork : public QNetworkAccessManager
{
public:
    QByteArray body;
    int status = 200;
    int requests = 0;
    bool aborted = false;
    QNetworkRequest lastRequest;

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        ++requests;
        lastRequest = request;
        return new ProgressReply(request, body, status, &aborted, this);
    }
};

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
    void reconfigurationPublishesNewerWaitingState()
    {
        ProgressNetwork network;
        network.body = bytes(sample());
        PanelProgress progress(nullptr, &network, true);
        const QJsonObject config{{QStringLiteral("source"), QStringLiteral("url")},
                                 {QStringLiteral("url"), QStringLiteral("https://example.test/progress")},
                                 {QStringLiteral("showProgress"), true},
                                 {QStringLiteral("intervalMs"), 60000}};
        progress.configure(config);
        QTRY_COMPARE_WITH_TIMEOUT(progress.state().value(QStringLiteral("status")).toString(), QStringLiteral("running"), 2000);
        const auto previous = progress.state().value(QStringLiteral("sequence")).toInteger();
        progress.configure(config);
        QCOMPARE(progress.state().value(QStringLiteral("detail")).toString(), QStringLiteral("waiting"));
        const auto waiting = progress.state().value(QStringLiteral("sequence")).toInteger();
        QVERIFY(waiting > previous);
        QTRY_COMPARE_WITH_TIMEOUT(progress.state().value(QStringLiteral("status")).toString(), QStringLiteral("running"), 2000);
        QVERIFY(progress.state().value(QStringLiteral("sequence")).toInteger() > waiting);
    }

    void networkResponsesAreBoundedAndRedirectsRejected()
    {
        for (int scenario = 0; scenario < 4; ++scenario) {
            ProgressNetwork network;
            network.body = scenario == 2 ? QByteArray(65537, 'x') : bytes(sample());
            network.status = scenario == 1 ? 302 : scenario == 3 ? 404 : 200;
            PanelProgress progress(nullptr, &network, true);
            progress.configure(QJsonObject{{QStringLiteral("source"), QStringLiteral("url")},
                                           {QStringLiteral("url"), QStringLiteral("https://example.test/progress")},
                                           {QStringLiteral("showProgress"), true},
                                           {QStringLiteral("intervalMs"), 60000}});
            if (scenario == 0) {
                QTRY_COMPARE_WITH_TIMEOUT(progress.state().value(QStringLiteral("percent")).toInt(-1), 38, 2000);
            } else {
                QTRY_COMPARE_WITH_TIMEOUT(progress.state().value(QStringLiteral("detail")).toString(), QStringLiteral("request_failed"), 2000);
            }
            QCOMPARE(network.requests, 1);
            QCOMPARE(network.lastRequest.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::ManualRedirectPolicy));
            QCOMPARE(network.aborted, scenario == 2);
        }
    }

    void fileWatchRecoversAfterCreationAndAtomicReplacement()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("progress.json"));
        PanelProgress progress(nullptr, nullptr, true);
        progress.configure(QJsonObject{{QStringLiteral("source"), QStringLiteral("file")},
                                       {QStringLiteral("path"), path},
                                       {QStringLiteral("showProgress"), true},
                                       {QStringLiteral("intervalMs"), 60000}});
        QCOMPARE(progress.state().value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        for (int percent : {10, 20, 30}) {
            QJsonObject object = sample();
            object.insert(QStringLiteral("percent"), percent);
            QSaveFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QVERIFY(file.write(bytes(object)) > 0);
            QVERIFY(file.commit());
            QTRY_COMPARE_WITH_TIMEOUT(progress.state().value(QStringLiteral("percent")).toInt(-1), percent, 5000);
        }
    }

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

        object.insert(QStringLiteral("percent"), QJsonValue::Null);
        const QJsonObject nullPercent = PanelProgress::normalize(bytes(object), now, now - 7);
        QVERIFY(!nullPercent.isEmpty());
        QVERIFY(!nullPercent.contains(QStringLiteral("percent")));
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
