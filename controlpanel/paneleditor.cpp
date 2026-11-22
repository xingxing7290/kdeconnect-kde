/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "paneleditor.h"
#include "panelactions.h"
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QUuid>

namespace ControlPanel
{
PanelEditor::PanelEditor(const QString &directory, QObject *parent, bool networking)
    : QObject(parent)
    , m_store(directory)
    , m_assets(QFileInfo(m_store.filePath()).absolutePath())
    , m_weather(QFileInfo(m_store.filePath()).absolutePath(), this, nullptr, networking)
    , m_progress(this, nullptr, networking)
    , m_hass(this)
    , m_sysInfo(this)
    , m_favicon(QFileInfo(m_store.filePath()).absolutePath(), this, nullptr, networking)
    , m_hassCompatPath(QFileInfo(m_store.filePath()).absolutePath() + QStringLiteral("/hass-peer-compat.json"))
{
    connect(&m_store, &PanelStore::errorOccurred, this, [this](const QString &error) {
        fail(error);
    });
    connect(&m_store, &PanelStore::changed, this, [this] {
        const auto root = m_store.document().privateConfig();
        m_foreground.setEnabled(root.value(QStringLiteral("mode")).toString() == QLatin1String("follow"));
        if (root.value(QStringLiteral("mode")).toString() == QLatin1String("single") || pageIndex(root.value(QStringLiteral("pages")).toArray()) < 0) {
            m_currentPageId = root.value(QStringLiteral("homePageId")).toString();
        }
        m_error.clear();
        Q_EMIT errorChanged();
        Q_EMIT configChanged();
        Q_EMIT changed();
    });
    connect(&m_foreground, &PanelForeground::applicationChanged, this, &PanelEditor::foregroundChanged);
    connect(&m_foreground, &PanelForeground::errorChanged, this, &PanelEditor::foregroundChanged);
    connect(&m_weather, &PanelWeather::changed, this, &PanelEditor::weatherChanged);
    connect(&m_progress, &PanelProgress::changed, this, &PanelEditor::progressChanged);
    connect(&m_hass, &PanelHass::changed, this, &PanelEditor::updateHass);
    connect(&m_sysInfo, &PanelSysInfo::changed, this, &PanelEditor::sysInfoChanged);
    const auto updateWeather = [this] {
        m_weather.configure(m_store.isUsable() ? m_store.document().privateConfig().value(QStringLiteral("idle")).toObject() : QJsonObject());
    };
    const auto updateProgress = [this] {
        const QJsonObject idle = m_store.isUsable() ? m_store.document().privateConfig().value(QStringLiteral("idle")).toObject() : QJsonObject();
        m_progress.configure(idle.value(QStringLiteral("info")).toObject());
    };
    connect(&m_store, &PanelStore::changed, this, updateWeather);
    connect(&m_store, &PanelStore::errorOccurred, this, updateWeather);
    connect(&m_store, &PanelStore::changed, this, updateProgress);
    connect(&m_store, &PanelStore::errorOccurred, this, updateProgress);
    const auto updateHassConfig = [this] {
        m_hass.configure(m_store.isUsable() ? m_store.document().privateConfig().value(QStringLiteral("hass")).toObject() : QJsonObject());
    };
    connect(&m_store, &PanelStore::changed, this, updateHassConfig);
    connect(&m_store, &PanelStore::errorOccurred, this, updateHassConfig);
    const auto updateSysInfoConfig = [this] {
        m_sysInfo.configure(m_store.isUsable() ? m_store.document().privateConfig().value(QStringLiteral("sysinfo")).toObject() : QJsonObject());
    };
    connect(&m_store, &PanelStore::changed, this, updateSysInfoConfig);
    connect(&m_store, &PanelStore::errorOccurred, this, updateSysInfoConfig);
    if (QFile::exists(m_hassCompatPath))
        m_hassCompatWatcher.addPath(m_hassCompatPath);
    connect(&m_hassCompatWatcher, &QFileSystemWatcher::fileChanged, this, &PanelEditor::updateHassCompat);
    m_store.load();
    updateWeather();
    updateProgress();
    updateHassConfig();
    updateSysInfoConfig();
    if (m_currentPageId.isEmpty())
        m_currentPageId = m_store.document().privateConfig().value(QStringLiteral("homePageId")).toString();
}

QVariantMap PanelEditor::weather() const
{
    return m_weather.state().toVariantMap();
}

QVariantMap PanelEditor::progress() const
{
    return m_progress.state().toVariantMap();
}

QVariantMap PanelEditor::hass() const
{
    return m_store.document().privateConfig().value(QStringLiteral("hass")).toObject().toVariantMap();
}

QVariantList PanelEditor::hassEntities() const
{
    return m_hass.availableEntities().toVariantList();
}

QVariantMap PanelEditor::hassState() const
{
    return m_hass.state().toVariantMap();
}

QString PanelEditor::hassCompatNote() const
{
    QFile marker(m_hassCompatPath);
    if (!marker.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject value = QJsonDocument::fromJson(marker.readAll()).object();
    if (!value.contains(QLatin1String("supportsHass")) || value.value(QLatin1String("supportsHass")).toBool(true))
        return {};
    return tr("平板上的控制面板版本过旧，无法识别「智能家居」表盘。下发时已临时改用数字时钟；请升级平板上的 APK 后再切换。");
}

void PanelEditor::updateHassCompat()
{
    // Some tools replace the marker instead of editing it in place, which drops the watch.
    if (!m_hassCompatWatcher.files().contains(m_hassCompatPath) && QFile::exists(m_hassCompatPath))
        m_hassCompatWatcher.addPath(m_hassCompatPath);
    Q_EMIT hassCompatChanged();
}

QVariantMap PanelEditor::sysInfo() const
{
    return m_sysInfo.state().toVariantMap();
}

bool PanelEditor::sysInfoEnabled() const
{
    return m_store.document().privateConfig().value(QStringLiteral("sysinfo")).toObject()
        .value(QStringLiteral("enabled")).toBool(false);
}

bool PanelEditor::setSysInfoEnabled(bool enabled)
{
    QJsonObject config = m_store.document().privateConfig();
    QJsonObject settings = config.value(QStringLiteral("sysinfo")).toObject();
    settings.insert(QStringLiteral("enabled"), enabled);
    config.insert(QStringLiteral("sysinfo"), settings);
    return save(config);
}

bool PanelEditor::setSysInfoInterval(int intervalMs)
{
    if (intervalMs < PanelSysInfo::MinIntervalMs || intervalMs > PanelSysInfo::MaxIntervalMs)
        return fail(QStringLiteral("invalid_option"));
    QJsonObject config = m_store.document().privateConfig();
    QJsonObject settings = config.value(QStringLiteral("sysinfo")).toObject();
    settings.insert(QStringLiteral("intervalMs"), intervalMs);
    config.insert(QStringLiteral("sysinfo"), settings);
    return save(config);
}

QVariantMap PanelEditor::config() const
{
    return m_store.document().privateConfig().toVariantMap();
}
QString PanelEditor::currentPageId() const
{
    return m_currentPageId;
}
QString PanelEditor::error() const
{
    return m_error;
}
QStringList PanelEditor::keyNames() const
{
    return PanelActions::keyNames();
}

QString PanelEditor::foregroundApplication() const
{
    return m_foreground.application();
}

bool PanelEditor::foregroundSupported() const
{
    return PanelForeground::supported();
}

QString PanelEditor::foregroundError() const
{
    return m_foreground.error();
}

QString PanelEditor::websiteIconStatus() const
{
    return m_websiteIconStatus;
}

bool PanelEditor::copyForegroundApplication()
{
    if (m_foreground.application().isEmpty() || !m_foreground.error().isEmpty() || !qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return false;
    QGuiApplication::clipboard()->setText(m_foreground.application());
    return true;
}

int PanelEditor::pageIndex(const QJsonArray &pages) const
{
    for (int index = 0; index < pages.size(); ++index) {
        if (pages[index].toObject().value(QStringLiteral("id")).toString() == m_currentPageId)
            return index;
    }
    return -1;
}

QVariantMap PanelEditor::currentPage() const
{
    const auto pages = m_store.document().privateConfig().value(QStringLiteral("pages")).toArray();
    const int index = pageIndex(pages);
    return index < 0 ? QVariantMap() : pages[index].toObject().toVariantMap();
}

bool PanelEditor::fail(const QString &error)
{
    m_error = error;
    Q_EMIT errorChanged();
    return false;
}

bool PanelEditor::save(const QJsonObject &config)
{
    return m_store.save(config, config.value(QStringLiteral("revision")).toInteger(-1));
}

QVariantMap PanelEditor::editContext(int index) const
{
    if (index < 0 || index >= PanelDocument::SlotCount)
        return {};
    const auto slots = currentPage().value(QStringLiteral("slots")).toList();
    if (slots.size() != PanelDocument::SlotCount)
        return {};
    return {{QStringLiteral("pageId"), m_currentPageId},
            {QStringLiteral("revision"), m_store.document().revision()},
            {QStringLiteral("slotIndex"), index},
            {QStringLiteral("buttonId"), slots[index].toMap().value(QStringLiteral("id")).toString()}};
}

bool PanelEditor::matchesContext(int index, const QVariantMap &context) const
{
    const auto expected = editContext(index);
    const auto json = QJsonObject::fromVariantMap(context);
    return !expected.isEmpty() && json.value(QStringLiteral("pageId")).isString() && json.value(QStringLiteral("buttonId")).isString()
        && json.value(QStringLiteral("revision")).isDouble() && json.value(QStringLiteral("slotIndex")).isDouble()
        && json.value(QStringLiteral("pageId")).toString() == expected.value(QStringLiteral("pageId")).toString()
        && json.value(QStringLiteral("buttonId")).toString() == expected.value(QStringLiteral("buttonId")).toString()
        && json.value(QStringLiteral("revision")).toDouble() == expected.value(QStringLiteral("revision")).toDouble()
        && json.value(QStringLiteral("slotIndex")).toDouble() == index;
}

bool PanelEditor::setButtonChecked(int index, const QVariantMap &button, const QVariantMap &context)
{
    return matchesContext(index, context) ? setButton(index, button) : fail(QStringLiteral("edit_conflict"));
}

bool PanelEditor::clearButtonChecked(int index, const QVariantMap &context)
{
    return matchesContext(index, context) ? clearButton(index) : fail(QStringLiteral("edit_conflict"));
}

bool PanelEditor::moveButtonChecked(int source, int destination, const QVariantMap &context)
{
    return matchesContext(source, context) ? moveButton(source, destination) : fail(QStringLiteral("edit_conflict"));
}

QVariantMap PanelEditor::pageEditContext(const QString &pageId) const
{
    for (const auto &page : m_store.document().privateConfig().value(QStringLiteral("pages")).toArray()) {
        if (page.toObject().value(QStringLiteral("id")).toString() == pageId)
            return {{QStringLiteral("pageId"), pageId}, {QStringLiteral("revision"), m_store.document().revision()}};
    }
    return {};
}

bool PanelEditor::matchesPageContext(const QString &pageId, const QVariantMap &context) const
{
    const auto expected = pageEditContext(pageId);
    const auto json = QJsonObject::fromVariantMap(context);
    return !expected.isEmpty() && json.value(QStringLiteral("pageId")).isString() && json.value(QStringLiteral("revision")).isDouble()
        && json.value(QStringLiteral("pageId")).toString() == pageId
        && json.value(QStringLiteral("revision")).toDouble() == expected.value(QStringLiteral("revision")).toDouble();
}

bool PanelEditor::renamePageChecked(const QString &pageId, const QString &name, const QString &application, const QVariantMap &context)
{
    return matchesPageContext(pageId, context) ? renamePage(pageId, name, application) : fail(QStringLiteral("edit_conflict"));
}

bool PanelEditor::deletePageChecked(const QString &pageId, const QVariantMap &context)
{
    return matchesPageContext(pageId, context) ? deletePage(pageId) : fail(QStringLiteral("edit_conflict"));
}

bool PanelEditor::selectPage(const QString &pageId)
{
    for (const auto &page : m_store.document().privateConfig().value(QStringLiteral("pages")).toArray()) {
        if (page.toObject().value(QStringLiteral("id")).toString() == pageId) {
            if (m_currentPageId == pageId)
                return true;
            m_currentPageId = pageId;
            Q_EMIT changed();
            return true;
        }
    }
    return fail(QStringLiteral("unknown_page"));
}

bool PanelEditor::addPage()
{
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    if (pages.size() >= PanelDocument::MaxPages)
        return fail(QStringLiteral("page_limit"));
    QJsonArray buttons;
    for (int index = 0; index < PanelDocument::SlotCount; ++index)
        buttons.append(QJsonValue::Null);
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    pages.append(QJsonObject{{QStringLiteral("id"), id},
                             {QStringLiteral("name"), QStringLiteral("Page %1").arg(pages.size() + 1)},
                             {QStringLiteral("application"), QString()},
                             {QStringLiteral("slots"), buttons}});
    config.insert(QStringLiteral("pages"), pages);
    if (!save(config))
        return false;
    return selectPage(id);
}

bool PanelEditor::deletePage(const QString &pageId)
{
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    if (pages.size() <= 1)
        return fail(QStringLiteral("last_page"));
    bool found = false;
    for (int index = 0; index < pages.size(); ++index) {
        if (pages[index].toObject().value(QStringLiteral("id")).toString() == pageId) {
            pages.removeAt(index);
            found = true;
            break;
        }
    }
    if (!found)
        return fail(QStringLiteral("unknown_page"));
    config.insert(QStringLiteral("pages"), pages);
    const auto fallback = pages.first().toObject().value(QStringLiteral("id")).toString();
    if (config.value(QStringLiteral("homePageId")).toString() == pageId)
        config.insert(QStringLiteral("homePageId"), fallback);
    if (config.value(QStringLiteral("activePageId")).toString() == pageId)
        config.insert(QStringLiteral("activePageId"), config.value(QStringLiteral("homePageId")));
    return save(config);
}

bool PanelEditor::renamePage(const QString &pageId, const QString &name, const QString &application)
{
    const auto applicationName = ForegroundFilter::applicationName(application);
    if (!application.trimmed().isEmpty() && applicationName.isEmpty())
        return fail(QStringLiteral("invalid_application"));
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    for (int index = 0; index < pages.size(); ++index) {
        auto page = pages[index].toObject();
        if (page.value(QStringLiteral("id")).toString() == pageId) {
            page.insert(QStringLiteral("name"), name.trimmed());
            page.insert(QStringLiteral("application"), applicationName);
            pages[index] = page;
            config.insert(QStringLiteral("pages"), pages);
            return save(config);
        }
    }
    return fail(QStringLiteral("unknown_page"));
}

bool PanelEditor::setButton(int index, const QVariantMap &value)
{
    if (index < 0 || index >= PanelDocument::SlotCount)
        return fail(QStringLiteral("invalid_slot"));
    auto button = QJsonObject::fromVariantMap(value);
    QString error;
    if (!PanelActions::resolve(button, &error))
        return fail(error);
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    const int pageNumber = pageIndex(pages);
    if (pageNumber < 0)
        return fail(QStringLiteral("unknown_page"));
    auto page = pages[pageNumber].toObject();
    auto buttons = page.value(QStringLiteral("slots")).toArray();
    // Existing identity belongs to this slot; a new library/drop button gets a fresh ID.
    const auto oldId = buttons[index].toObject().value(QStringLiteral("id")).toString();
    button.insert(QStringLiteral("id"), oldId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : oldId);
    // A website without a user-supplied icon is saved with the generic browser icon first,
    // then its favicon replaces that icon once the network fetch finishes.
    const bool needsWebsiteIcon = button.value(QStringLiteral("kind")).toString() == QLatin1String("website")
        && button.value(QStringLiteral("icon")).toString().isEmpty();
    if (!button.contains(QStringLiteral("icon")))
        button.insert(QStringLiteral("icon"), QString());
    if (button.value(QStringLiteral("icon")).toString().isEmpty())
        button.insert(QStringLiteral("icon"), defaultButtonIcon(button));
    if (!button.value(QStringLiteral("icon")).toString().isEmpty() && !m_assets.read(button.value(QStringLiteral("icon")).toString()))
        return fail(QStringLiteral("icon_unavailable"));
    buttons[index] = button;
    page.insert(QStringLiteral("slots"), buttons);
    pages[pageNumber] = page;
    config.insert(QStringLiteral("pages"), pages);
    if (!save(config))
        return false;
    if (needsWebsiteIcon) {
        const auto address = button.value(QStringLiteral("target")).toObject().value(QStringLiteral("url")).toString();
        const QUrl url = PanelFavicon::normalize(address);
        if (!url.isEmpty())
            startWebsiteIcon(address,
                             url,
                             page.value(QStringLiteral("id")).toString(),
                             index,
                             button.value(QStringLiteral("id")).toString(),
                             true,
                             button.value(QStringLiteral("icon")).toString());
    }
    return true;
}

bool PanelEditor::clearButton(int index)
{
    if (index < 0 || index >= PanelDocument::SlotCount)
        return fail(QStringLiteral("invalid_slot"));
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    const int pageNumber = pageIndex(pages);
    if (pageNumber < 0)
        return fail(QStringLiteral("unknown_page"));
    auto page = pages[pageNumber].toObject();
    auto buttons = page.value(QStringLiteral("slots")).toArray();
    buttons[index] = QJsonValue::Null;
    page.insert(QStringLiteral("slots"), buttons);
    pages[pageNumber] = page;
    config.insert(QStringLiteral("pages"), pages);
    return save(config);
}

bool PanelEditor::moveButton(int source, int destination)
{
    if (source < 0 || source >= PanelDocument::SlotCount || destination < 0 || destination >= PanelDocument::SlotCount)
        return fail(QStringLiteral("invalid_slot"));
    if (source == destination)
        return true;
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    const int pageNumber = pageIndex(pages);
    if (pageNumber < 0)
        return fail(QStringLiteral("unknown_page"));
    auto page = pages[pageNumber].toObject();
    auto buttons = page.value(QStringLiteral("slots")).toArray();
    const QJsonValue sourceButton = buttons[source];
    buttons[source] = buttons[destination];
    buttons[destination] = sourceButton;
    page.insert(QStringLiteral("slots"), buttons);
    pages[pageNumber] = page;
    config.insert(QStringLiteral("pages"), pages);
    return save(config);
}

bool PanelEditor::dropFile(int index, const QUrl &url)
{
    if (!url.isLocalFile())
        return fail(QStringLiteral("invalid_target"));
    return setButton(index, inspectFile(url));
}

QVariantMap PanelEditor::inspectFile(const QUrl &url) const
{
    if (!url.isLocalFile())
        return {};
    const QFileInfo file(url.toLocalFile());
    const bool program =
        file.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0 || file.suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) == 0;
    const auto kind = file.isDir() ? QStringLiteral("folder") : program ? QStringLiteral("program") : QStringLiteral("file");
    auto name = file.isDir() ? file.fileName() : file.completeBaseName();
    if (name.isEmpty())
        name = file.fileName().isEmpty() ? tr("Root folder") : file.fileName();
    return {{QStringLiteral("kind"), kind},
            {QStringLiteral("name"), name},
            {QStringLiteral("target"), QVariantMap{{QStringLiteral("path"), file.absoluteFilePath()}}}};
}

bool PanelEditor::setMode(const QString &mode)
{
    auto config = m_store.document().privateConfig();
    config.insert(QStringLiteral("mode"), mode);
    return save(config);
}

QString PanelEditor::importIcon(const QUrl &url)
{
    QString error;
    const auto id = m_assets.importImage(url, &error);
    if (id.isEmpty())
        fail(error);
    return id;
}
QString PanelEditor::programIcon(const QString &path)
{
    QString error;
    const auto id = m_assets.importProgramIcon(QUrl::fromLocalFile(path), &error);
    if (id.isEmpty())
        fail(error);
    return id;
}

void PanelEditor::setWebsiteIconStatus(const QString &status)
{
    if (m_websiteIconStatus == status)
        return;
    m_websiteIconStatus = status;
    Q_EMIT websiteIconStatusChanged();
}

void PanelEditor::fetchWebsiteIcon(const QString &url)
{
    const auto address = url.trimmed();
    const QUrl parsed = PanelFavicon::normalize(address);
    if (parsed.isEmpty()) {
        setWebsiteIconStatus(QStringLiteral("unavailable"));
        return;
    }
    startWebsiteIcon(address, parsed, {}, -1, {}, false);
}

void PanelEditor::startWebsiteIcon(const QString &address,
                                   const QUrl &url,
                                   const QString &pageId,
                                   int index,
                                   const QString &buttonId,
                                   bool apply,
                                   const QString &expectedIcon)
{
    setWebsiteIconStatus(QStringLiteral("loading"));
    m_favicon.fetch(url, [this, address, pageId, index, buttonId, apply, expectedIcon](const QString &iconId, const QString &error) {
        if (iconId.isEmpty()) {
            setWebsiteIconStatus(error == QLatin1String("favicon_disabled") ? QStringLiteral("disabled")
                                                                           : QStringLiteral("unavailable"));
            return;
        }
        setWebsiteIconStatus(QStringLiteral("ready"));
        Q_EMIT websiteIconReady(address, iconId);
        if (apply)
            applyWebsiteIcon(pageId, index, buttonId, address, iconId, expectedIcon);
    });
}

void PanelEditor::applyWebsiteIcon(const QString &pageId,
                                   int index,
                                   const QString &buttonId,
                                   const QString &url,
                                   const QString &iconId,
                                   const QString &expectedIcon)
{
    // The button may have been edited, moved or deleted while the network request was running.
    // Only the exact button that started the fetch may be updated, using the current revision.
    if (pageId.isEmpty() || buttonId.isEmpty() || index < 0 || index >= PanelDocument::SlotCount || !m_assets.read(iconId))
        return;
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    for (qsizetype number = 0; number < pages.size(); ++number) {
        auto page = pages[number].toObject();
        if (page.value(QStringLiteral("id")).toString() != pageId)
            continue;
        auto buttons = page.value(QStringLiteral("slots")).toArray();
        if (index >= buttons.size())
            return;
        auto button = buttons[index].toObject();
        if (button.value(QStringLiteral("id")).toString() != buttonId || button.value(QStringLiteral("kind")).toString() != QLatin1String("website")
            || button.value(QStringLiteral("target")).toObject().value(QStringLiteral("url")).toString() != url
            || button.value(QStringLiteral("icon")).toString() != expectedIcon || button.value(QStringLiteral("icon")).toString() == iconId)
            return;
        button.insert(QStringLiteral("icon"), iconId);
        buttons[index] = button;
        page.insert(QStringLiteral("slots"), buttons);
        pages[number] = page;
        config.insert(QStringLiteral("pages"), pages);
        save(config);
        return;
    }
}

QString PanelEditor::defaultButtonIcon(const QJsonObject &button) const
{
    const auto kind = button.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("program")) {
        const auto id =
            m_assets.importProgramIcon(QUrl::fromLocalFile(button.value(QStringLiteral("target")).toObject().value(QStringLiteral("path")).toString()));
        if (!id.isEmpty())
            return id;
    }
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return {};
    static const QHash<QString, QString> systemIcons{{QStringLiteral("thisPc"), QStringLiteral("computer")},
                                                     {QStringLiteral("recycleBin"), QStringLiteral("user-trash")},
                                                     {QStringLiteral("commandPrompt"), QStringLiteral("utilities-terminal")},
                                                     {QStringLiteral("taskManager"), QStringLiteral("utilities-system-monitor")},
                                                     {QStringLiteral("showDesktop"), QStringLiteral("user-desktop")},
                                                     {QStringLiteral("screenshot"), QStringLiteral("camera-photo")},
                                                     {QStringLiteral("settings"), QStringLiteral("preferences-system")},
                                                     {QStringLiteral("networkSettings"), QStringLiteral("network-wireless")},
                                                     {QStringLiteral("personalization"), QStringLiteral("preferences-desktop-theme")},
                                                     {QStringLiteral("displaySettings"), QStringLiteral("video-display")},
                                                     {QStringLiteral("soundSettings"), QStringLiteral("audio-volume-high")}};
    static const QHash<QString, QString> kindIcons{{QStringLiteral("website"), QStringLiteral("internet-web-browser")},
                                                   {QStringLiteral("file"), QStringLiteral("text-x-generic")},
                                                   {QStringLiteral("folder"), QStringLiteral("folder")},
                                                   {QStringLiteral("shortcut"), QStringLiteral("input-keyboard")},
                                                   {QStringLiteral("command"), QStringLiteral("utilities-terminal")}};
    const auto name = kind == QLatin1String("system")
        ? systemIcons.value(button.value(QStringLiteral("target")).toObject().value(QStringLiteral("action")).toString())
        : kindIcons.value(kind, QStringLiteral("application-x-executable"));
    const auto icon = QIcon::fromTheme(name);
    return icon.isNull() ? QString() : m_assets.importPixels(icon.pixmap(PanelAssets::IconSize, PanelAssets::IconSize).toImage());
}

bool PanelEditor::fillMissingIcons()
{
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    bool changed = false;
    for (int pageNumber = 0; pageNumber < pages.size(); ++pageNumber) {
        auto page = pages[pageNumber].toObject();
        auto buttons = page.value(QStringLiteral("slots")).toArray();
        for (int slot = 0; slot < buttons.size(); ++slot) {
            auto button = buttons[slot].toObject();
            if (button.isEmpty() || !button.value(QStringLiteral("icon")).toString().isEmpty())
                continue;
            const auto id = defaultButtonIcon(button);
            if (id.isEmpty())
                return fail(QStringLiteral("icon_unavailable"));
            button.insert(QStringLiteral("icon"), id);
            buttons[slot] = button;
            changed = true;
        }
        page.insert(QStringLiteral("slots"), buttons);
        pages[pageNumber] = page;
    }
    if (!changed)
        return true;
    config.insert(QStringLiteral("pages"), pages);
    return save(config);
}
QUrl PanelEditor::assetUrl(const QString &id) const
{
    return m_assets.url(id);
}

bool PanelEditor::setAppearance(const QString &key, const QVariant &value)
{
    if (!QStringList{QStringLiteral("background"), QStringLiteral("showLabels"), QStringLiteral("showVolume"), QStringLiteral("swipeDirection")}.contains(key))
        return fail(QStringLiteral("invalid_option"));
    auto config = m_store.document().privateConfig();
    auto options = config.value(QStringLiteral("appearance")).toObject();
    options.insert(key, QJsonValue::fromVariant(value));
    config.insert(QStringLiteral("appearance"), options);
    return save(config);
}

bool PanelEditor::setAppearanceChecked(const QString &key, const QVariant &value, double expectedRevision)
{
    return expectedRevision == double(m_store.document().revision()) ? setAppearance(key, value) : fail(QStringLiteral("edit_conflict"));
}

bool PanelEditor::setIdle(const QString &key, const QVariant &value)
{
    return setIdleOptions({{key, value}});
}

bool PanelEditor::setIdleOptions(const QVariantMap &values)
{
    auto config = m_store.document().privateConfig();
    auto options = config.value(QStringLiteral("idle")).toObject();
    static const QStringList scalarKeys{QStringLiteral("enabled"), QStringLiteral("timeoutMs"), QStringLiteral("dialId"),
                                        QStringLiteral("city"), QStringLiteral("message")};
    static const QStringList infoKeys{QStringLiteral("notes"), QStringLiteral("showForecast"), QStringLiteral("showProgress"),
                                      QStringLiteral("staleMs"), QStringLiteral("source"), QStringLiteral("path"),
                                      QStringLiteral("url"), QStringLiteral("intervalMs")};
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (it.key() == QStringLiteral("info")) {
            const QVariantMap incoming = it.value().toMap();
            if (incoming.isEmpty()) {
                return fail(QStringLiteral("invalid_option"));
            }
            QJsonObject info = options.value(QStringLiteral("info")).toObject();
            for (auto field = incoming.constBegin(); field != incoming.constEnd(); ++field) {
                if (!infoKeys.contains(field.key())) {
                    return fail(QStringLiteral("invalid_option"));
                }
                info.insert(field.key(), QJsonValue::fromVariant(field.value()));
            }
            options.insert(QStringLiteral("info"), info);
            continue;
        }
        if (!scalarKeys.contains(it.key())) {
            return fail(QStringLiteral("invalid_option"));
        }
        options.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    config.insert(QStringLiteral("idle"), options);
    return save(config);
}

bool PanelEditor::setIdleOptionsChecked(const QVariantMap &values, double expectedRevision)
{
    return expectedRevision == double(m_store.document().revision()) ? setIdleOptions(values) : fail(QStringLiteral("edit_conflict"));
}

bool PanelEditor::setHass(const QVariantMap &values)
{
    auto config = m_store.document().privateConfig();
    QJsonObject hass = config.value(QStringLiteral("hass")).toObject();
    static const QStringList keys{QStringLiteral("enabled"), QStringLiteral("url"), QStringLiteral("token"), QStringLiteral("entities")};
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!keys.contains(it.key())) {
            return fail(QStringLiteral("invalid_option"));
        }
        hass.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    config.insert(QStringLiteral("hass"), hass);
    return save(config);
}

