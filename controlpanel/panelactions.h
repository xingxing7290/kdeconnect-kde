/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include <QJsonObject>
#include <QStringList>
#include <optional>

namespace ControlPanel
{
struct ResolvedAction {
    QString kind;
    QString target;
    QStringList arguments;
    QString workingDirectory;
    QList<int> keys;
};

class PanelActions
{
public:
    static std::optional<ResolvedAction> resolve(const QJsonObject &button, QString *error = nullptr);
    static QString execute(const QJsonObject &button);
    static QStringList keyNames();
    static QStringList systemActionNames();
};
}
