/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "panelfavicon.h"
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QSharedPointer>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace ControlPanel
{
namespace
{
struct ScoredIcon
{
    QUrl url;
    int score;
};

QString attribute(const QRegularExpression &pattern, const QString &tag)
{
    const auto match = pattern.match(tag);
    if (!match.hasMatch())
        return {};
    for (int group = 1; group <= 3; ++group) {
        const auto value = match.captured(group);
        if (!value.isEmpty())
            return value;
    }
    return {};
}

int iconScore(const QString &rel, const QString &sizes)
{
    const auto normalized = rel.toLower();
    int score = -1;
    if (normalized.contains(QLatin1String("apple-touch-icon")))
        score = 300;
    else if (normalized.contains(QLatin1String("shortcut")))
        score = 180;
    else if (normalized.contains(QLatin1String("mask-icon")))
        score = 60;
    else if (normalized.contains(QLatin1String("icon")))
        score = 200;
    if (score < 0)
        return -1;
    static const QRegularExpression dimension(QString::fromUtf8(R"RE((\d{1,4})x(\d{1,4}))RE"));
    int largest = 0;
    auto matches = dimension.globalMatch(sizes);
    while (matches.hasNext())
        largest = std::max(largest, matches.next().captured(1).toInt());
    return score + std::min(largest, 256);
}

QList<QUrl> iconCandidates(const QByteArray &html, const QUrl &base)
{
    static const QRegularExpression linkTag(QString::fromUtf8(R"RE(<link\b[^>]*>)RE"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression relAttribute(QString::fromUtf8(R"RE(\brel\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))RE"),
                                                 QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression hrefAttribute(QString::fromUtf8(R"RE(\bhref\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))RE"),
                                                  QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression sizesAttribute(QString::fromUtf8(R"RE(\bsizes\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))RE"),
                                                   QRegularExpression::CaseInsensitiveOption);
    const QString head = QString::fromUtf8(html.left(131072));
    const auto headEnd = head.indexOf(QLatin1String("</head>"), 0, Qt::CaseInsensitive);
    const QString source = headEnd > 0 ? head.left(headEnd) : head;
    QList<ScoredIcon> scored;
    auto tags = linkTag.globalMatch(source);
    while (tags.hasNext()) {
        const QString tag = tags.next().captured(0);
        const int score = iconScore(attribute(relAttribute, tag), attribute(sizesAttribute, tag));
        auto href = attribute(hrefAttribute, tag).trimmed();
        href.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        if (score < 0 || href.isEmpty() || href.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive))
            continue;
        const QUrl candidate = base.resolved(QUrl(href, QUrl::StrictMode));
        if (!PanelFavicon::validRemote(candidate) || candidate.toString().size() > 2048)
            continue;
        const bool duplicate = std::any_of(scored.cbegin(), scored.cend(), [&candidate](const ScoredIcon &item) {
            return item.url == candidate;
        });
        if (!duplicate)
            scored.append({candidate, score});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const ScoredIcon &left, const ScoredIcon &right) {
        return left.score > right.score;
    });
    QList<QUrl> result;
    for (const auto &item : std::as_const(scored)) {
        result.append(item.url);
        if (result.size() >= PanelFavicon::MaxCandidates)
            break;
    }
    return result;
}

QImage decodeIcon(const QByteArray &bytes)
{
    static const QSet<QByteArray> formats{QByteArrayLiteral("png"), QByteArrayLiteral("jpeg"), QByteArrayLiteral("jpg"),
                                          QByteArrayLiteral("ico"), QByteArrayLiteral("x-icon"), QByteArrayLiteral("vnd.microsoft.icon"),
                                          QByteArrayLiteral("gif"), QByteArrayLiteral("webp"), QByteArrayLiteral("bmp"),
                                          QByteArrayLiteral("svg"), QByteArrayLiteral("svgz")};
    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly))
        return {};
    QImageReader reader(&buffer);
    if (!formats.contains(reader.format().toLower()))
        return {};
    const QSize declared = reader.size();
    if (declared.width() > 4096 || declared.height() > 4096)
        return {};
    reader.setAutoTransform(true);
    QImage best = reader.read();
    if (best.isNull())
        return {};
    // ICO files commonly ship several frames; keep the largest one.
    const int frames = reader.imageCount();
    for (int index = 1; index < frames && reader.jumpToImage(index); ++index) {
        const QImage frame = reader.read();
        if (!frame.isNull() && frame.width() * frame.height() > best.width() * best.height())
            best = frame;
    }
    if (best.width() < 16 || best.height() < 16 || best.width() > 4096 || best.height() > 4096)
        return {};
    return best;
}
}

PanelFavicon::PanelFavicon(const QString &directory, QObject *parent, QNetworkAccessManager *network, bool networking)
    : QObject(parent)
    , m_assets(directory)
    , m_network(network ? network : new QNetworkAccessManager(this))
    , m_networking(networking)
{
}

PanelFavicon::~PanelFavicon()
{
    for (const auto &reply : std::as_const(m_replies)) {
        if (reply)
            reply->abort();
    }
    for (const auto &context : std::as_const(m_contexts)) {
        if (context)
            delete context.data();
    }
}

QUrl PanelFavicon::normalize(const QString &value)
{
    QString text = value.trimmed();
    if (text.isEmpty() || text.size() > 2048)
        return {};
    if (!text.contains(QStringLiteral("://")))
        text.prepend(QStringLiteral("https://"));
    const QUrl url(text, QUrl::StrictMode);
    return validRemote(url) ? url : QUrl();
}

bool PanelFavicon::validRemote(const QUrl &url)
{
    return url.isValid() && !url.host().isEmpty()
        && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))
        && url.userInfo().isEmpty() && !url.path().contains(QChar::Null) && url.toString().size() <= 2048;
}

