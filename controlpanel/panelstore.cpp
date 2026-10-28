/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace ControlPanel
{
namespace
{
std::optional<QByteArray> readLimited(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > PanelDocument::MaxBytes) {
        return std::nullopt;
    }
    return file.read(PanelDocument::MaxBytes + 1);
}

bool atomicWrite(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    return file.write(bytes) == bytes.size() && file.commit();
}
}

PanelStore::PanelStore(const QString &directory, QObject *parent)
    : QObject(parent)
    , m_directory(directory.isEmpty() ? defaultDirectory() : QDir(directory).absolutePath())
{
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(100);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        m_reloadTimer.start();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        m_reloadTimer.start();
    });
    connect(&m_reloadTimer, &QTimer::timeout, this, [this] {
        load();
    });
}

QString PanelStore::defaultDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath(QStringLiteral("kdeconnect/controlpanel"));
}

QString PanelStore::filePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("panel.json"));
}

QString PanelStore::lastError() const
{
    return m_error;
}

bool PanelStore::isUsable() const
{
    return m_loaded && m_error.isEmpty();
}

const PanelDocument &PanelStore::document() const
{
    return m_document;
}

bool PanelStore::fail(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorOccurred(error);
    }
    return false;
}

void PanelStore::watch()
{
    if (!m_watcher.directories().contains(m_directory)) {
        m_watcher.addPath(m_directory);
    }
    if (QFile::exists(filePath()) && !m_watcher.files().contains(filePath())) {
        m_watcher.addPath(filePath());
    }
}

bool PanelStore::load()
{
    if (!QDir().mkpath(m_directory)) {
        return fail(QStringLiteral("directory_unavailable"));
    }
    watch();
    if (!QFile::exists(filePath())) {
        if (m_loaded || QFile::exists(filePath() + QStringLiteral(".bak"))) {
            return fail(QStringLiteral("missing_config"));
        }
        return save(m_document.privateConfig(), 0);
    }
    const auto bytes = readLimited(filePath());
    if (!bytes) {
        return fail(QStringLiteral("config_unreadable"));
    }
    QString error;
    const auto loaded = PanelDocument::fromJson(*bytes, &error);
    if (!loaded) {
        return fail(error);
    }
    if (m_loaded && loaded->revision() < m_document.revision()) {
        return fail(QStringLiteral("revision_rollback"));
    }
    if (m_loaded && loaded->revision() == m_document.revision() && loaded->privateConfig() != m_document.privateConfig()) {
        return fail(QStringLiteral("revision_collision"));
    }
    const bool updated = !m_loaded || !m_error.isEmpty() || loaded->privateConfig() != m_document.privateConfig();
    m_document = *loaded;
    m_loaded = true;
    m_error.clear();
    if (updated) {
        Q_EMIT changed();
    }
    return true;
}

bool PanelStore::save(const QJsonObject &config, qint64 expectedRevision)
{
    QString validationError;
    const auto candidate = PanelDocument::fromJson(QJsonDocument(config).toJson(QJsonDocument::Compact), &validationError);
    if (!candidate) {
        return fail(validationError);
    }
    if (!QDir().mkpath(m_directory)) {
        return fail(QStringLiteral("directory_unavailable"));
    }
    QLockFile lock(filePath() + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) {
        return fail(QStringLiteral("store_busy"));
    }
    QByteArray previous;
    qint64 currentRevision = 0;
    if (QFile::exists(filePath())) {
        const auto bytes = readLimited(filePath());
        const auto current = bytes ? PanelDocument::fromJson(*bytes) : std::nullopt;
        if (!current) {
            return fail(QStringLiteral("invalid_on_disk"));
        }
        previous = *bytes;
        currentRevision = current->revision();
    } else if (m_loaded || QFile::exists(filePath() + QStringLiteral(".bak"))) {
        return fail(QStringLiteral("missing_config"));
    }
    if (currentRevision != expectedRevision) {
        return fail(QStringLiteral("stale_revision"));
    }
    return write(*candidate, previous);
}

bool PanelStore::write(const PanelDocument &candidate, const QByteArray &previous)
{
    const QString counterPath = QDir(m_directory).filePath(QStringLiteral("revision"));
    qint64 counter = 0;
    if (QFile::exists(counterPath)) {
        const auto bytes = readLimited(counterPath);
        bool ok = false;
        counter = bytes ? bytes->trimmed().toLongLong(&ok) : -1;
        if (!ok || counter < 0 || counter > PanelDocument::MaxRevision) {
            return fail(QStringLiteral("invalid_revision_counter"));
        }
    }
    const auto oldDocument = previous.isEmpty() ? std::nullopt : PanelDocument::fromJson(previous);
    const qint64 highest = std::max({counter, m_document.revision(), candidate.revision(), oldDocument ? oldDocument->revision() : 0});
    if (highest >= PanelDocument::MaxRevision) {
        return fail(QStringLiteral("revision_exhausted"));
    }
    QJsonObject config = candidate.privateConfig();
    config.insert(QStringLiteral("revision"), highest + 1);
    const QByteArray encoded = QJsonDocument(config).toJson(QJsonDocument::Compact);
    const auto validated = PanelDocument::fromJson(encoded);
    if (!validated) {
        return fail(QStringLiteral("config_too_large"));
    }
    if (!previous.isEmpty() && !atomicWrite(filePath() + QStringLiteral(".bak"), previous)) {
        return fail(QStringLiteral("backup_failed"));
    }
    // Commit the counter first: interrupted writes may leave gaps, never reused revisions.
    if (!atomicWrite(counterPath, QByteArray::number(highest + 1)) || !atomicWrite(filePath(), encoded)) {
        return fail(QStringLiteral("write_failed"));
    }
    m_document = *validated;
    m_loaded = true;
    m_error.clear();
    watch();
    Q_EMIT changed();
    return true;
}

bool PanelStore::restoreBackup()
{
    QLockFile lock(filePath() + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) {
        return fail(QStringLiteral("store_busy"));
    }
    const auto bytes = readLimited(filePath() + QStringLiteral(".bak"));
    const auto backup = bytes ? PanelDocument::fromJson(*bytes) : std::nullopt;
    if (!backup) {
        return fail(QStringLiteral("invalid_backup"));
    }
    if (QFile::exists(filePath())) {
        const QString preserved = filePath() + QStringLiteral(".rejected-") + QString::number(QDateTime::currentMSecsSinceEpoch());
        if (!QFile::copy(filePath(), preserved)) {
            return fail(QStringLiteral("preserve_failed"));
        }
    }
    return write(*backup, {});
}
}

#include "moc_panelstore.cpp"
