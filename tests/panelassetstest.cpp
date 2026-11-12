/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>
#include <panelassets.h>
#include <paneleditor.h>
using namespace ControlPanel;

class PanelAssetsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void pngAndJpegAreNormalizedAndMetadataIsRemoved()
    {
        QTemporaryDir directory;
        PanelAssets assets(directory.path());
        QImage image(640, 320, QImage::Format_ARGB32);
        image.fill(QColor(32, 96, 192));
        image.setText(QStringLiteral("private"), QStringLiteral("secret-original-path"));
        const auto png = QDir(directory.path()).filePath(QStringLiteral("test with spaces.png"));
        const auto jpeg = QDir(directory.path()).filePath(QStringLiteral("test.jpeg"));
        QVERIFY(image.save(png, "PNG"));
        QVERIFY(image.save(jpeg, "JPEG"));
        QString error;
        const auto id = assets.importImage(QUrl::fromLocalFile(png), &error);
        QVERIFY2(PanelAssets::validId(id), qPrintable(error));
        QCOMPARE(assets.importImage(QUrl::fromLocalFile(png)), id);
        const auto bytes = assets.read(id);
        QVERIFY(bytes);
        QCOMPARE(QCryptographicHash::hash(*bytes, QCryptographicHash::Sha256).toHex(), id.toLatin1());
        QVERIFY(!bytes->contains("secret-original-path"));
        const auto decoded = QImage::fromData(*bytes, "PNG");
        QCOMPARE(decoded.size(), QSize(256, 256));
        QCOMPARE(decoded.pixelColor(128, 0).alpha(), 0);
        QVERIFY(!assets.importImage(QUrl::fromLocalFile(jpeg)).isEmpty());
        PanelAssets restarted(directory.path());
        QCOMPARE(restarted.read(id), bytes);
    }
    void invalidOrOversizedSourcesNeverBecomeAssets()
    {
        QTemporaryDir directory;
        PanelAssets assets(directory.path());
        QVERIFY(assets.importImage(QUrl(QStringLiteral("https://example.com/image.png"))).isEmpty());
        QImage huge(4097, 1, QImage::Format_ARGB32);
        huge.fill(Qt::red);
        const auto path = QDir(directory.path()).filePath(QStringLiteral("bad.png"));
        QVERIFY(huge.save(path, "PNG"));
        QVERIFY(assets.importImage(QUrl::fromLocalFile(path)).isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not an image");
        file.close();
        QVERIFY(assets.importImage(QUrl::fromLocalFile(path)).isEmpty());
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(PanelAssets::MaxImportBytes + 1));
        file.close();
        QVERIFY(assets.importImage(QUrl::fromLocalFile(path)).isEmpty());
        QVERIFY(!assets.read(QStringLiteral("../../private")));
        QVERIFY(assets.url(QStringLiteral("file:///private")).isEmpty());
    }
    void corruptCacheIsNotPublishedAndCanBeReimported()
    {
        QTemporaryDir directory;
        PanelAssets assets(directory.path());
        QImage image(32, 32, QImage::Format_ARGB32);
        image.fill(Qt::green);
        const auto id = assets.importPixels(image);
        QVERIFY(!id.isEmpty());
        QFile file(assets.url(id).toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("corrupt");
        file.close();
        QVERIFY(!assets.read(id));
        QCOMPARE(assets.importPixels(image), id);
        QVERIFY(assets.read(id));
    }
    void iconImportDoesNotChangeConfigUntilButtonIsSaved()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        const auto original = editor.config();
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::blue);
        const auto path = QDir(directory.path()).filePath(QStringLiteral("custom.png"));
        QVERIFY(image.save(path));
        const auto id = editor.importIcon(QUrl::fromLocalFile(path));
        QVERIFY(!id.isEmpty());
        QCOMPARE(editor.config(), original);
        QVERIFY(editor.setButton(0,
                                 {{QStringLiteral("kind"), QStringLiteral("website")},
                                  {QStringLiteral("name"), QStringLiteral("Example")},
                                  {QStringLiteral("icon"), id},
                                  {QStringLiteral("target"), QVariantMap{{QStringLiteral("url"), QStringLiteral("https://example.com")}}}}));
        const auto button = editor.currentPage().value(QStringLiteral("slots")).toList()[0].toMap();
        QCOMPARE(button.value(QStringLiteral("icon")).toString(), id);
        QVERIFY(!editor.assetUrl(id).isEmpty());
    }
#ifdef Q_OS_WIN
    void nativeProgramIconIsExtractedWithoutLaunchingProgram()
    {
        QTemporaryDir directory;
        PanelAssets assets(directory.path());
        const auto program = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("kdeconnect-app.exe"));
        QVERIFY(QFile::exists(program));
        const auto id = assets.importProgramIcon(QUrl::fromLocalFile(program));
        QVERIFY(!id.isEmpty());
        QVERIFY(assets.read(id));
        const auto copy = QDir(directory.path()).filePath(QStringLiteral("应用 # test.exe"));
        QVERIFY(QFile::copy(program, copy));
        PanelEditor editor(directory.path());
        QVERIFY(editor.setButton(0,
                                 {{QStringLiteral("name"), QStringLiteral("Program")},
                                  {QStringLiteral("kind"), QStringLiteral("program")},
                                  {QStringLiteral("target"), QVariantMap{{QStringLiteral("path"), copy}}}}));
        QCOMPARE(editor.currentPage().value(QStringLiteral("slots")).toList()[0].toMap().value(QStringLiteral("icon")).toString(), id);
        QVERIFY(assets.importProgramIcon(QUrl::fromLocalFile(QStringLiteral("\\\\example.invalid\\app.exe"))).isEmpty());
    }
#endif
};
QTEST_GUILESS_MAIN(PanelAssetsTest)
#include "panelassetstest.moc"
