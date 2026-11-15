/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once

#include <QUrl>
#include <QVariantMap>

namespace ControlPanel
{
class PanelAssets;
class PanelStore;

/**
 * Portable configuration bundles: one JSON document carrying the complete desktop
 * configuration (all pages, all buttons and their targets) plus every icon that
 * those buttons reference. Icons are stored as verbatim base64 PNG bytes so that
 * their SHA-256 ids remain valid after import. Revision numbers are never
 * imported; the local store allocates the next one.
 */
namespace PanelPortable
{
/** Fully validates a bundle without writing anything. */
QVariantMap inspect(const QUrl &url, const PanelAssets &assets, QString *error = nullptr);
/** Writes a self-contained bundle to the chosen local file. */
QVariantMap exportTo(const QUrl &url, const PanelStore &store, const PanelAssets &assets, QString *error = nullptr);
/** Validates, installs missing icons and replaces the configuration atomically. */
QVariantMap importFrom(const QUrl &url, PanelStore &store, const PanelAssets &assets, QString *error = nullptr);
}
}