bool PanelEditor::setHassChecked(const QVariantMap &values, double expectedRevision)
{
    return expectedRevision == double(m_store.document().revision()) ? setHass(values) : fail(QStringLiteral("edit_conflict"));
}

void PanelEditor::refreshHass()
{
    m_hass.refresh();
}

QString PanelEditor::hassCommand(const QString &entityId, const QString &action, int brightness)
{
    // The GUI holds its own HA connection; issuing the service call here avoids a round trip
    // through the daemon and the phone. The resulting state_changed is seen by both processes,
    // so the daemon still pushes the fresh snapshot to the tablet.
    return m_hass.callService(entityId, action, brightness);
}

void PanelEditor::updateHass()
{
    Q_EMIT hassChanged();
    Q_EMIT hassEntitiesChanged();
}

bool PanelEditor::restoreBackup()
{
    return m_store.restoreBackup();
}

QVariantMap PanelEditor::inspectBundle(const QUrl &url)
{
    return PanelPortable::inspect(url, m_assets);
}

QVariantMap PanelEditor::exportBundle(const QUrl &url)
{
    return PanelPortable::exportTo(url, m_store, m_assets);
}

QVariantMap PanelEditor::importBundle(const QUrl &url)
{
    // Bundle errors are reported by the dedicated dialog; do not call fail()
    // here or the generic errorChanged popup would fire as well.
    const auto result = PanelPortable::importFrom(url, m_store, m_assets);
    if (result.value(QStringLiteral("ok")).toBool()) {
        m_currentPageId = m_store.document().privateConfig().value(QStringLiteral("homePageId")).toString();
        Q_EMIT changed();
    }
    return result;
}
}

#include "moc_paneleditor.cpp"
