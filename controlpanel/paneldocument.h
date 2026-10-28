/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace ControlPanel
{
class PanelDocument
{
public:
    static constexpr int MaxBytes = 256 * 1024;
    static constexpr int MaxDepth = 64;
    static constexpr int SlotCount = 18;
    static constexpr int MaxPages = 10;
    static constexpr qint64 MaxRevision = 9007199254740991LL;

    static std::optional<PanelDocument> fromJson(const QByteArray &json, QString *error = nullptr);
    static PanelDocument empty();

    const QJsonObject &privateConfig() const;
    QJsonObject publicConfig() const;
    qint64 revision() const;
    QJsonObject button(const QString &id) const;

private:
    explicit PanelDocument(const QJsonObject &config);
    static bool validate(const QJsonObject &config, QString *error);
    QJsonObject m_config;
};
}
