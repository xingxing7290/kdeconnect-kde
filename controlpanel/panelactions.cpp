/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelactions.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QMap>
#include <QProcess>
#include <QSet>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#endif

namespace ControlPanel
{
namespace
{
const QMap<QString, int> &keyMap()
{
    static const QMap<QString, int> map = [] {
        QMap<QString, int> result{{QStringLiteral("CTRL"), 0x11},
                                  {QStringLiteral("ALT"), 0x12},
                                  {QStringLiteral("SHIFT"), 0x10},
                                  {QStringLiteral("WIN"), 0x5B},
                                  {QStringLiteral("ENTER"), 0x0D},
                                  {QStringLiteral("ESC"), 0x1B},
                                  {QStringLiteral("TAB"), 0x09},
                                  {QStringLiteral("SPACE"), 0x20},
                                  {QStringLiteral("BACKSPACE"), 0x08},
                                  {QStringLiteral("DELETE"), 0x2E},
                                  {QStringLiteral("INSERT"), 0x2D},
                                  {QStringLiteral("HOME"), 0x24},
                                  {QStringLiteral("END"), 0x23},
                                  {QStringLiteral("PAGEUP"), 0x21},
                                  {QStringLiteral("PAGEDOWN"), 0x22},
                                  {QStringLiteral("LEFT"), 0x25},
                                  {QStringLiteral("UP"), 0x26},
                                  {QStringLiteral("RIGHT"), 0x27},
                                  {QStringLiteral("DOWN"), 0x28},
                                  {QStringLiteral("PRINTSCREEN"), 0x2C},
                                  {QStringLiteral("MEDIA_PLAY_PAUSE"), 0xB3},
                                  {QStringLiteral("MEDIA_NEXT"), 0xB0},
                                  {QStringLiteral("MEDIA_PREVIOUS"), 0xB1},
                                  {QStringLiteral("VOLUME_MUTE"), 0xAD},
                                  {QStringLiteral("VOLUME_UP"), 0xAF},
                                  {QStringLiteral("VOLUME_DOWN"), 0xAE}};
        for (int key = 'A'; key <= 'Z'; ++key)
            result.insert(QString(QChar(key)), key);
        for (int key = '0'; key <= '9'; ++key)
            result.insert(QString(QChar(key)), key);
        for (int key = 1; key <= 24; ++key)
            result.insert(QStringLiteral("F%1").arg(key), 0x6F + key);
        return result;
    }();
    return map;
}

const QMap<QString, QString> &systemTargets()
{
    static const QMap<QString, QString> targets{
        {QStringLiteral("thisPc"), QStringLiteral("shell:MyComputerFolder")},
        {QStringLiteral("recycleBin"), QStringLiteral("shell:RecycleBinFolder")},
        {QStringLiteral("settings"), QStringLiteral("ms-settings:")},
        {QStringLiteral("networkSettings"), QStringLiteral("ms-settings:network")},
        {QStringLiteral("personalization"), QStringLiteral("ms-settings:personalization")},
        {QStringLiteral("displaySettings"), QStringLiteral("ms-settings:display")},
        {QStringLiteral("soundSettings"), QStringLiteral("ms-settings:sound")},
        {QStringLiteral("showDesktop"), QStringLiteral("WIN,D")},
        {QStringLiteral("taskView"), QStringLiteral("WIN,TAB")},
        {QStringLiteral("taskManager"), QStringLiteral("CTRL,SHIFT,ESC")},
        {QStringLiteral("screenshot"), QStringLiteral("WIN,SHIFT,S")},
        {QStringLiteral("playPause"), QStringLiteral("MEDIA_PLAY_PAUSE")},
        {QStringLiteral("nextTrack"), QStringLiteral("MEDIA_NEXT")},
        {QStringLiteral("previousTrack"), QStringLiteral("MEDIA_PREVIOUS")},
        {QStringLiteral("mute"), QStringLiteral("VOLUME_MUTE")},
        {QStringLiteral("commandPrompt"), QStringLiteral("cmd")},
        {QStringLiteral("lock"), QStringLiteral("lock")},
    };
    return targets;
}

bool stringValue(const QJsonValue &value, int maximum, bool allowEmpty = false)
{
    return value.isString() && value.toString().size() <= maximum && !value.toString().contains(QChar::Null)
        && (allowEmpty || !value.toString().trimmed().isEmpty());
}

QString sendKeys(const QList<int> &keys)
{
#ifdef Q_OS_WIN
    if (!GetForegroundWindow())
        return QStringLiteral("no_foreground_window");
    for (const int modifier : {VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN}) {
        if (GetAsyncKeyState(modifier) & 0x8000)
            return QStringLiteral("modifiers_held");
    }
    QList<INPUT> input;
    auto append = [&input](int key, bool release) {
        INPUT event{};
        event.type = INPUT_KEYBOARD;
        event.ki.wVk = static_cast<WORD>(key);
        const bool extended = key == VK_LWIN || (key >= VK_PRIOR && key <= VK_DOWN) || key == VK_INSERT || key == VK_DELETE || key >= VK_VOLUME_MUTE;
        event.ki.dwFlags = (release ? KEYEVENTF_KEYUP : 0) | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
        input.append(event);
    };
    for (const int key : keys)
        append(key, false);
    for (auto it = keys.crbegin(); it != keys.crend(); ++it)
        append(*it, true);
    const auto sent = SendInput(static_cast<UINT>(input.size()), input.data(), sizeof(INPUT));
    if (sent != static_cast<UINT>(input.size())) {
        // A partial send must never leave a modifier held down.
        input.clear();
        for (auto it = keys.crbegin(); it != keys.crend(); ++it)
            append(*it, true);
        SendInput(static_cast<UINT>(input.size()), input.data(), sizeof(INPUT));
        return QStringLiteral("permission_denied");
    }
    return {};
#else
    Q_UNUSED(keys)
    return QStringLiteral("unsupported_platform");
#endif
}

bool openTarget(const QString &target)
{
#ifdef Q_OS_WIN
    const auto native = target.toStdWString();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    info.lpVerb = L"open";
    info.lpFile = native.c_str();
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info);
#else
    return QDesktopServices::openUrl(QUrl(target));
#endif
}
}

