/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include "paneldocument.h"
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace ControlPanel
{
// Both the editor and daemon use the same per-user directory, independently of app name.
class PanelStore : public QObject
{
    Q_OBJECT
public:
    explicit PanelStore(const QString &directory = QString(), QObject *parent = nullptr);
    static QString defaultDirectory();
    QString filePath() const;
    QString lastError() const;
    bool isUsable() const;
    const PanelDocument &document() const;

    bool load();
    bool save(const QJsonObject &config, qint64 expectedRevision);
    bool restoreBackup();

Q_SIGNALS:
    void changed();
    void errorOccurred(const QString &error);

private:
    bool fail(const QString &error);
    void watch();
    bool write(const PanelDocument &candidate, const QByteArray &previous);
    QString m_directory;
    QString m_error;
    PanelDocument m_document = PanelDocument::empty();
    bool m_loaded = false;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
};
}
