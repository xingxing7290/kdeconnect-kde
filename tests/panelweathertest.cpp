/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <panelweather.h>

struct WeatherResponse {
    QByteArray bytes;
    int status = 200;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
};
class WeatherReply : public QNetworkReply
{
public:
    WeatherReply(const QNetworkRequest &request, WeatherResponse response, QObject *parent)
        : QNetworkReply(parent)
        , m_response(std::move(response))
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, m_response.status);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] {
            if (isFinished())
                return;
            if (m_response.error != NoError)
                setError(m_response.error, QStringLiteral("fixture error"));
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
            setError(OperationCanceledError, QStringLiteral("aborted"));
            setFinished(true);
            Q_EMIT finished();
        }
    }
    qint64 bytesAvailable() const override
    {
        return m_response.bytes.size() - m_offset + QNetworkReply::bytesAvailable();
    }
    bool isSequential() const override
    {
        return true;
    }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const auto count = std::min<qint64>(maximum, m_response.bytes.size() - m_offset);
        if (count <= 0)
            return -1;
        memcpy(data, m_response.bytes.constData() + m_offset, count);
        m_offset += count;
        return count;
    }

private:
    WeatherResponse m_response;
    qint64 m_offset = 0;
};
class WeatherNetwork : public QNetworkAccessManager
{
public:
    QList<WeatherResponse> responses;
    QList<QUrl> requests;

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        requests.append(request.url());
        return new WeatherReply(request, responses.isEmpty() ? WeatherResponse{"{}", 404} : responses.takeFirst(), this);
    }
};
class PanelWeatherTest : public QObject
{
    Q_OBJECT
    static QJsonObject idle(const QString &city)
    {
        return {{QStringLiteral("enabled"), true}, {QStringLiteral("city"), city}};
    }
    static WeatherResponse location(const QString &name = QStringLiteral("长沙"))
    {
        return {QJsonDocument(QJsonObject{{QStringLiteral("results"),
                                           QJsonArray{QJsonObject{{QStringLiteral("name"), name},
                                                                  {QStringLiteral("country"), QStringLiteral("中国")},
                                                                  {QStringLiteral("latitude"), 28.2},
                                                                  {QStringLiteral("longitude"), 112.9}}}}})
                    .toJson()};
    }
    static QJsonObject forecast()
    {
        return {{QStringLiteral("current_units"),
                 QJsonObject{{QStringLiteral("temperature_2m"), QStringLiteral("°C")}, {QStringLiteral("time"), QStringLiteral("unixtime")}}},
                {QStringLiteral("current"),
                 QJsonObject{{QStringLiteral("time"), QDateTime::currentSecsSinceEpoch() - 300},
                             {QStringLiteral("temperature_2m"), 18.5},
                             {QStringLiteral("weather_code"), 3}}}};
    }
private Q_SLOTS:
    void networkingMustBeExplicitAndEmptyCityNeverQueries()
    {
        QTemporaryDir directory;
        WeatherNetwork network;
        ControlPanel::PanelWeather weather(directory.path(), nullptr, &network);
        weather.configure(idle(QStringLiteral("长沙市")));
        QCOMPARE(weather.state().value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QVERIFY(network.requests.isEmpty());
        weather.configure(idle(QString()));
        QCOMPARE(weather.state().value(QStringLiteral("status")).toString(), QStringLiteral("no_city"));
        QVERIFY(network.requests.isEmpty());
    }
    void queryNormalizesCityAndPersistsOnlyValidatedObservation()
    {
        QTemporaryDir directory;
        WeatherNetwork network;
        network.responses = {location(), {QJsonDocument(forecast()).toJson()}};
        ControlPanel::PanelWeather weather(directory.path(), nullptr, &network, true);
        weather.configure(idle(QStringLiteral("  长沙市  ")));
        QTRY_COMPARE(weather.state().value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
        QCOMPARE(network.requests.size(), 2);
        QCOMPARE(QUrlQuery(network.requests[0]).queryItemValue(QStringLiteral("name")), QStringLiteral("长沙市"));
        QCOMPARE(network.requests[0].scheme(), QStringLiteral("https"));
        QCOMPARE(network.requests[1].host(), QStringLiteral("api.open-meteo.com"));
        QCOMPARE(QUrlQuery(network.requests[1]).queryItemValue(QStringLiteral("timeformat")), QStringLiteral("unixtime"));
        QCOMPARE(weather.state().value(QStringLiteral("temperature")).toDouble(), 18.5);
        QCOMPARE(weather.state().value(QStringLiteral("resolvedCity")).toString(), QStringLiteral("长沙 / 中国"));
        const auto original = weather.state();
        ControlPanel::PanelWeather restarted(directory.path(), nullptr, &network, true);
        restarted.configure(idle(QStringLiteral("长沙市")));
        QCOMPARE(restarted.state(), original);
        QCOMPARE(network.requests.size(), 2);
        network.responses = {{"{}", 503}};
        restarted.refresh();
        QTRY_COMPARE(restarted.state().value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QCOMPARE(restarted.state().value(QStringLiteral("fetchedAt")), original.value(QStringLiteral("fetchedAt")));
        QCOMPARE(restarted.state().value(QStringLiteral("temperature")), original.value(QStringLiteral("temperature")));
        auto old = forecast();
        auto oldCurrent = old.value(QStringLiteral("current")).toObject();
        oldCurrent.insert(QStringLiteral("time"), QDateTime::currentSecsSinceEpoch() - 900);
        old.insert(QStringLiteral("current"), oldCurrent);
        network.responses = {location(), {QJsonDocument(old).toJson()}};
        restarted.refresh();
        QTRY_COMPARE(restarted.state().value(QStringLiteral("error")).toString(), QStringLiteral("older_weather"));
        QCOMPARE(restarted.state().value(QStringLiteral("observedAt")), original.value(QStringLiteral("observedAt")));
    }
    void chineseCitySuffixCanFallbackWithoutChangingConfiguredIdentity()
    {
        QTemporaryDir directory;
        WeatherNetwork network;
        network.responses = {{"{}"}, location(), {QJsonDocument(forecast()).toJson()}};
        ControlPanel::PanelWeather weather(directory.path(), nullptr, &network, true);
        weather.configure(idle(QStringLiteral("长沙市")));
        QTRY_COMPARE(weather.state().value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
        QCOMPARE(network.requests.size(), 3);
        QCOMPARE(QUrlQuery(network.requests[1]).queryItemValue(QStringLiteral("name")), QStringLiteral("长沙"));
        QCOMPARE(weather.state().value(QStringLiteral("city")).toString(), QStringLiteral("长沙市"));
    }
    void cityChangesCancelOldResponseAndClearPreviousData()
    {
        QTemporaryDir directory;
        WeatherNetwork network;
        network.responses = {location(QStringLiteral("Old")), location(QStringLiteral("New")), {QJsonDocument(forecast()).toJson()}};
        ControlPanel::PanelWeather weather(directory.path(), nullptr, &network, true);
        weather.configure(idle(QStringLiteral("Old")));
        weather.configure(idle(QStringLiteral("New")));
        QTRY_COMPARE(weather.state().value(QStringLiteral("status")).toString(), QStringLiteral("ready"));
        QCOMPARE(weather.state().value(QStringLiteral("city")).toString(), QStringLiteral("New"));
        QCOMPARE(weather.state().value(QStringLiteral("resolvedCity")).toString(), QStringLiteral("New / 中国"));
        network.responses = {{"{}", 500}};
        weather.configure(idle(QStringLiteral("Unknown")));
        QVERIFY(!weather.state().contains(QStringLiteral("temperature")));
        QTRY_COMPARE(weather.state().value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QVERIFY(!weather.state().contains(QStringLiteral("temperature")));
    }
    void invalidForecast_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        auto object = forecast();
        auto current = object.value(QStringLiteral("current")).toObject();
        current.insert(QStringLiteral("temperature_2m"), QStringLiteral("18.5"));
        object.insert(QStringLiteral("current"), current);
        QTest::newRow("string-temperature") << QJsonDocument(object).toJson();
        object = forecast();
        current = object.value(QStringLiteral("current")).toObject();
        current.insert(QStringLiteral("weather_code"), 99.5);
        object.insert(QStringLiteral("current"), current);
        QTest::newRow("fractional-code") << QJsonDocument(object).toJson();
        current.insert(QStringLiteral("weather_code"), 4);
        object.insert(QStringLiteral("current"), current);
        QTest::newRow("unknown-code") << QJsonDocument(object).toJson();
        object = forecast();
        current = object.value(QStringLiteral("current")).toObject();
        current.insert(QStringLiteral("time"), QDateTime::currentSecsSinceEpoch() - 14400);
        object.insert(QStringLiteral("current"), current);
        QTest::newRow("old-time") << QJsonDocument(object).toJson();
        object = forecast();
        object.insert(QStringLiteral("current_units"), QJsonObject{{QStringLiteral("temperature_2m"), QStringLiteral("°F")}});
        QTest::newRow("wrong-units") << QJsonDocument(object).toJson();
        QTest::newRow("oversized") << QByteArray(70000, 'a');
        QTest::newRow("not-object") << QByteArray("[]");
    }
    void invalidForecast()
    {
        QFETCH(QByteArray, bytes);
        QTemporaryDir directory;
        WeatherNetwork network;
        network.responses = {location(), {bytes}};
        ControlPanel::PanelWeather weather(directory.path(), nullptr, &network, true);
        weather.configure(idle(QStringLiteral("长沙市")));
        QTRY_COMPARE(weather.state().value(QStringLiteral("status")).toString(), QStringLiteral("unavailable"));
        QVERIFY(!weather.state().contains(QStringLiteral("temperature")));
        QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("weather.json"))));
    }
};
QTEST_GUILESS_MAIN(PanelWeatherTest)
#include "panelweathertest.moc"
