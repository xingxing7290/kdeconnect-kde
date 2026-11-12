/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelassets.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>

namespace ControlPanel
{
namespace
{
QString fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return {};
}
}
PanelAssets::PanelAssets(const QString &directory)
    : m_directory(QDir(directory).filePath(QStringLiteral("assets")))
{
}
bool PanelAssets::validId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[a-f0-9]{64}\\z"));
    return pattern.match(id).hasMatch();
}
QString PanelAssets::importImage(const QUrl &url, QString *error) const
{
    if (!url.isLocalFile())
        return fail(error, QStringLiteral("invalid_image"));
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > MaxImportBytes)
        return fail(error, QStringLiteral("image_too_large_or_unreadable"));
    const auto bytes = file.read(MaxImportBytes + 1);
    if (bytes.size() > MaxImportBytes)
        return fail(error, QStringLiteral("image_too_large_or_unreadable"));
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto format = reader.format().toLower();
    const auto size = reader.size();
    if ((format != QByteArrayLiteral("png") && format != QByteArrayLiteral("jpeg") && format != QByteArrayLiteral("jpg")) || size.width() <= 0
        || size.height() <= 0 || size.width() > 4096 || size.height() > 4096)
        return fail(error, QStringLiteral("invalid_image"));
    reader.setAutoTransform(true);
    reader.setScaledSize(size.scaled(IconSize, IconSize, Qt::KeepAspectRatio));
    return importPixels(reader.read(), error);
}
QString PanelAssets::importProgramIcon(const QUrl &url, QString *error) const
{
    if (!url.isLocalFile())
        return fail(error, QStringLiteral("icon_unavailable"));
    const auto image = extractProgramIcon(url.toLocalFile());
    if (image.isNull())
        return fail(error, QStringLiteral("icon_unavailable"));
    return importPixels(image, error);
}
QString PanelAssets::importPixels(const QImage &image, QString *error) const
{
    if (error)
        error->clear();
    if (image.isNull() || image.width() > 4096 || image.height() > 4096)
        return fail(error, QStringLiteral("invalid_image"));
    QImage normalized(IconSize, IconSize, QImage::Format_ARGB32);
    normalized.fill(Qt::transparent);
    QPainter painter(&normalized);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const auto size = image.size().scaled(IconSize, IconSize, Qt::KeepAspectRatio);
    painter.drawImage(QRect(QPoint((IconSize - size.width()) / 2, (IconSize - size.height()) / 2), size), image);
    painter.end();
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!normalized.save(&buffer, "PNG") || bytes.size() > MaxBytes)
        return fail(error, QStringLiteral("invalid_image"));
    const auto id = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    if (read(id))
        return id;
    if (QFileInfo(m_directory).isSymLink() || !QDir().mkpath(m_directory))
        return fail(error, QStringLiteral("asset_store_unavailable"));
    qint64 used = 0;
    const auto files = QDir(m_directory).entryInfoList({QStringLiteral("*.png")}, QDir::Files);
    for (const auto &entry : files)
        used += entry.size();
    if (used + bytes.size() > MaxStoreBytes)
        return fail(error, QStringLiteral("asset_store_full"));
    const auto path = QDir(m_directory).filePath(id + QStringLiteral(".png"));
    if (QFileInfo(path).isSymLink())
        return fail(error, QStringLiteral("asset_store_unavailable"));
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit())
        return fail(error, QStringLiteral("asset_store_unavailable"));
    return id;
}
std::optional<QByteArray> PanelAssets::read(const QString &id) const
{
    if (!validId(id) || QFileInfo(m_directory).isSymLink())
        return std::nullopt;
    const auto path = QDir(m_directory).filePath(id + QStringLiteral(".png"));
    if (QFileInfo(path).isSymLink())
        return std::nullopt;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > MaxBytes)
        return std::nullopt;
    const auto bytes = file.read(MaxBytes + 1);
    if (bytes.size() > MaxBytes || QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() != id.toLatin1())
        return std::nullopt;
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, QByteArrayLiteral("png"));
    if (reader.size() != QSize(IconSize, IconSize) || reader.read().isNull())
        return std::nullopt;
    return bytes;
}
QUrl PanelAssets::url(const QString &id) const
{
    return read(id) ? QUrl::fromLocalFile(QDir(m_directory).filePath(id + QStringLiteral(".png"))) : QUrl();
}
#ifndef Q_OS_WIN
QImage extractProgramIcon(const QString &)
{
    return {};
}
#endif
}
