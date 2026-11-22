/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

namespace ControlPanel
{
/**
 * Samples local hardware counters and publishes a validated, display-ready snapshot for the
 * phone. Only aggregated numbers leave the desktop: no paths, machine names, disk labels or
 * process lists are ever included. Windows uses native APIs; other platforms report the metrics
 * as unavailable so the module still compiles and the dial degrades gracefully.
 *
 * The snapshot is a live state, like weather/progress/hass: it is never written into the
 * configuration and never changes its revision.
 */
class PanelSysInfo : public QObject
{
    Q_OBJECT
public:
    explicit PanelSysInfo(QObject *parent = nullptr);

    QJsonObject state() const;
    /** Applies the private settings block (enabled, intervalMs). */
    void configure(const QJsonObject &settings);
    /** Samples immediately, bypassing the timer. */
    void refresh();

    static constexpr int MinIntervalMs = 1000;
    static constexpr int MaxIntervalMs = 30000;
    static constexpr int DefaultIntervalMs = 5000;

    /** Settings validation shared with the document validator and tests. */
    static bool validSettings(const QJsonObject &settings);
    /**
     * Pure conversion from a raw sample (all numeric fields) into the wire snapshot.
     * Clamps ranges and drops invalid sections without discarding the whole reading.
     */
    static QJsonObject normalize(const QJsonObject &raw, qint64 fetchedAt);
    /** Snapshot used when sampling is disabled or unavailable. */
    static QJsonObject unavailable(qint64 fetchedAt, const QString &error);

Q_SIGNALS:
    void changed();

private:
    void sample();
    void publish();
    QJsonObject rawSample();
    static bool sectionsChanged(const QJsonObject &left, const QJsonObject &right);

    QTimer m_timer;
    QTimer m_heartbeat;
    QTimer m_notify;
    QJsonObject m_snapshot;
    QString m_status = QStringLiteral("disabled");
    QString m_error;
    bool m_enabled = false;
    int m_intervalMs = DefaultIntervalMs;
    qint64 m_lastSampleMs = 0;
    double m_lastIdle = -1;
    double m_lastTotal = -1;
    double m_lastRx = -1;
    double m_lastTx = -1;
};
}
