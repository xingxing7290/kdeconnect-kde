/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelforeground.h"
#include <QCoreApplication>
#include <QStringList>
#include <iterator>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace ControlPanel
{
#ifdef Q_OS_WIN
namespace
{
QString processApplication(DWORD processId)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process)
        return {};
    wchar_t path[32768];
    DWORD length = DWORD(std::size(path));
    const bool success = QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    return success ? ForegroundFilter::applicationName(QString::fromWCharArray(path, int(length))) : QString();
}

struct HostedApplication {
    DWORD hostProcessId;
    QString application;
};

BOOL CALLBACK findHostedApplication(HWND window, LPARAM parameter)
{
    auto *result = reinterpret_cast<HostedApplication *>(parameter);
    if (!IsWindowVisible(window))
        return TRUE;
    wchar_t className[256] = {};
    if (!GetClassNameW(window, className, 256) || QString::fromWCharArray(className) != QLatin1String("Windows.UI.Core.CoreWindow"))
        return TRUE;
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (!processId || processId == result->hostProcessId || processId == DWORD(QCoreApplication::applicationPid()))
        return TRUE;
    result->application = processApplication(processId);
    return result->application.isEmpty() ? TRUE : FALSE;
}
}
#endif

QString ForegroundFilter::applicationName(const QString &value)
{
    QString name = value.trimmed();
    if (name.size() >= 2 && name.startsWith(QLatin1Char('"')) && name.endsWith(QLatin1Char('"')))
        name = name.mid(1, name.size() - 2).trimmed();
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    name = name.section(QLatin1Char('/'), -1);
    if (name.isEmpty() || name.size() > 260)
        return {};
    for (const auto character : name) {
        if (!character.isPrint())
            return {};
    }
    return name.toLower();
}

void ForegroundFilter::cancelPending()
{
    m_candidate.clear();
}

std::optional<QString> ForegroundFilter::observe(const ForegroundSample &sample, qint64 nowMs)
{
    const auto name = applicationName(sample.application);
    if (sample.ignored || name.isEmpty() || name == m_stable) {
        cancelPending();
        return std::nullopt;
    }
    if (name != m_candidate || nowMs < m_candidateSince) {
        m_candidate = name;
        m_candidateSince = nowMs;
        return std::nullopt;
    }
    if (nowMs - m_candidateSince < DebounceMs)
        return std::nullopt;
    m_stable = name;
    cancelPending();
    return m_stable;
}

PanelForeground::PanelForeground(QObject *parent, Reader reader)
    : QObject(parent)
    , m_reader(reader ? std::move(reader) : readCurrent)
{
    m_clock.start();
    m_timer.setInterval(200);
    connect(&m_timer, &QTimer::timeout, this, &PanelForeground::poll);
}

QString PanelForeground::application() const
{
    return m_application;
}

QString PanelForeground::error() const
{
    return m_error;
}

void PanelForeground::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    Q_EMIT errorChanged();
}

bool PanelForeground::supported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

ForegroundSample PanelForeground::readCurrent()
{
#ifdef Q_OS_WIN
    HWND window = GetForegroundWindow();
    if (!window)
        return {};
    // Owned dialogs belong to the same application; no titles or document paths are read.
    if (HWND owner = GetAncestor(window, GA_ROOTOWNER))
        window = owner;
    wchar_t className[256] = {};
    GetClassNameW(window, className, 256);
    const auto windowClass = QString::fromWCharArray(className);
    if (QStringList{QStringLiteral("#32768"), QStringLiteral("tooltips_class32"), QStringLiteral("MultitaskingViewFrame")}.contains(windowClass))
        return {};
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (!processId || processId == DWORD(QCoreApplication::applicationPid()))
        return {};
    auto name = processApplication(processId);
    if (name.isEmpty())
        return {QString(), true, QStringLiteral("process_unavailable")};
    if (QStringList{QStringLiteral("kdeconnect-app.exe"), QStringLiteral("kdeconnectd.exe"), QStringLiteral("kdeconnect-indicator.exe")}.contains(name))
        return {};
    if (name == QLatin1String("applicationframehost.exe")) {
        HostedApplication hosted{processId, {}};
        EnumChildWindows(window, findHostedApplication, reinterpret_cast<LPARAM>(&hosted));
        if (hosted.application.isEmpty())
            return {QString(), true, QStringLiteral("hosted_application_unavailable")};
        name = hosted.application;
    }
    return {name, false};
#else
    return {};
#endif
}

void PanelForeground::setEnabled(bool enabled)
{
    if (enabled == m_timer.isActive())
        return;
    m_filter.cancelPending();
    if (enabled) {
        m_timer.start();
        poll();
    } else {
        m_timer.stop();
    }
}

void PanelForeground::poll()
{
    const auto sample = m_reader();
    if (!sample.error.isEmpty())
        setError(sample.error);
    const auto stable = m_filter.observe(sample, m_clock.elapsed());
    if (stable) {
        m_application = *stable;
        setError({});
        Q_EMIT applicationChanged();
    } else if (!sample.ignored && !sample.application.isEmpty() && ForegroundFilter::applicationName(sample.application) == m_application) {
        setError({});
    }
}
}

#include "moc_panelforeground.cpp"
