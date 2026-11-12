/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include <QImage>
#include <QUrl>
#include <optional>

namespace ControlPanel
{
class PanelAssets
{
public:
    static constexpr qint64 MaxBytes = 512 * 1024;
    static constexpr qint64 MaxImportBytes = 8 * 1024 * 1024;
    static constexpr qint64 MaxStoreBytes = 128 * 1024 * 1024;
    static constexpr int IconSize = 256;
    explicit PanelAssets(const QString &configDirectory);
    static bool validId(const QString &id);
    QString importImage(const QUrl &url, QString *error = nullptr) const;
    QString importProgramIcon(const QUrl &url, QString *error = nullptr) const;
    QString importPixels(const QImage &image, QString *error = nullptr) const;
    std::optional<QByteArray> read(const QString &id) const;
    QUrl url(const QString &id) const;

private:
    QString m_directory;
};
QImage extractProgramIcon(const QString &path);
}
