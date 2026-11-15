/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelprogress.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>
#include <cmath>
#include <limits>

namespace ControlPanel
{
namespace
{
constexpr qsizetype MaxPayload = 65536;
constexpr qint64 HeartbeatSeconds = 2;
constexpr qint64 MaxTimestamp = 4102444800LL;

bool text(const QJsonValue &value, qsizetype maximum)
{
    if (!value.isString() || value.toString().size() > maximum) {
        return false;
    }
    for (const QChar character : value.toString()) {
        if (character.category() == QChar::Other_Control) {
            return false;
        }
    }
    return true;
}

bool number(const QJsonValue &value, double minimum, double maximum, bool integral = false)
{
    const double result = value.toDouble(std::numeric_limits<double>::quiet_NaN());
    return value.isDouble() && std::isfinite(result) && result >= minimum && result <= maximum
        && (!integral || result == std::floor(result));
}
}

PanelProgress::PanelProgress(QObject *parent, QNetworkAccessManager *network, bool networking)
    : QObject(parent)
    , m_network(network ? network : new QNetworkAccessManager(this))
    , m_networking(networking)
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &PanelProgress::refresh);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &PanelProgress::refresh);
    // The heartbeat is independent of the (user configurable) source poll interval so a slow
    // source cannot accidentally make the phone declare a healthy link stale.
    m_heartbeat.setSingleShot(false);
    m_heartbeat.setInterval(static_cast<int>(HeartbeatSeconds * 1000));
    connect(&m_heartbeat, &QTimer::timeout, this, &PanelProgress::heartbeat);
}

PanelProgress::~PanelProgress()
{
    if (m_reply) {
        m_reply->abort();
    }
}

QJsonObject PanelProgress::state() const
{
    return m_state;
}

QJsonObject PanelProgress::normalize(const QByteArray &data, qint64 now, qint64 sourceTime)
{
    if (data.isEmpty() || data.size() > MaxPayload) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    const QJsonObject input = document.object();
    static const QSet<QString> statuses{QStringLiteral("running"), QStringLiteral("idle"), QStringLiteral("done"),
                                        QStringLiteral("error"), QStringLiteral("paused"), QStringLiteral("unavailable")};
    const QString status = input.value(QStringLiteral("status")).toString();
    if (!statuses.contains(status)) {
        return {};
    }
    QString title;
    QString stage;
    QString detail;
    for (const auto &field : {qMakePair(QStringLiteral("title"), 120), qMakePair(QStringLiteral("stage"), 120),
                              qMakePair(QStringLiteral("detail"), 240)}) {
        const QJsonValue value = input.value(field.first);
        if (!value.isUndefined() && !value.isNull() && !text(value, field.second)) {
            return {};
        }
    }
    title = input.value(QStringLiteral("title")).toString();
    stage = input.value(QStringLiteral("stage")).toString();
    detail = input.value(QStringLiteral("detail")).toString();

    QJsonValue percent;
    const QJsonValue percentValue = input.value(QStringLiteral("percent"));
    if (!percentValue.isUndefined() && !percentValue.isNull()) {
        if (!number(percentValue, 0, 100, true)) {
            return {};
        }
        percent = QJsonValue(percentValue.toInt());
    }
    qint64 updatedAt = sourceTime > 0 ? sourceTime : now;
    const QJsonValue updated = input.value(QStringLiteral("updatedAt"));
    if (!updated.isUndefined() && !updated.isNull()) {
        if (!number(updated, 0, MaxTimestamp, true) || updated.toInteger() > now + 600) {
            return {};
        }
        updatedAt = updated.toInteger();
    }
    if (updatedAt > now + 600 || updatedAt <= 0) {
        updatedAt = now;
    }
    QJsonObject result{{QStringLiteral("source"), QStringLiteral("workbuddy")},
                       {QStringLiteral("status"), status},
                       {QStringLiteral("title"), title},
                       {QStringLiteral("stage"), stage},
                       {QStringLiteral("detail"), detail},
                       {QStringLiteral("updatedAt"), updatedAt},
                       {QStringLiteral("fetchedAt"), now}};
    if (!percent.isUndefined()) {
        result.insert(QStringLiteral("percent"), percent);
    }
    return result;
}

QJsonObject PanelProgress::unavailable(qint64 now, const QString &error)
{
    return QJsonObject{{QStringLiteral("source"), QStringLiteral("workbuddy")},
                       {QStringLiteral("status"), QStringLiteral("unavailable")},
                       {QStringLiteral("title"), QString()},
                       {QStringLiteral("stage"), QString()},
                       {QStringLiteral("detail"), error.left(240)},
                       {QStringLiteral("sequence"), 0},
                       {QStringLiteral("updatedAt"), now},
                       {QStringLiteral("fetchedAt"), now}};
}

