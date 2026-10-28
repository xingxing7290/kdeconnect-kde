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
QString ForegroundFilter::applicationName(const QString &value)
{
    QString name = value.trimmed();
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
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process)
        return {};
    wchar_t path[32768];
    DWORD length = DWORD(std::size(path));
    const bool success = QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    if (!success)
        return {};
    const auto name = ForegroundFilter::applicationName(QString::fromWCharArray(path, int(length)));
    if (QStringList{QStringLiteral("kdeconnect-app.exe"), QStringLiteral("kdeconnectd.exe"), QStringLiteral("kdeconnect-indicator.exe")}.contains(name))
        return {};
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
    const auto stable = m_filter.observe(m_reader(), m_clock.elapsed());
    if (stable) {
        m_application = *stable;
        Q_EMIT applicationChanged();
    }
}
}

#include "moc_panelforeground.cpp"
