/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace ControlPanel
{
/**
 * Local appearance preferences for the Windows control-panel window.
 *
 * The selected theme is deliberately kept out of panel.json: that document is
 * synchronised to the paired Android device and carries a revision counter, so
 * storing a desktop-only preference there would make every theme change look
 * like a configuration change to the phone. QSettings keeps the choice local.
 */
class Appearance : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int themeIndex READ themeIndex WRITE setThemeIndex NOTIFY appearanceChanged)
    Q_PROPERTY(QStringList themeNames READ themeNames CONSTANT)
    Q_PROPERTY(QString themeId READ themeId NOTIFY appearanceChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY appearanceChanged)

public:
    explicit Appearance(QObject *parent = nullptr);

    int themeIndex() const;
    void setThemeIndex(int index);
    QStringList themeNames() const;
    QString themeId() const;
    bool dark() const;
    QVariantMap colors() const;

    /** Number of selectable entries, including index 0 ("follow system"). */
    Q_INVOKABLE int themeCount() const;

Q_SIGNALS:
    void appearanceChanged();

private:
    int effectiveIndex() const;

    int m_index = 0;
};

}
