/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <optional>

namespace ControlPanel
{
struct ForegroundSample {
    QString application;
    bool ignored = true;
};

// Ignores transient focus changes without restarting the stable application's page.
class ForegroundFilter
{
public:
    static constexpr qint64 DebounceMs = 400;
    std::optional<QString> observe(const ForegroundSample &sample, qint64 nowMs);
    void cancelPending();
    static QString applicationName(const QString &value);

private:
    QString m_stable;
    QString m_candidate;
    qint64 m_candidateSince = 0;
};

class PanelForeground : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString application READ application NOTIFY applicationChanged)
    Q_PROPERTY(bool supported READ supported CONSTANT)
public:
    using Reader = std::function<ForegroundSample()>;
    explicit PanelForeground(QObject *parent = nullptr, Reader reader = {});
    QString application() const;
    static bool supported();
    static ForegroundSample readCurrent();
    void setEnabled(bool enabled);

Q_SIGNALS:
    void applicationChanged();

private:
    void poll();
    Reader m_reader;
    ForegroundFilter m_filter;
    QElapsedTimer m_clock;
    QTimer m_timer;
    QString m_application;
};
}
