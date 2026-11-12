/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

namespace ControlPanel
{
class PanelWeather : public QObject
{
    Q_OBJECT
public:
    explicit PanelWeather(const QString &directory, QObject *parent = nullptr, QNetworkAccessManager *network = nullptr, bool networking = false);
    ~PanelWeather() override;
    QJsonObject state() const;
    void configure(const QJsonObject &idle);
    void refresh();
    static bool validCode(int code);
    static bool validObservation(const QJsonObject &object, qint64 now, qint64 maximumAge = 10800);
    static bool validForecast(const QJsonObject &object);

Q_SIGNALS:
    void changed();

private:
    void locate(const QString &name, bool fallback);
    void request(const QUrl &url, std::function<void(const QJsonObject &)> success);
    void unavailable(const QString &error);
    void complete(QJsonObject data);
    QJsonObject cached() const;
    QString m_path;
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_timer;
    QJsonObject m_state;
    QString m_city;
    bool m_enabled = false;
    bool m_networking;
    quint64 m_generation = 0;
};
}
