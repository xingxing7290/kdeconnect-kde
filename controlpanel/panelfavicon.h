/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include "panelassets.h"
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace ControlPanel
{
/**
 * Resolves a website favicon into a normalized control-panel PNG asset.
 *
 * Every fetch owns a short-lived QObject context, so a preview fetch and a
 * background fetch can run at the same time. Destroying the resolver aborts
 * outstanding replies; callbacks are never invoked after destruction.
 */
class PanelFavicon : public QObject
{
public:
    using Callback = std::function<void(const QString &iconId, const QString &error)>;

    static constexpr qsizetype MaxHtmlBytes = 256 * 1024;
    static constexpr qsizetype MaxIconBytes = 2 * 1024 * 1024;
    static constexpr int MaxCandidates = 6;
    static constexpr int MaxRedirects = 3;

    explicit PanelFavicon(const QString &directory,
                          QObject *parent = nullptr,
                          QNetworkAccessManager *network = nullptr,
                          bool networking = false);
    ~PanelFavicon() override;

    /** Accepts "example.com" as well as a full URL; returns an empty QUrl for anything unsafe. */
    static QUrl normalize(const QString &value);
    static bool validRemote(const QUrl &url);

    void fetch(const QUrl &page, Callback done);

private:
    using Response = std::function<void(const QByteArray &bytes, const QUrl &finalUrl, const QString &error)>;
    void get(QObject *context, const QUrl &url, qsizetype limit, int redirects, Response done);
    void tryCandidate(QObject *context, const QList<QUrl> &candidates, qsizetype index, Callback done);
    void release(QObject *context, const Callback &done, const QString &iconId, const QString &error);

    PanelAssets m_assets;
    QNetworkAccessManager *m_network;
    bool m_networking;
    QHash<QString, QString> m_cache;
    QList<QPointer<QObject>> m_contexts;
    QList<QPointer<QNetworkReply>> m_replies;
};
}
