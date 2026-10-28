/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelruntime.h"
#include <QJsonArray>

namespace ControlPanel
{
PanelRuntime::PanelRuntime(PanelStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    connect(store, &PanelStore::changed, this, [this] {
        const auto config = m_store->document().privateConfig();
        m_activePageId = config.value(QStringLiteral("mode")).toString() == QLatin1String("single") ? config.value(QStringLiteral("homePageId")).toString()
                                                                                                    : config.value(QStringLiteral("activePageId")).toString();
        Q_EMIT stateChanged();
    });
    const auto config = store->document().privateConfig();
    m_activePageId = config.value(QStringLiteral("mode")).toString() == QLatin1String("single") ? config.value(QStringLiteral("homePageId")).toString()
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
            {QStringLiteral("foregroundApplication"), QString()}};
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
