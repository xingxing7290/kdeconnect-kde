/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelsysinfo.h"
#include <QDateTime>
#include <QJsonValue>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#endif

namespace ControlPanel
{
namespace
{
constexpr int HeartbeatMs = 5000;
constexpr double MaxBytes = 9.0e18;
constexpr double MaxUptime = 4'102'444'800.0; // year 2100

bool finite(const QJsonValue &value, double minimum, double maximum)
{
    const auto result = value.toDouble(std::numeric_limits<double>::quiet_NaN());
    return value.isDouble() && std::isfinite(result) && result >= minimum && result <= maximum;
}

double number(const QJsonObject &object, const QString &key, double fallback = -1)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toDouble(fallback) : fallback;
}
}

PanelSysInfo::PanelSysInfo(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &PanelSysInfo::sample);

    m_heartbeat.setInterval(HeartbeatMs);
    connect(&m_heartbeat, &QTimer::timeout, this, [this] {
        // Re-publish the last snapshot so the phone can tell "still alive, nothing changed"
        // from "the link died"; a quiet machine would otherwise always look stale.
        if (m_enabled && m_status == QLatin1String("ready"))
            Q_EMIT changed();
    });

    m_notify.setSingleShot(true);
    m_notify.setInterval(200);
    connect(&m_notify, &QTimer::timeout, this, &PanelSysInfo::publish);

    m_snapshot = unavailable(QDateTime::currentSecsSinceEpoch(), QStringLiteral("disabled"));
}

bool PanelSysInfo::validSettings(const QJsonObject &settings)
{
    const QJsonValue enabled = settings.value(QStringLiteral("enabled"));
    if (!enabled.isBool())
        return false;
    const QJsonValue interval = settings.value(QStringLiteral("intervalMs"));
    return interval.isUndefined() || finite(interval, MinIntervalMs, MaxIntervalMs);
}

QJsonObject PanelSysInfo::unavailable(qint64 fetchedAt, const QString &error)
{
    return QJsonObject{{QStringLiteral("protocolVersion"), 1},
                       {QStringLiteral("status"), error.isEmpty() ? QStringLiteral("unavailable") : QStringLiteral("error")},
                       {QStringLiteral("error"), error},
                       {QStringLiteral("fetchedAt"), fetchedAt},
                       {QStringLiteral("cpu"), QJsonObject()},
                       {QStringLiteral("memory"), QJsonObject()},
                       {QStringLiteral("disk"), QJsonObject()},
                       {QStringLiteral("network"), QJsonObject()},
                       {QStringLiteral("uptimeSeconds"), QJsonValue()},
                       {QStringLiteral("processCount"), QJsonValue()}};
}

QJsonObject PanelSysInfo::normalize(const QJsonObject &raw, qint64 fetchedAt)
{
    QJsonObject result{{QStringLiteral("protocolVersion"), 1},
                       {QStringLiteral("status"), QStringLiteral("ready")},
                       {QStringLiteral("error"), QString()},
                       {QStringLiteral("fetchedAt"), fetchedAt}};

    const double cores = number(raw, QStringLiteral("cores"));
    if (cores >= 1 && cores <= 1024)
        result.insert(QStringLiteral("cores"), int(cores));

    const double cpuUsage = number(raw, QStringLiteral("cpuUsage"));
    if (cpuUsage >= 0 && cpuUsage <= 100) {
        QJsonObject cpu{{QStringLiteral("usage"), qRound(cpuUsage * 10) / 10.0}};
        result.insert(QStringLiteral("cpu"), cpu);
    }

    const double total = number(raw, QStringLiteral("memTotal"));
    const double used = number(raw, QStringLiteral("memUsed"));
    if (total > 0 && total <= MaxBytes && used >= 0 && used <= total) {
        result.insert(QStringLiteral("memory"),
                      QJsonObject{{QStringLiteral("usedBytes"), qint64(used)},
                                  {QStringLiteral("totalBytes"), qint64(total)},
                                  {QStringLiteral("usage"), qRound((used / total) * 1000.0) / 10.0}});
    }

    const double diskTotal = number(raw, QStringLiteral("diskTotal"));
    const double diskUsed = number(raw, QStringLiteral("diskUsed"));
    if (diskTotal > 0 && diskTotal <= MaxBytes && diskUsed >= 0 && diskUsed <= diskTotal) {
        result.insert(QStringLiteral("disk"),
                      QJsonObject{{QStringLiteral("usedBytes"), qint64(diskUsed)},
                                  {QStringLiteral("totalBytes"), qint64(diskTotal)},
                                  {QStringLiteral("usage"), qRound((diskUsed / diskTotal) * 1000.0) / 10.0}});
    }

    const double rx = number(raw, QStringLiteral("rxPerSec"));
    const double tx = number(raw, QStringLiteral("txPerSec"));
    if (rx >= 0 && rx <= MaxBytes && tx >= 0 && tx <= MaxBytes) {
        result.insert(QStringLiteral("network"),
                      QJsonObject{{QStringLiteral("rxBytesPerSec"), qint64(rx)},
                                  {QStringLiteral("txBytesPerSec"), qint64(tx)}});
    }

    const double uptime = number(raw, QStringLiteral("uptimeSeconds"));
    if (uptime >= 0 && uptime <= MaxUptime)
        result.insert(QStringLiteral("uptimeSeconds"), qint64(uptime));
    const double processes = number(raw, QStringLiteral("processCount"));
    if (processes >= 0 && processes <= 100000)
        result.insert(QStringLiteral("processCount"), int(processes));
    return result;
}

