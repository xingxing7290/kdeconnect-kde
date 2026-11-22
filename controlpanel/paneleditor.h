/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include "panelassets.h"
#include "panelfavicon.h"
#include "panelforeground.h"
#include "panelhass.h"
#include "panelsysinfo.h"
#include "panelportable.h"
#include "panelprogress.h"
#include "panelstore.h"
#include "panelweather.h"
#include <QFileSystemWatcher>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace ControlPanel
{
class PanelEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap config READ config NOTIFY configChanged)
    Q_PROPERTY(QVariantMap currentPage READ currentPage NOTIFY changed)
    Q_PROPERTY(QString currentPageId READ currentPageId NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList keyNames READ keyNames CONSTANT)
    Q_PROPERTY(QString foregroundApplication READ foregroundApplication NOTIFY foregroundChanged)
    Q_PROPERTY(bool foregroundSupported READ foregroundSupported CONSTANT)
    Q_PROPERTY(QString foregroundError READ foregroundError NOTIFY foregroundChanged)
    Q_PROPERTY(QVariantMap weather READ weather NOTIFY weatherChanged)
    Q_PROPERTY(QVariantMap progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QVariantMap hass READ hass NOTIFY hassChanged)
    Q_PROPERTY(QVariantList hassEntities READ hassEntities NOTIFY hassEntitiesChanged)
    Q_PROPERTY(QVariantMap hassState READ hassState NOTIFY hassChanged)
    Q_PROPERTY(QString hassCompatNote READ hassCompatNote NOTIFY hassCompatChanged)
    Q_PROPERTY(QVariantMap sysInfo READ sysInfo NOTIFY sysInfoChanged)
    Q_PROPERTY(bool sysInfoEnabled READ sysInfoEnabled NOTIFY sysInfoChanged)
    Q_PROPERTY(QString websiteIconStatus READ websiteIconStatus NOTIFY websiteIconStatusChanged)
public:
    /** Weather, website icons and the WorkBuddy progress source all use this switch; tests keep networking off. */
    explicit PanelEditor(const QString &directory = {}, QObject *parent = nullptr, bool networking = false);
    QVariantMap weather() const;
    QVariantMap progress() const;
    /** Home Assistant block as stored privately (includes the token, so it can be edited). */
    QVariantMap hass() const;
    QVariantList hassEntities() const;
    QVariantMap hassState() const;
    QVariantMap sysInfo() const;
    bool sysInfoEnabled() const;
    /** Non-empty when the connected peer is too old to understand the smart-home dial. */
    QString hassCompatNote() const;
    QVariantMap config() const;
    QVariantMap currentPage() const;
    QString currentPageId() const;
    QString error() const;
    QStringList keyNames() const;
    QString foregroundApplication() const;
    bool foregroundSupported() const;
    QString foregroundError() const;
    QString websiteIconStatus() const;
    Q_INVOKABLE bool copyForegroundApplication();
    Q_INVOKABLE bool selectPage(const QString &pageId);
    Q_INVOKABLE bool addPage();
    Q_INVOKABLE bool deletePage(const QString &pageId);
    Q_INVOKABLE bool renamePage(const QString &pageId, const QString &name, const QString &application);
    Q_INVOKABLE QVariantMap pageEditContext(const QString &pageId) const;
    Q_INVOKABLE bool renamePageChecked(const QString &pageId, const QString &name, const QString &application, const QVariantMap &context);
    Q_INVOKABLE bool deletePageChecked(const QString &pageId, const QVariantMap &context);
    Q_INVOKABLE bool setButton(int index, const QVariantMap &button);
    Q_INVOKABLE QVariantMap editContext(int index) const;
    Q_INVOKABLE bool setButtonChecked(int index, const QVariantMap &button, const QVariantMap &context);
    Q_INVOKABLE bool clearButtonChecked(int index, const QVariantMap &context);
    Q_INVOKABLE bool clearButton(int index);
    Q_INVOKABLE bool moveButton(int source, int destination);
    Q_INVOKABLE bool moveButtonChecked(int source, int destination, const QVariantMap &context);
    Q_INVOKABLE bool dropFile(int index, const QUrl &url);
    Q_INVOKABLE QVariantMap inspectFile(const QUrl &url) const;
    Q_INVOKABLE QString importIcon(const QUrl &url);
    Q_INVOKABLE QString programIcon(const QString &path);
    /** Preview-only fetch: emits websiteIconReady(url, iconId) without changing the configuration. */
    Q_INVOKABLE void fetchWebsiteIcon(const QString &url);
    Q_INVOKABLE QUrl assetUrl(const QString &id) const;
    Q_INVOKABLE bool setMode(const QString &mode);
    Q_INVOKABLE bool setAppearance(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setAppearanceChecked(const QString &key, const QVariant &value, double expectedRevision);
    Q_INVOKABLE bool setIdle(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setIdleOptions(const QVariantMap &values);
    Q_INVOKABLE bool setIdleOptionsChecked(const QVariantMap &values, double expectedRevision);
    Q_INVOKABLE bool setHass(const QVariantMap &values);
    Q_INVOKABLE bool setHassChecked(const QVariantMap &values, double expectedRevision);
    Q_INVOKABLE void refreshHass();
    Q_INVOKABLE bool setSysInfoEnabled(bool enabled);
    Q_INVOKABLE bool setSysInfoInterval(int intervalMs);
    /** Controls Home Assistant directly from the desktop; returns an empty string on success. */
    Q_INVOKABLE QString hassCommand(const QString &entityId, const QString &action, int brightness);
    Q_INVOKABLE bool fillMissingIcons();
    Q_INVOKABLE bool restoreBackup();
    /** Portable bundles: result maps carry ok/error and a human-readable summary. */
    Q_INVOKABLE QVariantMap inspectBundle(const QUrl &url);
    Q_INVOKABLE QVariantMap exportBundle(const QUrl &url);
    Q_INVOKABLE QVariantMap importBundle(const QUrl &url);

Q_SIGNALS:
    void configChanged();
    void changed();
    void errorChanged();
    void foregroundChanged();
    void weatherChanged();
    void progressChanged();
    void hassChanged();
    void hassEntitiesChanged();
    void hassCompatChanged();
    void sysInfoChanged();
    void websiteIconStatusChanged();
    void websiteIconReady(const QString &url, const QString &iconId);

private:
    bool save(const QJsonObject &config);
    bool fail(const QString &error);
    void setWebsiteIconStatus(const QString &status);
    void updateHass();
    void updateHassCompat();
    void startWebsiteIcon(const QString &address,
                          const QUrl &url,
                          const QString &pageId,
                          int index,
                          const QString &buttonId,
                          bool apply,
                          const QString &expectedIcon = {});
    void applyWebsiteIcon(const QString &pageId, int index, const QString &buttonId, const QString &url, const QString &iconId, const QString &expectedIcon);
    int pageIndex(const QJsonArray &pages) const;
    bool matchesContext(int index, const QVariantMap &context) const;
    bool matchesPageContext(const QString &pageId, const QVariantMap &context) const;
    QString defaultButtonIcon(const QJsonObject &button) const;
    PanelStore m_store;
    PanelForeground m_foreground;
    PanelAssets m_assets;
    PanelWeather m_weather;
    PanelProgress m_progress;
    PanelHass m_hass;
    PanelSysInfo m_sysInfo;
    PanelFavicon m_favicon;
    QString m_hassCompatPath;
    QFileSystemWatcher m_hassCompatWatcher;
    QString m_currentPageId;
    QString m_error;
    QString m_websiteIconStatus = QStringLiteral("idle");
};
}
