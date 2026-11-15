/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QBuffer>
#include <QEventLoop>
#include <QImage>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <panelassets.h>
#include <panelfavicon.h>
#include <algorithm>
#include <cstring>

using ControlPanel::PanelAssets;
using ControlPanel::PanelFavicon;

struct FaviconResponse
{
    QByteArray bytes;
    int status = 200;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QUrl redirect;
};

static FaviconResponse response(const QByteArray &bytes, int status = 200)
{
    FaviconResponse result;
    result.bytes = bytes;
    result.status = status;
    return result;
}

class FaviconReply : public QNetworkReply
{
public:
    FaviconReply(const QNetworkRequest &request, FaviconResponse data, QObject *parent)
        : QNetworkReply(parent)
        , m_response(std::move(data))
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, m_response.status);
        if (!m_response.redirect.isEmpty())
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, m_response.redirect);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] {
            if (isFinished())
                return;
            if (m_response.error != NoError)
                setError(m_response.error, QStringLiteral("fixture error"));
            Q_EMIT readyRead();
            if (!isFinished()) {
                setFinished(true);
                Q_EMIT finished();
            }
        });
    }
    void abort() override
    {
        if (!isFinished()) {
            setError(OperationCanceledError, QStringLiteral("aborted"));
            setFinished(true);
            Q_EMIT finished();
        }
    }
    qint64 bytesAvailable() const override
    {
        return m_response.bytes.size() - m_offset + QNetworkReply::bytesAvailable();
    }
    bool isSequential() const override
    {
        return true;
    }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const auto count = std::min<qint64>(maximum, m_response.bytes.size() - m_offset);
        if (count <= 0)
            return -1;
        memcpy(data, m_response.bytes.constData() + m_offset, count);
        m_offset += count;
        return count;
    }

private:
    FaviconResponse m_response;
    qint64 m_offset = 0;
};

class FaviconNetwork : public QNetworkAccessManager
{
public:
    QList<QUrl> requests;
    QHash<QString, FaviconResponse> responses;

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        requests.append(request.url());
        const auto found = responses.constFind(request.url().toString());
        return new FaviconReply(request, found == responses.constEnd() ? response(QByteArrayLiteral("{}"), 404) : *found, this);
    }
};

