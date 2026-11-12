/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelweather.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QSharedPointer>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ControlPanel
{
namespace
{
constexpr qsizetype MaxResponse = 65536;
bool text(const QJsonValue &value, int maximum, bool empty = false)
{
    if (!value.isString() || value.toString().size() > maximum || (!empty && value.toString().trimmed().isEmpty()))
        return false;
    for (const auto character : value.toString())
        if (character.category() == QChar::Other_Control)
            return false;
    return true;
}
bool number(const QJsonValue &value, double minimum, double maximum, bool integer = false)
{
    const auto result = value.toDouble(std::numeric_limits<double>::quiet_NaN());
    return value.isDouble() && std::isfinite(result) && result >= minimum && result <= maximum && (!integer || result == std::floor(result));
}
}
PanelWeather::PanelWeather(const QString &directory, QObject *parent, QNetworkAccessManager *network, bool networking)
    : QObject(parent)
    , m_path(QDir(directory).filePath(QStringLiteral("weather.json")))
    , m_network(network ? network : new QNetworkAccessManager(this))
    , m_networking(networking)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &PanelWeather::refresh);
}
PanelWeather::~PanelWeather()
{
    ++m_generation;
    if (m_reply)
        m_reply->abort();
}
QJsonObject PanelWeather::state() const
{
    return m_state;
}
bool PanelWeather::validCode(int code)
{
    static const QSet<int> values{0, 1, 2, 3, 45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 99};
    return values.contains(code);
}
bool PanelWeather::validObservation(const QJsonObject &object, qint64 now, qint64 maximumAge)
{
    return object.value(QStringLiteral("source")).toString() == QLatin1String("open-meteo") && text(object.value(QStringLiteral("resolvedCity")), 260)
        && number(object.value(QStringLiteral("observedAt")), now - maximumAge, now + 600, true)
        && number(object.value(QStringLiteral("fetchedAt")), now - maximumAge, now + 600, true)
        && object.value(QStringLiteral("observedAt")).toInteger() <= object.value(QStringLiteral("fetchedAt")).toInteger() + 600
        && number(object.value(QStringLiteral("temperature")), -100, 70) && number(object.value(QStringLiteral("weatherCode")), 0, 99, true)
        && validCode(object.value(QStringLiteral("weatherCode")).toInt());
}
QJsonObject PanelWeather::cached() const
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16384)
        return {};
    const auto object = QJsonDocument::fromJson(file.read(16385)).object();
    if (object.value(QStringLiteral("city")).toString() != m_city || object.value(QStringLiteral("status")).toString() != QLatin1String("ready")
        || !validObservation(object, QDateTime::currentSecsSinceEpoch(), 86400))
        return {};
    QJsonObject result;
    for (const auto &key : {"city", "resolvedCity", "source", "observedAt", "fetchedAt", "temperature", "weatherCode", "status"})
        result.insert(QLatin1String(key), object.value(QLatin1String(key)));
    return result;
}
void PanelWeather::configure(const QJsonObject &idle)
{
    const auto city = idle.value(QStringLiteral("city")).toString().trimmed();
    const bool enabled = idle.value(QStringLiteral("enabled")).toBool() && text(city, 120);
    if (m_enabled == enabled && m_city == city && !m_state.isEmpty())
        return;
    m_enabled = enabled;
    m_city = city;
    ++m_generation;
    m_timer.stop();
    if (m_reply)
        m_reply->abort();
    m_reply.clear();
    m_state = {{QStringLiteral("city"), m_city},
               {QStringLiteral("source"), QStringLiteral("open-meteo")},
               {QStringLiteral("status"),
                enabled              ? QStringLiteral("loading")
                    : city.isEmpty() ? QStringLiteral("no_city")
                                     : QStringLiteral("disabled")}};
    if (enabled) {
        const auto stored = cached();
        if (!stored.isEmpty())
            m_state = stored;
        const auto now = QDateTime::currentSecsSinceEpoch();
        if (!stored.isEmpty() && validObservation(stored, now) && now - stored.value(QStringLiteral("fetchedAt")).toInteger() < 900) {
            if (m_networking)
                m_timer.start(static_cast<int>(std::clamp<qint64>(900 - now + stored.value(QStringLiteral("fetchedAt")).toInteger(), 1, 900) * 1000));
            Q_EMIT changed();
            return;
        }
    }
    Q_EMIT changed();
    if (enabled)
        refresh();
}
void PanelWeather::refresh()
{
    if (!m_enabled || m_reply)
        return;
    if (!m_networking) {
        unavailable(QStringLiteral("networking_disabled"));
        return;
    }
    m_state.insert(QStringLiteral("status"), QStringLiteral("loading"));
    Q_EMIT changed();
    locate(m_city, true);
}
void PanelWeather::locate(const QString &name, bool fallback)
{
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("name"), name);
    query.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("language"), QStringLiteral("zh"));
    url.setQuery(query);
    request(url, [this, name, fallback](const QJsonObject &object) {
        const auto results = object.value(QStringLiteral("results")).toArray();
        if (results.isEmpty() && fallback && name.endsWith(QStringLiteral("市")) && name.size() > 2) {
            locate(name.chopped(1), false);
            return;
        }
        const auto place = results.isEmpty() ? QJsonObject() : results.first().toObject();
        if (!number(place.value(QStringLiteral("latitude")), -90, 90) || !number(place.value(QStringLiteral("longitude")), -180, 180)
            || !text(place.value(QStringLiteral("name")), 120) || !text(place.value(QStringLiteral("country")), 120)) {
            unavailable(QStringLiteral("city_not_found"));
            return;
        }
        const QString resolved = place.value(QStringLiteral("name")).toString() + QStringLiteral(" / ") + place.value(QStringLiteral("country")).toString();
        QUrl forecast(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
        QUrlQuery parameters;
        parameters.addQueryItem(QStringLiteral("latitude"), QString::number(place.value(QStringLiteral("latitude")).toDouble(), 'f', 6));
        parameters.addQueryItem(QStringLiteral("longitude"), QString::number(place.value(QStringLiteral("longitude")).toDouble(), 'f', 6));
        parameters.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,weather_code"));
        parameters.addQueryItem(QStringLiteral("temperature_unit"), QStringLiteral("celsius"));
        parameters.addQueryItem(QStringLiteral("timeformat"), QStringLiteral("unixtime"));
        parameters.addQueryItem(QStringLiteral("timezone"), QStringLiteral("GMT"));
        parameters.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("1"));
        forecast.setQuery(parameters);
        request(forecast, [this, resolved](const QJsonObject &forecastObject) {
            const auto current = forecastObject.value(QStringLiteral("current")).toObject();
            const auto units = forecastObject.value(QStringLiteral("current_units")).toObject();
            QJsonObject result{{QStringLiteral("city"), m_city},
                               {QStringLiteral("resolvedCity"), resolved},
                               {QStringLiteral("source"), QStringLiteral("open-meteo")},
                               {QStringLiteral("status"), QStringLiteral("ready")},
                               {QStringLiteral("observedAt"), current.value(QStringLiteral("time"))},
                               {QStringLiteral("fetchedAt"), QDateTime::currentSecsSinceEpoch()},
                               {QStringLiteral("temperature"), current.value(QStringLiteral("temperature_2m"))},
                               {QStringLiteral("weatherCode"), current.value(QStringLiteral("weather_code"))}};
            if (units.value(QStringLiteral("temperature_2m")).toString() != QStringLiteral("°C")
                || units.value(QStringLiteral("time")).toString() != QLatin1String("unixtime")
                || !validObservation(result, QDateTime::currentSecsSinceEpoch())) {
                unavailable(QStringLiteral("invalid_weather"));
                return;
            }
            if (validObservation(m_state, QDateTime::currentSecsSinceEpoch(), 86400)
                && result.value(QStringLiteral("observedAt")).toInteger() < m_state.value(QStringLiteral("observedAt")).toInteger()) {
                unavailable(QStringLiteral("older_weather"));
                return;
            }
            complete(result);
        });
    });
}
void PanelWeather::request(const QUrl &url, std::function<void(const QJsonObject &)> success)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(10000);
    request.setRawHeader("User-Agent", "KDEConnect-ControlPanel/1 (personal non-commercial fork)");
    auto *reply = m_network->get(request);
    m_reply = reply;
    reply->setReadBufferSize(MaxResponse + 1);
    const auto generation = m_generation;
    auto bytes = QSharedPointer<QByteArray>::create();
    const auto collect = [reply, bytes] {
        if (bytes->size() > MaxResponse)
            return;
        bytes->append(reply->read(MaxResponse + 1 - bytes->size()));
        if (bytes->size() > MaxResponse)
            reply->abort();
    };
    connect(reply, &QNetworkReply::readyRead, this, collect);
    auto *deadline = new QTimer(reply);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
    deadline->start(15000);
    connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, generation, collect, success = std::move(success)] {
        collect();
        const bool valid = reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && !bytes->isEmpty() && bytes->size() <= MaxResponse;
        reply->deleteLater();
        if (generation != m_generation)
            return;
        m_reply.clear();
        const auto document = valid ? QJsonDocument::fromJson(*bytes) : QJsonDocument();
        if (!document.isObject())
            unavailable(QStringLiteral("weather_unavailable"));
        else
            success(document.object());
    });
}
void PanelWeather::unavailable(const QString &error)
{
    m_state.insert(QStringLiteral("status"), QStringLiteral("unavailable"));
    m_state.insert(QStringLiteral("error"), error);
    if (m_networking && m_enabled)
        m_timer.start(300000);
    Q_EMIT changed();
}
void PanelWeather::complete(QJsonObject data)
{
    m_state = std::move(data);
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    const auto bytes = QJsonDocument(m_state).toJson(QJsonDocument::Compact);
    if (file.open(QIODevice::WriteOnly)) {
        if (file.write(bytes) == bytes.size())
            file.commit();
        else
            file.cancelWriting();
    }
    m_timer.start(900000);
    Q_EMIT changed();
}
}

#include "moc_panelweather.cpp"
