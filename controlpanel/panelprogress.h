/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;

namespace ControlPanel
{
/**
 * Reads a WorkBuddy progress source (local JSON file or HTTP endpoint) and publishes a live
 * snapshot. The source is a desktop-private setting: it is read only, never executed, and never
 * sent to the phone. File changes trigger an immediate read; a timer provides the fallback poll
 * and periodic heartbeat so the phone can keep the snapshot fresh without a packet storm.
 */
class PanelProgress : public QObject
{
    Q_OBJECT
public:
    explicit PanelProgress(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr, bool networking = false);
    ~PanelProgress() override;
    QJsonObject state() const;
    void configure(const QJsonObject &info);
    void refresh();
    /** Pure helpers: turn an untrusted source payload into a validated, normalised snapshot. */
    static QJsonObject normalize(const QByteArray &data, qint64 now, qint64 sourceTime);
    static QJsonObject unavailable(qint64 now, const QString &error);

Q_SIGNALS:
    void changed();

private:
    void readFile();
    void requestUrl();
    void heartbeat();
    void accept(const QJsonObject &snapshot, const QByteArray &signature);
    void failProgress(const QString &error);
    static QByteArray signatureOf(const QJsonObject &snapshot);
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QFileSystemWatcher m_watcher;
    QTimer m_timer;
    QTimer m_heartbeat;
    QJsonObject m_state;
    QString m_source;
    QString m_path;
    QString m_url;
    QString m_watchedPath;
    QByteArray m_signature;
    qint64 m_sequence = 0;
    qint64 m_lastPublished = 0;
    int m_intervalMs = 2000;
    bool m_enabled = false;
    bool m_networking;
};
}