class PanelFaviconTest : public QObject
{
    Q_OBJECT
private:
    static QByteArray png(int size = 64)
    {
        QImage image(size, size, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return bytes;
    }
    struct FetchResult
    {
        QString id;
        QString error;
    };
    static FetchResult fetch(PanelFavicon &favicon, const QUrl &url)
    {
        FetchResult result;
        bool finished = false;
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        favicon.fetch(url, [&](const QString &id, const QString &error) {
            result = {id, error};
            finished = true;
            loop.quit();
        });
        if (!finished) {
            deadline.start(5000);
            loop.exec();
        }
        return result;
    }
    static void verifyAsset(const QString &directory, const QString &id)
    {
        QVERIFY(PanelAssets::validId(id));
        PanelAssets assets(directory);
        const auto bytes = assets.read(id);
        QVERIFY(bytes.has_value());
        QBuffer buffer;
        buffer.setData(*bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, QByteArrayLiteral("png"));
        QCOMPARE(reader.size(), QSize(PanelAssets::IconSize, PanelAssets::IconSize));
        QVERIFY(!reader.read().isNull());
    }

private Q_SLOTS:
    void declaredIconIsPreferredAndNormalized()
    {
        QTemporaryDir directory;
        FaviconNetwork network;
        network.responses.insert(QStringLiteral("https://example.org/"),
                                 response(QByteArrayLiteral("<html><head><link rel=\"icon\" sizes=\"64x64\" href=\"/brand.png\">"
                                                            "<link rel=\"apple-touch-icon\" href=\"/touch.png\"></head><body>home</body></html>")));
        network.responses.insert(QStringLiteral("https://example.org/brand.png"), response(png(64)));
        network.responses.insert(QStringLiteral("https://example.org/touch.png"), response(png(180)));
        PanelFavicon favicon(directory.path(), nullptr, &network, true);
        const auto result = fetch(favicon, QUrl(QStringLiteral("https://example.org/")));
        QVERIFY(result.error.isEmpty());
        QVERIFY(!result.id.isEmpty());
        verifyAsset(directory.path(), result.id);
        QVERIFY(network.requests.contains(QUrl(QStringLiteral("https://example.org/touch.png"))));
        QVERIFY(!network.requests.contains(QUrl(QStringLiteral("https://example.org/brand.png"))));
    }
    void missingLinkFallsBackToRootFavicon()
    {
        QTemporaryDir directory;
        FaviconNetwork network;
        network.responses.insert(QStringLiteral("https://example.org/path"), response(QByteArrayLiteral("<html><head><title>none</title></head></html>")));
        network.responses.insert(QStringLiteral("https://example.org/favicon.ico"), response(png(32)));
        PanelFavicon favicon(directory.path(), nullptr, &network, true);
        const auto result = fetch(favicon, QUrl(QStringLiteral("https://example.org/path")));
        QVERIFY(result.error.isEmpty());
        verifyAsset(directory.path(), result.id);
        QVERIFY(network.requests.contains(QUrl(QStringLiteral("https://example.org/favicon.ico"))));
    }
    void brokenCandidateDoesNotBreakTheFallback()
    {
        QTemporaryDir directory;
        FaviconNetwork network;
        network.responses.insert(QStringLiteral("https://example.org/"),
                                 response(QByteArrayLiteral("<html><head><link rel=\"icon\" href=\"/broken.png\"></head></html>")));
        network.responses.insert(QStringLiteral("https://example.org/broken.png"), response(QByteArrayLiteral("not an image")));
        network.responses.insert(QStringLiteral("https://example.org/favicon.ico"), response(png(48)));
        PanelFavicon favicon(directory.path(), nullptr, &network, true);
        const auto result = fetch(favicon, QUrl(QStringLiteral("https://example.org/")));
        QVERIFY(result.error.isEmpty());
        verifyAsset(directory.path(), result.id);
    }
    void oversizedImageIsAbortedWithoutImport()
    {
        QTemporaryDir directory;
        FaviconNetwork network;
        network.responses.insert(QStringLiteral("https://example.org/"),
                                 response(QByteArrayLiteral("<html><head><link rel=\"icon\" href=\"/huge.png\"></head></html>")));
        network.responses.insert(QStringLiteral("https://example.org/huge.png"),
                                 response(QByteArray(static_cast<int>(PanelFavicon::MaxIconBytes) + 64, 'x')));
        PanelFavicon favicon(directory.path(), nullptr, &network, true);
        const auto result = fetch(favicon, QUrl(QStringLiteral("https://example.org/")));
        QVERIFY(result.id.isEmpty());
        QCOMPARE(result.error, QStringLiteral("favicon_unavailable"));
    }
    void originCacheAvoidsRepeatedRequests()
    {
        QTemporaryDir directory;
        FaviconNetwork network;
        network.responses.insert(QStringLiteral("https://example.org/"), response(QByteArrayLiteral("<html><head></head></html>")));
        network.responses.insert(QStringLiteral("https://example.org/docs"), response(QByteArrayLiteral("<html><head></head></html>")));
        network.responses.insert(QStringLiteral("https://example.org/favicon.ico"), response(png(40)));
        PanelFavicon favicon(directory.path(), nullptr, &network, true);
        const auto first = fetch(favicon, QUrl(QStringLiteral("https://example.org/")));
        QVERIFY(!first.id.isEmpty());
        const auto count = network.requests.size();
        const auto second = fetch(favicon, QUrl(QStringLiteral("https://example.org/docs")));
        QCOMPARE(second.id, first.id);
        QVERIFY(second.error.isEmpty());
        QCOMPARE(network.requests.size(), count);
    }
    void redirectIsFollowedBeforeParsing()
    {
        QTemporaryDir directory;
        FaviconNetwork network;
        FaviconResponse moved = response(QByteArray(), 302);
        moved.redirect = QUrl(QStringLiteral("https://www.example.org/"));
        network.responses.insert(QStringLiteral("https://example.org/"), moved);
        network.responses.insert(QStringLiteral("https://www.example.org/"),
                                 response(QByteArrayLiteral("<html><head><link rel=\"icon\" href=\"/icon.png\"></head></html>")));
        network.responses.insert(QStringLiteral("https://www.example.org/icon.png"), response(png(64)));
        PanelFavicon favicon(directory.path(), nullptr, &network, true);
        const auto result = fetch(favicon, QUrl(QStringLiteral("https://example.org/")));
        QVERIFY(result.error.isEmpty());
        verifyAsset(directory.path(), result.id);
    }
    void invalidUrlsAndDisabledNetworkingNeverReachTheNetwork()
    {
        QTemporaryDir directory;
        FaviconNetwork network;
        PanelFavicon disabled(directory.path(), nullptr, &network, false);
        QCOMPARE(fetch(disabled, QUrl(QStringLiteral("https://example.org/"))).error, QStringLiteral("favicon_disabled"));
        QVERIFY(network.requests.isEmpty());
        PanelFavicon enabled(directory.path(), nullptr, &network, true);
        QCOMPARE(fetch(enabled, QUrl(QStringLiteral("file:///C:/Windows/notepad.exe"))).error, QStringLiteral("invalid_url"));
        QVERIFY(network.requests.isEmpty());
        QVERIFY(PanelFavicon::normalize(QStringLiteral("example.com")).host() == QLatin1String("example.com"));
        QVERIFY(PanelFavicon::normalize(QStringLiteral("https://user:pass@example.com")).isEmpty());
        QVERIFY(PanelFavicon::normalize(QStringLiteral("   ")).isEmpty());
        QVERIFY(PanelFavicon::normalize({}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(PanelFaviconTest)
#include "panelfavicontest.moc"
