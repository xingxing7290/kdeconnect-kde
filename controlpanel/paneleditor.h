/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include "panelassets.h"
#include "panelforeground.h"
#include "panelstore.h"
#include "panelweather.h"
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace ControlPanel
{
class PanelEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap config READ config NOTIFY changed)
    Q_PROPERTY(QVariantMap currentPage READ currentPage NOTIFY changed)
    Q_PROPERTY(QString currentPageId READ currentPageId NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList keyNames READ keyNames CONSTANT)
    Q_PROPERTY(QString foregroundApplication READ foregroundApplication NOTIFY foregroundChanged)
    Q_PROPERTY(bool foregroundSupported READ foregroundSupported CONSTANT)
    Q_PROPERTY(QVariantMap weather READ weather NOTIFY weatherChanged)
public:
    explicit PanelEditor(const QString &directory = {}, QObject *parent = nullptr, bool weatherNetworking = false);
    QVariantMap weather() const;
    QVariantMap config() const;
    QVariantMap currentPage() const;
    QString currentPageId() const;
    QString error() const;
    QStringList keyNames() const;
    QString foregroundApplication() const;
    bool foregroundSupported() const;
    Q_INVOKABLE bool copyForegroundApplication();
    Q_INVOKABLE bool selectPage(const QString &pageId);
    Q_INVOKABLE bool addPage();
    Q_INVOKABLE bool deletePage(const QString &pageId);
    Q_INVOKABLE bool renamePage(const QString &pageId, const QString &name, const QString &application);
    Q_INVOKABLE bool setButton(int index, const QVariantMap &button);
    Q_INVOKABLE bool clearButton(int index);
    Q_INVOKABLE bool moveButton(int source, int destination);
    Q_INVOKABLE bool dropFile(int index, const QUrl &url);
    Q_INVOKABLE QVariantMap inspectFile(const QUrl &url) const;
    Q_INVOKABLE QString importIcon(const QUrl &url);
    Q_INVOKABLE QString programIcon(const QString &path);
    Q_INVOKABLE QUrl assetUrl(const QString &id) const;
    Q_INVOKABLE bool setMode(const QString &mode);
    Q_INVOKABLE bool setAppearance(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setIdle(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setIdleOptions(const QVariantMap &values);
    Q_INVOKABLE bool restoreBackup();

Q_SIGNALS:
    void changed();
    void errorChanged();
    void foregroundChanged();
    void weatherChanged();

private:
    bool save(const QJsonObject &config);
    bool fail(const QString &error);
    int pageIndex(const QJsonArray &pages) const;
    PanelStore m_store;
    PanelForeground m_foreground;
    PanelAssets m_assets;
    PanelWeather m_weather;
    QString m_currentPageId;
    QString m_error;
};
}