QJsonObject PanelSysInfo::state() const
{
    return m_snapshot;
}

void PanelSysInfo::configure(const QJsonObject &settings)
{
    const bool valid = validSettings(settings);
    const bool enabled = valid && settings.value(QStringLiteral("enabled")).toBool(false);
    const int interval = valid ? settings.value(QStringLiteral("intervalMs")).toInt(DefaultIntervalMs)
                               : DefaultIntervalMs;
    const bool changed = enabled != m_enabled || interval != m_intervalMs;
    m_enabled = enabled;
    m_intervalMs = qBound(MinIntervalMs, interval, MaxIntervalMs);

    if (!m_enabled) {
        m_timer.stop();
        m_heartbeat.stop();
        m_lastSampleMs = 0;
        m_status = QStringLiteral("disabled");
        m_error.clear();
        QJsonObject disabled = unavailable(QDateTime::currentSecsSinceEpoch(), QString());
        disabled.insert(QStringLiteral("status"), QStringLiteral("disabled"));
        m_snapshot = disabled;
        publish();
        return;
    }
    if (changed) {
        m_timer.stop();
        m_heartbeat.stop();
        m_lastSampleMs = 0;
        m_lastIdle = -1;
        m_lastTotal = -1;
        m_lastRx = -1;
        m_lastTx = -1;
        m_timer.start(m_intervalMs);
        m_heartbeat.start();
    }
    sample();
}

void PanelSysInfo::refresh()
{
    if (m_enabled)
        sample();
}

void PanelSysInfo::publish()
{
    Q_EMIT changed();
}

QJsonObject PanelSysInfo::rawSample()
{
    QJsonObject raw;
#if defined(Q_OS_WIN)
    FILETIME idleFt{}, kernelFt{}, userFt{};
    if (GetSystemTimes(&idleFt, &kernelFt, &userFt)) {
        const auto combine = [](const FILETIME &value) -> double {
            return double(quint64(value.dwHighDateTime) << 32 | value.dwLowDateTime);
        };
        raw.insert(QStringLiteral("cpuIdleTicks"), combine(idleFt));
        raw.insert(QStringLiteral("cpuTotalTicks"), combine(kernelFt) + combine(userFt));
        SYSTEM_INFO systemInfo{};
        GetSystemInfo(&systemInfo);
        raw.insert(QStringLiteral("cores"), int(systemInfo.dwNumberOfProcessors));
    }

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        raw.insert(QStringLiteral("memTotal"), double(memory.ullTotalPhys));
        raw.insert(QStringLiteral("memUsed"), double(memory.ullTotalPhys - memory.ullAvailPhys));
    }

    raw.insert(QStringLiteral("uptimeSeconds"), double(GetTickCount64() / 1000ULL));

    ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFree{};
    if (GetDiskFreeSpaceExW(nullptr, &freeBytes, &totalBytes, &totalFree) && totalBytes.QuadPart > 0)
        raw.insert(QStringLiteral("diskUsed"), double(totalBytes.QuadPart - totalFree.QuadPart));
    if (totalBytes.QuadPart > 0)
        raw.insert(QStringLiteral("diskTotal"), double(totalBytes.QuadPart));

    // GetIfTable2 allocates the table; aggregate all interface counters and free it afterwards.
    PMIB_IF_TABLE2 table = nullptr;
    if (GetIfTable2(&table) == NO_ERROR) {
        double received = 0;
        double sent = 0;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            received += double(table->Table[i].InOctets);
            sent += double(table->Table[i].OutOctets);
        }
        raw.insert(QStringLiteral("rxBytes"), received);
        raw.insert(QStringLiteral("txBytes"), sent);
        FreeMibTable(table);
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        DWORD count = 0;
        if (Process32FirstW(snapshot, &entry)) {
            do {
                ++count;
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        raw.insert(QStringLiteral("processCount"), int(count));
    }
#else
    // Non-Windows builds compile but report unavailable metrics; the dial degrades gracefully.
    Q_UNUSED(raw);
#endif
    return raw;
}

