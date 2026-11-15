/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "appearance.h"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>
#include <QtGlobal>

namespace ControlPanel
{
namespace
{
struct Theme {
    QString id;
    QString name;
    bool dark;
    QVariantMap overrides;
};

/** Shared light palette. Individual themes only override the few keys that make them distinct. */
QVariantMap lightBase()
{
    return {
        {QStringLiteral("window"), QStringLiteral("#f4f7fc")},
        {QStringLiteral("surface"), QStringLiteral("#ffffff")},
        {QStringLiteral("surfaceAlt"), QStringLiteral("#f7f9fd")},
        {QStringLiteral("surfaceHover"), QStringLiteral("#eef3fc")},
        {QStringLiteral("border"), QStringLiteral("#e3e9f2")},
        {QStringLiteral("borderStrong"), QStringLiteral("#cbd7e8")},
        {QStringLiteral("divider"), QStringLiteral("#e8edf5")},
        {QStringLiteral("text"), QStringLiteral("#1b2a40")},
        {QStringLiteral("textMuted"), QStringLiteral("#5c6b81")},
        {QStringLiteral("textSubtle"), QStringLiteral("#8a97ab")},
        {QStringLiteral("textOnAccent"), QStringLiteral("#ffffff")},
        {QStringLiteral("accent"), QStringLiteral("#3b6ef5")},
        {QStringLiteral("accentHover"), QStringLiteral("#2f5cd8")},
        {QStringLiteral("accentSoft"), QStringLiteral("#e7eeff")},
        {QStringLiteral("accentSoftText"), QStringLiteral("#2549a8")},
        {QStringLiteral("link"), QStringLiteral("#3b6ef5")},
        {QStringLiteral("field"), QStringLiteral("#ffffff")},
        {QStringLiteral("fieldBorder"), QStringLiteral("#d7e0ee")},
        {QStringLiteral("sidebarTop"), QStringLiteral("#16233d")},
        {QStringLiteral("sidebarBottom"), QStringLiteral("#233c6e")},
        {QStringLiteral("sidebarText"), QStringLiteral("#f5f8ff")},
        {QStringLiteral("sidebarMuted"), QStringLiteral("#a6b8d8")},
        {QStringLiteral("sidebarActive"), QStringLiteral("#33518f")},
        {QStringLiteral("sidebarHover"), QStringLiteral("#243b6b")},
        {QStringLiteral("sidebarIndicator"), QStringLiteral("#7fb0ff")},
        {QStringLiteral("danger"), QStringLiteral("#d34545")},
        {QStringLiteral("warning"), QStringLiteral("#b2740f")},
        {QStringLiteral("success"), QStringLiteral("#2e9e6b")},
        {QStringLiteral("slot"), QStringLiteral("#ffffff")},
        {QStringLiteral("slotBorder"), QStringLiteral("#dde5f0")},
        {QStringLiteral("slotHover"), QStringLiteral("#e8f0ff")},
        {QStringLiteral("chip"), QStringLiteral("#eef2f9")},
        {QStringLiteral("chipText"), QStringLiteral("#46566e")},
        {QStringLiteral("phoneFrame"), QStringLiteral("#0d1220")},
        {QStringLiteral("shadow"), QStringLiteral("#16233d")},
        {QStringLiteral("scrim"), QStringLiteral("#0b1220")},
    };
}

/** Shared dark palette. */
QVariantMap darkBase()
{
    return {
        {QStringLiteral("window"), QStringLiteral("#121826")},
        {QStringLiteral("surface"), QStringLiteral("#1b2333")},
        {QStringLiteral("surfaceAlt"), QStringLiteral("#222c3f")},
        {QStringLiteral("surfaceHover"), QStringLiteral("#28334a")},
        {QStringLiteral("border"), QStringLiteral("#2d3950")},
        {QStringLiteral("borderStrong"), QStringLiteral("#3d4b66")},
        {QStringLiteral("divider"), QStringLiteral("#2a3448")},
        {QStringLiteral("text"), QStringLiteral("#e8eefb")},
        {QStringLiteral("textMuted"), QStringLiteral("#9dabc2")},
        {QStringLiteral("textSubtle"), QStringLiteral("#76869f")},
        {QStringLiteral("textOnAccent"), QStringLiteral("#0d1424")},
        {QStringLiteral("accent"), QStringLiteral("#6c9cff")},
        {QStringLiteral("accentHover"), QStringLiteral("#8ab0ff")},
        {QStringLiteral("accentSoft"), QStringLiteral("#213460")},
        {QStringLiteral("accentSoftText"), QStringLiteral("#bcd2ff")},
        {QStringLiteral("link"), QStringLiteral("#8ab0ff")},
        {QStringLiteral("field"), QStringLiteral("#161e2e")},
        {QStringLiteral("fieldBorder"), QStringLiteral("#33415c")},
        {QStringLiteral("sidebarTop"), QStringLiteral("#0e1422")},
        {QStringLiteral("sidebarBottom"), QStringLiteral("#1a2640")},
        {QStringLiteral("sidebarText"), QStringLiteral("#eef3ff")},
        {QStringLiteral("sidebarMuted"), QStringLiteral("#93a3c0")},
        {QStringLiteral("sidebarActive"), QStringLiteral("#273d68")},
        {QStringLiteral("sidebarHover"), QStringLiteral("#1d2b48")},
        {QStringLiteral("sidebarIndicator"), QStringLiteral("#8ab6ff")},
        {QStringLiteral("danger"), QStringLiteral("#ff8f8f")},
        {QStringLiteral("warning"), QStringLiteral("#e7bb6d")},
        {QStringLiteral("success"), QStringLiteral("#6fd6a3")},
        {QStringLiteral("slot"), QStringLiteral("#202a3c")},
        {QStringLiteral("slotBorder"), QStringLiteral("#36445e")},
        {QStringLiteral("slotHover"), QStringLiteral("#2a3d60")},
        {QStringLiteral("chip"), QStringLiteral("#26324a")},
        {QStringLiteral("chipText"), QStringLiteral("#b8c6de")},
        {QStringLiteral("phoneFrame"), QStringLiteral("#05070d")},
        {QStringLiteral("shadow"), QStringLiteral("#05080f")},
        {QStringLiteral("scrim"), QStringLiteral("#05080f")},
    };
}

const QList<Theme> &themes()
{
    static const QList<Theme> list = {
        {QStringLiteral("auto"), QStringLiteral("跟随系统"), false, {}},
        {QStringLiteral("azure"), QStringLiteral("晴空蓝"), false, {}},
        {QStringLiteral("sand"), QStringLiteral("暖砂白"), false,
         {{QStringLiteral("window"), QStringLiteral("#faf7f2")},
          {QStringLiteral("surface"), QStringLiteral("#fffdfa")},
          {QStringLiteral("surfaceAlt"), QStringLiteral("#f8f3eb")},
          {QStringLiteral("surfaceHover"), QStringLiteral("#f3eadf")},
          {QStringLiteral("border"), QStringLiteral("#ece2d5")},
          {QStringLiteral("borderStrong"), QStringLiteral("#d9c9b5")},
          {QStringLiteral("divider"), QStringLiteral("#efe7dc")},
          {QStringLiteral("text"), QStringLiteral("#33291f")},
          {QStringLiteral("textMuted"), QStringLiteral("#766759")},
          {QStringLiteral("textSubtle"), QStringLiteral("#a08e7b")},
          {QStringLiteral("accent"), QStringLiteral("#d9753f")},
          {QStringLiteral("accentHover"), QStringLiteral("#c25f2e")},
          {QStringLiteral("accentSoft"), QStringLiteral("#fbeadf")},
          {QStringLiteral("accentSoftText"), QStringLiteral("#a44f21")},
          {QStringLiteral("link"), QStringLiteral("#c25f2e")},
          {QStringLiteral("field"), QStringLiteral("#fffdfa")},
          {QStringLiteral("fieldBorder"), QStringLiteral("#ddcfbd")},
          {QStringLiteral("sidebarTop"), QStringLiteral("#34231c")},
          {QStringLiteral("sidebarBottom"), QStringLiteral("#6b4129")},
          {QStringLiteral("sidebarText"), QStringLiteral("#fff7ef")},
          {QStringLiteral("sidebarMuted"), QStringLiteral("#d8bba4")},
          {QStringLiteral("sidebarActive"), QStringLiteral("#7d4a2c")},
          {QStringLiteral("sidebarHover"), QStringLiteral("#5a3826")},
          {QStringLiteral("sidebarIndicator"), QStringLiteral("#ffb784")},
          {QStringLiteral("danger"), QStringLiteral("#c7513f")},
          {QStringLiteral("warning"), QStringLiteral("#a9761d")},
          {QStringLiteral("success"), QStringLiteral("#3c8c62")},
          {QStringLiteral("slot"), QStringLiteral("#fffdfa")},
          {QStringLiteral("slotBorder"), QStringLiteral("#e6d7c5")},
          {QStringLiteral("slotHover"), QStringLiteral("#f8e9da")},
          {QStringLiteral("chip"), QStringLiteral("#f3e9dd")},
          {QStringLiteral("chipText"), QStringLiteral("#745a44")},
          {QStringLiteral("shadow"), QStringLiteral("#3b2a1f")},
          {QStringLiteral("scrim"), QStringLiteral("#241811")}}},
        {QStringLiteral("midnight"), QStringLiteral("夜幕蓝"), true, {}},
        {QStringLiteral("graphite"), QStringLiteral("石墨紫"), true,
         {{QStringLiteral("window"), QStringLiteral("#15171c")},
          {QStringLiteral("surface"), QStringLiteral("#1d2027")},
          {QStringLiteral("surfaceAlt"), QStringLiteral("#242831")},
          {QStringLiteral("surfaceHover"), QStringLiteral("#2b303a")},
          {QStringLiteral("border"), QStringLiteral("#30353f")},
          {QStringLiteral("borderStrong"), QStringLiteral("#434a57")},
          {QStringLiteral("divider"), QStringLiteral("#2b3039")},
          {QStringLiteral("text"), QStringLiteral("#eceef4")},
          {QStringLiteral("textMuted"), QStringLiteral("#a2a9b7")},
          {QStringLiteral("textSubtle"), QStringLiteral("#7b8290")},
          {QStringLiteral("accent"), QStringLiteral("#a78bfa")},
          {QStringLiteral("accentHover"), QStringLiteral("#bda6ff")},
          {QStringLiteral("accentSoft"), QStringLiteral("#332b50")},
          {QStringLiteral("accentSoftText"), QStringLiteral("#d6c9ff")},
          {QStringLiteral("link"), QStringLiteral("#bda6ff")},
          {QStringLiteral("field"), QStringLiteral("#191c22")},
          {QStringLiteral("fieldBorder"), QStringLiteral("#383e4a")},
          {QStringLiteral("sidebarTop"), QStringLiteral("#101216")},
          {QStringLiteral("sidebarBottom"), QStringLiteral("#22252e")},
          {QStringLiteral("sidebarText"), QStringLiteral("#f2f3f7")},
          {QStringLiteral("sidebarMuted"), QStringLiteral("#9aa1ae")},
          {QStringLiteral("sidebarActive"), QStringLiteral("#34304a")},
          {QStringLiteral("sidebarHover"), QStringLiteral("#262a33")},
          {QStringLiteral("sidebarIndicator"), QStringLiteral("#c3aeff")},
          {QStringLiteral("danger"), QStringLiteral("#ff9090")},
          {QStringLiteral("warning"), QStringLiteral("#e4bd79")},
          {QStringLiteral("slot"), QStringLiteral("#21252d")},
          {QStringLiteral("slotBorder"), QStringLiteral("#383e4a")},
          {QStringLiteral("slotHover"), QStringLiteral("#33294d")},
          {QStringLiteral("chip"), QStringLiteral("#292d36")},
          {QStringLiteral("chipText"), QStringLiteral("#c2c7d2")},
          {QStringLiteral("shadow"), QStringLiteral("#08090c")},
          {QStringLiteral("scrim"), QStringLiteral("#08090c")}}},
        {QStringLiteral("forest"), QStringLiteral("松林绿"), true,
         {{QStringLiteral("window"), QStringLiteral("#101a17")},
          {QStringLiteral("surface"), QStringLiteral("#182620")},
          {QStringLiteral("surfaceAlt"), QStringLiteral("#1f3028")},
          {QStringLiteral("surfaceHover"), QStringLiteral("#26392f")},
          {QStringLiteral("border"), QStringLiteral("#2a4036")},
          {QStringLiteral("borderStrong"), QStringLiteral("#3b5a4a")},
          {QStringLiteral("divider"), QStringLiteral("#263a30")},
          {QStringLiteral("text"), QStringLiteral("#e6f4ec")},
          {QStringLiteral("textMuted"), QStringLiteral("#9db9a9")},
          {QStringLiteral("textSubtle"), QStringLiteral("#789887")},
          {QStringLiteral("accent"), QStringLiteral("#4fd1a1")},
          {QStringLiteral("accentHover"), QStringLiteral("#76e2b9")},
          {QStringLiteral("accentSoft"), QStringLiteral("#1c4032")},
          {QStringLiteral("accentSoftText"), QStringLiteral("#b1eed4")},
          {QStringLiteral("link"), QStringLiteral("#76e2b9")},
          {QStringLiteral("field"), QStringLiteral("#141f1a")},
          {QStringLiteral("fieldBorder"), QStringLiteral("#2f4b3d")},
          {QStringLiteral("sidebarTop"), QStringLiteral("#0b1712")},
          {QStringLiteral("sidebarBottom"), QStringLiteral("#15392a")},
          {QStringLiteral("sidebarText"), QStringLiteral("#effaf3")},
          {QStringLiteral("sidebarMuted"), QStringLiteral("#96bba7")},
          {QStringLiteral("sidebarActive"), QStringLiteral("#21513a")},
          {QStringLiteral("sidebarHover"), QStringLiteral("#163d2c")},
          {QStringLiteral("sidebarIndicator"), QStringLiteral("#83e9bd")},
          {QStringLiteral("danger"), QStringLiteral("#ff9494")},
          {QStringLiteral("warning"), QStringLiteral("#e5c07a")},
          {QStringLiteral("success"), QStringLiteral("#5fd6a1")},
          {QStringLiteral("slot"), QStringLiteral("#1b2b23")},
          {QStringLiteral("slotBorder"), QStringLiteral("#314b3c")},
          {QStringLiteral("slotHover"), QStringLiteral("#1f4a36")},
          {QStringLiteral("chip"), QStringLiteral("#20362a")},
          {QStringLiteral("chipText"), QStringLiteral("#b6d6c2")},
          {QStringLiteral("shadow"), QStringLiteral("#050d09")},
          {QStringLiteral("scrim"), QStringLiteral("#050d09")}}},
    };
    return list;
}

QVariantMap paletteFor(int index)
{
    const Theme &theme = themes().at(index);
    QVariantMap result = theme.dark ? darkBase() : lightBase();
    for (auto it = theme.overrides.constBegin(); it != theme.overrides.constEnd(); ++it)
        result.insert(it.key(), it.value());
    return result;
}
}

Appearance::Appearance(QObject *parent)
    : QObject(parent)
{
    QSettings settings(QStringLiteral("KDEConnect"), QStringLiteral("ControlPanel"));
    const int stored = settings.value(QStringLiteral("appearance/theme"), 0).toInt();
    m_index = qBound(0, stored, themes().size() - 1);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Qt 6.5+ reports the OS light/dark preference. Only the "auto" entry reacts to it.
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
        if (m_index == 0)
            Q_EMIT appearanceChanged();
    });
#endif
}

int Appearance::themeIndex() const
{
    return m_index;
}

void Appearance::setThemeIndex(int index)
{
    const int clamped = qBound(0, index, themes().size() - 1);
    if (clamped == m_index)
        return;
    m_index = clamped;
    QSettings settings(QStringLiteral("KDEConnect"), QStringLiteral("ControlPanel"));
    settings.setValue(QStringLiteral("appearance/theme"), m_index);
    Q_EMIT appearanceChanged();
}

QStringList Appearance::themeNames() const
{
    QStringList names;
    names.reserve(themes().size());
    for (const Theme &theme : themes())
        names.append(theme.name);
    return names;
}

QString Appearance::themeId() const
{
    return themes().at(m_index).id;
}

int Appearance::themeCount() const
{
    return themes().size();
}

int Appearance::effectiveIndex() const
{
    if (m_index != 0)
        return m_index;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)
        return 3; // midnight
#endif
    return 1; // azure
}

bool Appearance::dark() const
{
    return themes().at(effectiveIndex()).dark;
}

QVariantMap Appearance::colors() const
{
    return paletteFor(effectiveIndex());
}

}
