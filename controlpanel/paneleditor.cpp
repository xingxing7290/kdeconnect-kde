/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "paneleditor.h"
#include "panelactions.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QUrl>
#include <QUuid>

namespace ControlPanel
{
PanelEditor::PanelEditor(const QString &directory, QObject *parent)
    : QObject(parent)
    , m_store(directory)
{
    connect(&m_store, &PanelStore::errorOccurred, this, [this](const QString &error) {
        fail(error);
    });
    connect(&m_store, &PanelStore::changed, this, [this] {
        const auto root = m_store.document().privateConfig();
        if (pageIndex(root.value(QStringLiteral("pages")).toArray()) < 0) {
            m_currentPageId = root.value(QStringLiteral("homePageId")).toString();
        }
        m_error.clear();
        Q_EMIT errorChanged();
        Q_EMIT changed();
    });
    m_store.load();
    if (m_currentPageId.isEmpty())
        m_currentPageId = m_store.document().privateConfig().value(QStringLiteral("homePageId")).toString();
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
    return m_store.save(config, m_store.document().revision());
}

bool PanelEditor::selectPage(const QString &pageId)
{
    for (const auto &page : m_store.document().privateConfig().value(QStringLiteral("pages")).toArray()) {
        if (page.toObject().value(QStringLiteral("id")).toString() == pageId) {
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
    auto config = m_store.document().privateConfig();
    auto pages = config.value(QStringLiteral("pages")).toArray();
    for (int index = 0; index < pages.size(); ++index) {
        auto page = pages[index].toObject();
        if (page.value(QStringLiteral("id")).toString() == pageId) {
            page.insert(QStringLiteral("name"), name.trimmed());
            page.insert(QStringLiteral("application"), application.trimmed());
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
    if (!button.contains(QStringLiteral("icon")))
        button.insert(QStringLiteral("icon"), QString());
    buttons[index] = button;
    page.insert(QStringLiteral("slots"), buttons);
    pages[pageNumber] = page;
    config.insert(QStringLiteral("pages"), pages);
    return save(config);
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
    const auto name = file.isDir() ? file.fileName() : file.completeBaseName();
    return {{QStringLiteral("kind"), kind},
            {QStringLiteral("name"), name.isEmpty() ? file.absoluteFilePath() : name},
            {QStringLiteral("target"), QVariantMap{{QStringLiteral("path"), file.absoluteFilePath()}}}};
}

bool PanelEditor::setMode(const QString &mode)
{
    auto config = m_store.document().privateConfig();
    config.insert(QStringLiteral("mode"), mode);
    return save(config);
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

bool PanelEditor::setIdle(const QString &key, const QVariant &value)
{
    return setIdleOptions({{key, value}});
}

bool PanelEditor::setIdleOptions(const QVariantMap &values)
{
    auto config = m_store.document().privateConfig();
    auto options = config.value(QStringLiteral("idle")).toObject();
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!QStringList{QStringLiteral("enabled"), QStringLiteral("timeoutMs"), QStringLiteral("dialId"), QStringLiteral("city"), QStringLiteral("message")}
                 .contains(it.key()))
            return fail(QStringLiteral("invalid_option"));
        options.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    config.insert(QStringLiteral("idle"), options);
    return save(config);
}

bool PanelEditor::restoreBackup()
{
    return m_store.restoreBackup();
}
}

#include "moc_paneleditor.cpp"