void PanelSysInfo::sample()
{
    if (!m_enabled) {
        m_timer.stop();
        return;
    }
    m_timer.start(m_intervalMs);
    const QJsonObject raw = rawSample();
    if (raw.isEmpty()) {
        m_status = QStringLiteral("error");
        m_error = QStringLiteral("sampling_unavailable");
        m_snapshot = unavailable(QDateTime::currentSecsSinceEpoch(), QStringLiteral("sampling_unavailable"));
        publish();
        return;
    }

    QJsonObject derived = raw;
    const qint64 sampledAt = QDateTime::currentMSecsSinceEpoch();
    const double idle = number(raw, QStringLiteral("cpuIdleTicks"));
    const double totalTicks = number(raw, QStringLiteral("cpuTotalTicks"));
    if (m_lastTotal >= 0 && totalTicks > m_lastTotal) {
        const double idleDelta = idle >= m_lastIdle ? idle - m_lastIdle : 0;
        const double totalDelta = totalTicks - m_lastTotal;
        const double usage = totalDelta > 0 ? (1.0 - idleDelta / totalDelta) * 100.0 : -1;
        if (usage >= 0 && usage <= 100 && std::isfinite(usage))
            derived.insert(QStringLiteral("cpuUsage"), usage);
    }
    m_lastIdle = idle;
    m_lastTotal = totalTicks;

    const double elapsedMs = double(sampledAt - m_lastSampleMs);
    if (m_lastSampleMs > 0 && elapsedMs >= 500 && elapsedMs <= 60000) {
        const double rx = number(raw, QStringLiteral("rxBytes"));
        const double tx = number(raw, QStringLiteral("txBytes"));
        if (m_lastRx >= 0 && rx >= m_lastRx)
            derived.insert(QStringLiteral("rxPerSec"), (rx - m_lastRx) * 1000.0 / elapsedMs);
        if (m_lastTx >= 0 && tx >= m_lastTx)
            derived.insert(QStringLiteral("txPerSec"), (tx - m_lastTx) * 1000.0 / elapsedMs);
    }
    m_lastSampleMs = sampledAt;
    m_lastRx = number(raw, QStringLiteral("rxBytes"));
    m_lastTx = number(raw, QStringLiteral("txBytes"));

    QJsonObject normalized = normalize(derived, QDateTime::currentSecsSinceEpoch());
    if (m_status != QLatin1String("ready") || !m_error.isEmpty()) {
        m_status = QStringLiteral("ready");
        m_error.clear();
        normalized.insert(QStringLiteral("status"), QStringLiteral("ready"));
        normalized.insert(QStringLiteral("error"), QString());
    }
    if (sectionsChanged(normalized, m_snapshot)) {
        m_snapshot = normalized;
        if (!m_notify.isActive())
            m_notify.start();
    }
}

bool PanelSysInfo::sectionsChanged(const QJsonObject &left, const QJsonObject &right)
{
    for (const QString &key : {QStringLiteral("cpu"), QStringLiteral("memory"), QStringLiteral("disk"), QStringLiteral("network")}) {
        if (left.value(key) != right.value(key))
            return true;
    }
    return left.value(QStringLiteral("uptimeSeconds")) != right.value(QStringLiteral("uptimeSeconds"))
        || left.value(QStringLiteral("processCount")) != right.value(QStringLiteral("processCount"))
        || left.value(QStringLiteral("status")).toString() != right.value(QStringLiteral("status")).toString();
}
}
