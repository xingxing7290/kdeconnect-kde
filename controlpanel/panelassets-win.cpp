/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelassets.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <qt_windows.h>

#include <shlobj.h>
#include <wrl/client.h>

namespace ControlPanel
{
namespace
{
QString localPath(const QString &path)
{
    static const QRegularExpression drive(QStringLiteral("\\A[A-Za-z]:[/\\\\]"));
    if (!drive.match(path).hasMatch())
        return {};
    const auto root = QDir::toNativeSeparators(path.left(3));
    const auto driveType = GetDriveTypeW(reinterpret_cast<LPCWSTR>(root.utf16()));
    if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE)
        return {};
    const auto canonical = QFileInfo(path).canonicalFilePath();
    return drive.match(canonical).hasMatch() && QFileInfo(canonical).isFile() ? canonical : QString();
}
}
QImage extractProgramIcon(const QString &source)
{
    auto path = localPath(source);
    if (path.isEmpty() || QFileInfo(path).size() > 256 * 1024 * 1024)
        return {};
    int index = 0;
    if (QFileInfo(path).suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) == 0) {
        if (QFileInfo(path).size() > 1024 * 1024)
            return {};
        const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        struct Cleanup {
            bool active;
            ~Cleanup()
            {
                if (active)
                    CoUninitialize();
            }
        } cleanup{SUCCEEDED(initialized)};
        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
            return {};
        Microsoft::WRL::ComPtr<IShellLinkW> link;
        Microsoft::WRL::ComPtr<IPersistFile> file;
        if (FAILED(CoCreateInstance(__uuidof(ShellLink), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))) || FAILED(link.As(&file))
            || FAILED(file->Load(reinterpret_cast<LPCWSTR>(path.utf16()), STGM_READ)))
            return {};
        wchar_t iconPath[MAX_PATH] = {};
        link->GetIconLocation(iconPath, MAX_PATH, &index);
        path = localPath(QString::fromWCharArray(iconPath));
        if (path.isEmpty()) {
            wchar_t target[MAX_PATH] = {};
            link->GetPath(target, MAX_PATH, nullptr, SLGP_RAWPATH);
            path = localPath(QString::fromWCharArray(target));
            index = 0;
        }
    }
    if (path.isEmpty() || QFileInfo(path).size() > 256 * 1024 * 1024)
        return {};
    const auto suffix = QFileInfo(path).suffix().toLower();
    if (!QStringList{QStringLiteral("exe"), QStringLiteral("dll"), QStringLiteral("ico")}.contains(suffix))
        return {};
    HICON icon = nullptr;
    const auto result = SHDefExtractIconW(reinterpret_cast<LPCWSTR>(path.utf16()), index, 0, &icon, nullptr, MAKELONG(PanelAssets::IconSize, 0));
    const auto image = SUCCEEDED(result) && icon ? QImage::fromHICON(icon) : QImage();
    if (icon)
        DestroyIcon(icon);
    return image;
}
}