void PanelFavicon::fetch(const QUrl &page, Callback done)
{
    if (!m_networking) {
        done({}, QStringLiteral("favicon_disabled"));
        return;
    }
    if (!validRemote(page)) {
        done({}, QStringLiteral("invalid_url"));
        return;
    }
    QString origin = page.scheme() + QLatin1String("://") + page.host();
    if (page.port() >= 0)
        origin += QStringLiteral(":") + QString::number(page.port());
    const auto cached = m_cache.value(origin);
    if (!cached.isEmpty()) {
        if (m_assets.read(cached)) {
            done(cached, {});
            return;
        }
        m_cache.remove(origin);
    }

    auto *context = new QObject(this);
    m_contexts.append(QPointer<QObject>(context));
    const auto finish = [this, context, origin, done](const QString &iconId, const QString &error) {
        if (!iconId.isEmpty())
            m_cache.insert(origin, iconId);
        release(context, done, iconId, error);
    };
    get(context, page, MaxHtmlBytes, 0,
        [this, context, page, finish](const QByteArray &html, const QUrl &finalUrl, const QString &) {
            const QUrl root = validRemote(finalUrl) ? finalUrl : page;
            auto candidates = iconCandidates(html, root);
            QUrl fallback;
            fallback.setScheme(root.scheme());
            fallback.setHost(root.host());
            fallback.setPort(root.port());
            fallback.setPath(QStringLiteral("/favicon.ico"));
            if (validRemote(fallback) && !candidates.contains(fallback))
                candidates.append(fallback);
            tryCandidate(context, candidates, 0, finish);
        });
}

void PanelFavicon::get(QObject *context, const QUrl &url, qsizetype limit, int redirects, Response done)
{
    if (redirects > MaxRedirects || !validRemote(url)) {
        done({}, url, QStringLiteral("favicon_unavailable"));
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(8000);
    request.setRawHeader(QByteArrayLiteral("User-Agent"), QByteArrayLiteral("KDEConnect-ControlPanel/1 (favicon; personal non-commercial fork)"));
    auto *reply = m_network->get(request);
    m_replies.append(QPointer<QNetworkReply>(reply));
    auto *deadline = new QTimer(reply);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
    deadline->start(15000);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    auto bytes = QSharedPointer<QByteArray>::create();
    const auto collect = [reply, bytes, limit] {
        if (bytes->size() > limit)
            return;
        bytes->append(reply->read(static_cast<qint64>(limit + 1 - bytes->size())));
        if (bytes->size() > limit)
            reply->abort();
    };
    connect(reply, &QNetworkReply::readyRead, context, collect);
    connect(reply, &QNetworkReply::finished, context, [this, context, reply, bytes, limit, redirects, done, collect] {
        collect();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const QUrl finalUrl = reply->url();
        const bool redirect = status >= 300 && status < 400 && !target.isEmpty();
        const bool valid = reply->error() == QNetworkReply::NoError && status == 200 && !bytes->isEmpty() && bytes->size() <= limit;
        m_replies.removeAll(QPointer<QNetworkReply>(reply));
        reply->deleteLater();
        if (redirect) {
            get(context, finalUrl.resolved(target), limit, redirects + 1, done);
            return;
        }
        done(valid ? *bytes : QByteArray(), valid ? finalUrl : QUrl(), valid ? QString() : QStringLiteral("favicon_unavailable"));
    });
}

void PanelFavicon::tryCandidate(QObject *context, const QList<QUrl> &candidates, qsizetype index, Callback done)
{
    if (index >= candidates.size()) {
        done({}, QStringLiteral("favicon_unavailable"));
        return;
    }
    get(context, candidates.at(index), MaxIconBytes, 0,
        [this, context, candidates, index, done](const QByteArray &bytes, const QUrl &, const QString &error) {
            if (error.isEmpty() && !bytes.isEmpty()) {
                const QImage image = decodeIcon(bytes);
                if (!image.isNull()) {
                    QString assetError;
                    const auto id = m_assets.importPixels(image, &assetError);
                    if (!id.isEmpty()) {
                        done(id, {});
                        return;
                    }
                }
            }
            tryCandidate(context, candidates, index + 1, done);
        });
}

void PanelFavicon::release(QObject *context, const Callback &done, const QString &iconId, const QString &error)
{
    m_contexts.removeAll(QPointer<QObject>(context));
    context->deleteLater();
    done(iconId, error);
}
}
