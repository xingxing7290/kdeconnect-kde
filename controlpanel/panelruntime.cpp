/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelruntime.h"
#include "panelforeground.h"
#include <QJsonArray>

namespace ControlPanel
{
PanelRuntime::PanelRuntime(PanelStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    connect(store, &PanelStore::changed, this, [this] {
        const auto config = m_store->document().privateConfig();
        const auto mode = config.value(QStringLiteral("mode")).toString();
        if (mode != m_mode)
            m_manualPageId.clear();
        m_mode = mode;
        bool manualPageExists = false;
        for (const auto &page : config.value(QStringLiteral("pages")).toArray()) {
            if (page.toObject().value(QStringLiteral("id")).toString() == m_manualPageId)
                manualPageExists = true;
        }
        if (!manualPageExists)
            m_manualPageId.clear();
        m_activePageId = mode == QLatin1String("single") ? config.value(QStringLiteral("homePageId")).toString()
            : mode == QLatin1String("follow")            ? (m_manualPageId.isEmpty() ? followedPage(config) : m_manualPageId)
                                                         : config.value(QStringLiteral("activePageId")).toString();
        Q_EMIT stateChanged();
    });
    const auto config = store->document().privateConfig();
    const auto mode = config.value(QStringLiteral("mode")).toString();
    m_mode = mode;
    m_activePageId = mode == QLatin1String("single") || mode == QLatin1String("follow") ? config.value(QStringLiteral("homePageId")).toString()
                                                                                        : config.value(QStringLiteral("activePageId")).toString();
}

PanelStore *PanelRuntime::store() const
{
    return m_store;
}

QJsonObject PanelRuntime::state() const
{
    return {{QStringLiteral("protocolVersion"), 1},
            {QStringLiteral("revision"), m_store->document().revision()},
            {QStringLiteral("activePageId"), m_activePageId},
            {QStringLiteral("foregroundApplication"), m_foregroundApplication}};
}

QString PanelRuntime::followedPage(const QJsonObject &config) const
{
    for (const auto &value : config.value(QStringLiteral("pages")).toArray()) {
        const auto page = value.toObject();
        const auto application = ForegroundFilter::applicationName(page.value(QStringLiteral("application")).toString());
        if (!application.isEmpty() && application == m_foregroundApplication)
            return page.value(QStringLiteral("id")).toString();
    }
    return config.value(QStringLiteral("homePageId")).toString();
}

void PanelRuntime::setForegroundApplication(const QString &application)
{
    const auto name = ForegroundFilter::applicationName(application);
    if (name.isEmpty() || m_foregroundApplication == name)
        return;
    m_foregroundApplication = name;
    m_manualPageId.clear();
    const auto config = m_store->document().privateConfig();
    if (m_store->isUsable() && config.value(QStringLiteral("mode")).toString() == QLatin1String("follow"))
        m_activePageId = followedPage(config);
    Q_EMIT stateChanged();
}

bool PanelRuntime::selectPage(const QString &pageId)
{
    if (!m_store->isUsable()) {
        return false;
    }
    const auto config = m_store->document().privateConfig();
    if (config.value(QStringLiteral("mode")).toString() == QLatin1String("single")) {
        return pageId == config.value(QStringLiteral("homePageId")).toString();
    }
    for (const auto &page : config.value(QStringLiteral("pages")).toArray()) {
        if (page.toObject().value(QStringLiteral("id")).toString() == pageId) {
            if (config.value(QStringLiteral("mode")).toString() == QLatin1String("follow"))
                m_manualPageId = pageId;
            if (m_activePageId != pageId) {
                m_activePageId = pageId;
                Q_EMIT stateChanged();
            }
            return true;
        }
    }
    return false;
}
}

#include "moc_panelruntime.cpp"