QStringList PanelActions::keyNames()
{
    return keyMap().keys();
}

QStringList PanelActions::systemActionNames()
{
    return systemTargets().keys();
}

std::optional<ResolvedAction> PanelActions::resolve(const QJsonObject &button, QString *error)
{
    if (error)
        *error = QStringLiteral("invalid_target");
    if (!button.value(QStringLiteral("target")).isObject())
        return std::nullopt;
    const auto target = button.value(QStringLiteral("target")).toObject();
    ResolvedAction action;
    action.kind = button.value(QStringLiteral("kind")).toString();
    if (action.kind == QLatin1String("website")) {
        const auto value = target.value(QStringLiteral("url"));
        if (!stringValue(value, 8192))
            return std::nullopt;
        const QUrl url(value.toString(), QUrl::StrictMode);
        if (!url.isValid() || url.host().isEmpty() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
            return std::nullopt;
        action.target = QString::fromLatin1(url.toEncoded());
    } else if (action.kind == QLatin1String("program") || action.kind == QLatin1String("file") || action.kind == QLatin1String("folder")) {
        if (!stringValue(target.value(QStringLiteral("path")), 32767))
            return std::nullopt;
        const QFileInfo file(target.value(QStringLiteral("path")).toString());
        if (!file.isAbsolute() || !file.exists()) {
            if (error)
                *error = QStringLiteral("target_missing");
            return std::nullopt;
        }
        if (action.kind == QLatin1String("folder") ? !file.isDir() : !file.isFile())
            return std::nullopt;
        action.target = file.absoluteFilePath();
        if (action.kind == QLatin1String("program")) {
#ifdef Q_OS_WIN
            if (file.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) && file.suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive))
                return std::nullopt;
#else
            if (!file.isExecutable())
                return std::nullopt;
#endif
            const auto arguments = target.value(QStringLiteral("arguments"));
            if (!arguments.isUndefined()) {
                if (!arguments.isArray() || arguments.toArray().size() > 64)
                    return std::nullopt;
                for (const auto &argument : arguments.toArray()) {
                    if (!stringValue(argument, 4096, true))
                        return std::nullopt;
                    action.arguments.append(argument.toString());
                }
            }
            const auto directory = target.value(QStringLiteral("workingDirectory"));
            if (!directory.isUndefined()) {
                if (!stringValue(directory, 32767, true))
                    return std::nullopt;
                action.workingDirectory = directory.toString();
                if (!action.workingDirectory.isEmpty() && (!QFileInfo(action.workingDirectory).isAbsolute() || !QFileInfo(action.workingDirectory).isDir()))
                    return std::nullopt;
            }
            if (file.suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) == 0 && (!action.arguments.isEmpty() || !action.workingDirectory.isEmpty()))
                return std::nullopt;
        }
    } else if (action.kind == QLatin1String("shortcut")) {
        const auto keys = target.value(QStringLiteral("keys"));
        if (!keys.isArray() || keys.toArray().isEmpty() || keys.toArray().size() > 5)
            return std::nullopt;
        QSet<int> used;
        bool normalKey = false;
        for (const auto &key : keys.toArray()) {
            if (!key.isString() || !keyMap().contains(key.toString()))
                return std::nullopt;
            const int code = keyMap().value(key.toString());
            if (used.contains(code))
                return std::nullopt;
            const bool modifier = code == 0x10 || code == 0x11 || code == 0x12 || code == 0x5B;
            if ((modifier && normalKey) || (!modifier && normalKey))
                return std::nullopt;
            normalKey |= !modifier;
            used.insert(code);
            action.keys.append(code);
        }
        if (!normalKey)
            return std::nullopt;
    } else if (action.kind == QLatin1String("system")) {
        const auto id = target.value(QStringLiteral("action"));
        if (!id.isString() || !systemTargets().contains(id.toString()))
            return std::nullopt;
        action.target = id.toString();
    } else if (action.kind == QLatin1String("command")) {
        if (!stringValue(target.value(QStringLiteral("key")), 128))
            return std::nullopt;
        action.target = target.value(QStringLiteral("key")).toString();
    } else
        return std::nullopt;
    if (error)
        error->clear();
    return action;
}