QByteArray PanelProgress::signatureOf(const QJsonObject &snapshot)
{
    QJsonObject content;
    for (const QString &key : {QStringLiteral("status"), QStringLiteral("title"), QStringLiteral("stage"),
                               QStringLiteral("detail"), QStringLiteral("updatedAt")}) {
        content.insert(key, snapshot.value(key));
    }
    if (snapshot.contains(QStringLiteral("percent"))) {
        content.insert(QStringLiteral("percent"), snapshot.value(QStringLiteral("percent")));
    }
    return QJsonDocument(content).toJson(QJsonDocument::Compact);
}

void PanelProgress::configure(const QJsonObject &info)
{
    m_timer.stop();
    m_heartbeat.stop();
    if (!m_watchedPath.isEmpty()) {
        m_watcher.removePath(m_watchedPath);
        m_watchedPath.clear();
    }
    if (m_reply) {
        m_reply->abort();
        m_reply.clear();
    }
    m_source = info.value(QStringLiteral("source")).toString();
    m_path = info.value(QStringLiteral("path")).toString();
    m_url = info.value(QStringLiteral("url")).toString();
    m_intervalMs = qBound(1000, info.value(QStringLiteral("intervalMs")).toInt(2000), 60000);
    const bool usableFile = m_source == QLatin1String("file") && !m_path.isEmpty();
    const bool usableUrl = m_source == QLatin1String("url") && !m_url.isEmpty();
    m_enabled = m_networking && info.value(QStringLiteral("showProgress")).toBool(false) && (usableFile || usableUrl);
    m_signature.clear();
    m_lastPublished = 0;
    m_state = unavailable(QDateTime::currentSecsSinceEpoch(), QStringLiteral("waiting"));
    Q_EMIT changed();
    if (!m_enabled) {
        return;
    }
    if (usableFile) {
        m_watchedPath = m_path;
        if (QFileInfo::exists(m_path)) {
            m_watcher.addPath(m_watchedPath);
        }
    }
    m_timer.setInterval(m_intervalMs);
    m_timer.start();
    m_heartbeat.start();
    refresh();
}

void PanelProgress::refresh()
{
    if (!m_enabled || m_reply) {
        return;
    }
    if (m_source == QLatin1String("file")) {
        readFile();
    } else if (m_source == QLatin1String("url")) {
        requestUrl();
    }
}

void PanelProgress::readFile()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        failProgress(QStringLiteral("source_missing"));
        return;
    }
    if (file.size() > MaxPayload) {
        failProgress(QStringLiteral("payload_too_large"));
        return;
    }
    const QByteArray data = file.read(MaxPayload + 1);
    const qint64 sourceTime = QFileInfo(m_path).lastModified().toSecsSinceEpoch();
    const QJsonObject snapshot = normalize(data, QDateTime::currentSecsSinceEpoch(), sourceTime);
    if (snapshot.isEmpty()) {
        failProgress(QStringLiteral("invalid_payload"));
    } else {
        accept(snapshot, signatureOf(snapshot));
    }
}

void PanelProgress::requestUrl()
{
    const QUrl address(m_url);
    if (!address.isValid() || (address.scheme() != QLatin1String("http") && address.scheme() != QLatin1String("https"))) {
        failProgress(QStringLiteral("invalid_url"));
        return;
    }
    QNetworkRequest request(address);
    request.setTransferTimeout(3000);
    m_reply = m_network->get(request);
    connect(m_reply.data(), &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply.data();
        if (!reply) {
            return;
        }
        const QByteArray data = reply->read(MaxPayload + 1);
        const bool redirected = !reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isNull();
        const bool ok = reply->error() == QNetworkReply::NoError && !redirected && data.size() <= MaxPayload;
        reply->deleteLater();
        m_reply.clear();
        if (!ok) {
            failProgress(QStringLiteral("request_failed"));
            return;
        }
        const QJsonObject snapshot = normalize(data, QDateTime::currentSecsSinceEpoch(), 0);
        if (snapshot.isEmpty()) {
            failProgress(QStringLiteral("invalid_payload"));
        } else {
            accept(snapshot, signatureOf(snapshot));
        }
    });
}

void PanelProgress::failProgress(const QString &error)
{
    const QJsonObject snapshot = unavailable(QDateTime::currentSecsSinceEpoch(), error);
    accept(snapshot, QByteArrayLiteral("error:") + error.toUtf8());
}

/** Re-announce the last validated snapshot without touching the (possibly slow) source. */
void PanelProgress::heartbeat()
{
    if (m_enabled && !m_state.isEmpty()) {
        accept(m_state, m_signature);
    }
}

void PanelProgress::accept(const QJsonObject &snapshot, const QByteArray &signature)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const bool different = signature != m_signature || m_state.isEmpty();
    const bool heartbeat = now - m_lastPublished >= HeartbeatSeconds;
    if (!different && !heartbeat) {
        return;
    }
    m_signature = signature;
    m_lastPublished = now;
    QJsonObject accepted = snapshot;
    accepted.insert(QStringLiteral("sequence"), ++m_sequence);
    accepted.insert(QStringLiteral("fetchedAt"), now);
    m_state = accepted;
    Q_EMIT changed();
}
}