QString PanelActions::execute(const QJsonObject &button)
{
    QString error;
    const auto action = resolve(button, &error);
    if (!action)
        return error;
    if (action->kind == QLatin1String("command"))
        return QStringLiteral("command_unavailable");
    if (action->kind == QLatin1String("shortcut"))
        return sendKeys(action->keys);
    if (action->kind == QLatin1String("system")) {
#ifdef Q_OS_WIN
        const auto target = systemTargets().value(action->target);
        if (target == QLatin1String("lock"))
            return LockWorkStation() ? QString() : QStringLiteral("permission_denied");
        if (target == QLatin1String("cmd")) {
            const auto path = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32/cmd.exe"));
            return QProcess::startDetached(path, {}) ? QString() : QStringLiteral("launch_failed");
        }
        if (target.startsWith(QLatin1String("shell:")) || target.startsWith(QLatin1String("ms-settings:"))) {
            return openTarget(target) ? QString() : QStringLiteral("launch_failed");
        }
        QList<int> keys;
        for (const auto &key : target.split(QLatin1Char(',')))
            keys.append(keyMap().value(key));
        return sendKeys(keys);
#else
        return QStringLiteral("unsupported_platform");
#endif
    }
    if (action->kind == QLatin1String("program") && QFileInfo(action->target).suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) != 0) {
        return QProcess::startDetached(action->target, action->arguments, action->workingDirectory) ? QString() : QStringLiteral("launch_failed");
    }
    const QString target = action->kind == QLatin1String("website") ? action->target
#ifdef Q_OS_WIN
                                                                    : QDir::toNativeSeparators(action->target);
#else
                                                                    : QUrl::fromLocalFile(action->target).toString();
#endif
    return openTarget(target) ? QString() : QStringLiteral("launch_failed");
}
}
