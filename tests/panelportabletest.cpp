/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <memory>
#include <panelassets.h>
#include <paneldocument.h>
#include <panelportable.h>
#include <panelstore.h>

using namespace ControlPanel;

class PanelPortableTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> directory;

    QString configDir() const
    {
        return QDir(directory->path()).filePath(QStringLiteral("config"));
    }
    QString bundlePath() const
    {
        return QDir(directory->path()).filePath(QStringLiteral("backup.kdpanel.json"));
    }
    QUrl bundleUrl() const
    {
        return QUrl::fromLocalFile(bundlePath());
    }
    QJsonArray emptySlots() const
    {
        QJsonArray buttonSlots;
        for (int index = 0; index < PanelDocument::SlotCount; ++index)
            buttonSlots.append(QJsonValue::Null);
        return buttonSlots;
    }
    QJsonObject button(const QString &id, const QString &kind, const QString &icon) const
    {
        QJsonObject target;
        if (kind == QLatin1String("website"))
            target.insert(QStringLiteral("url"), QString(QStringLiteral("https://example.com/") + id));
        else
            target.insert(QStringLiteral("path"), QString(QStringLiteral("C:/Apps/") + id + QStringLiteral(".exe")));
        return QJsonObject{{QStringLiteral("id"), id},
                           {QStringLiteral("name"), QString(QStringLiteral("Btn ") + id)},
                           {QStringLiteral("kind"), kind},
                           {QStringLiteral("icon"), icon},
                           {QStringLiteral("target"), target}};
    }
    /** Two pages, four buttons: website + program + file with icons, one icon-less system button. */
    QJsonObject makeConfig(const QStringList &icons) const
    {
        auto config = PanelDocument::empty().privateConfig();
        const QString home = config.value(QStringLiteral("homePageId")).toString();
        QJsonArray first = emptySlots();
        first[0] = button(QStringLiteral("web"), QStringLiteral("website"), icons.value(0));
        first[1] = button(QStringLiteral("app"), QStringLiteral("program"), icons.value(1));
        QJsonArray second = emptySlots();
        second[0] = button(QStringLiteral("doc"), QStringLiteral("file"), icons.value(2));
        second[1] = button(QStringLiteral("plain"), QStringLiteral("system"), QString());
        QJsonArray pages;
        pages.append(QJsonObject{{QStringLiteral("id"), home},
                                 {QStringLiteral("name"), QStringLiteral("主页")},
                                 {QStringLiteral("application"), QString()},
                                 {QStringLiteral("slots"), first}});
        pages.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("page-two")},
                                 {QStringLiteral("name"), QStringLiteral("第二页")},
                                 {QStringLiteral("application"), QString()},
                                 {QStringLiteral("slots"), second}});
        config.insert(QStringLiteral("pages"), pages);
        config.insert(QStringLiteral("mode"), QStringLiteral("pages"));
        config.insert(QStringLiteral("activePageId"), QStringLiteral("page-two"));
        return config;
    }
    QString makeIcon(const PanelAssets &assets, int shade, QString *error) const
    {
        QImage image(80, 80, QImage::Format_ARGB32);
        image.fill(QColor(20 + shade * 40, 90, 180));
        return assets.importPixels(image, error);
    }
    QJsonObject readBundle() const
    {
        QFile file(bundlePath());
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }
    bool writeBundle(const QJsonObject &root) const
    {
        QFile file(bundlePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) > 0;
    }
    /** Creates a populated source store and exports it; returns void so QVERIFY stays usable. */
    void prepareSource(PanelStore *store, PanelAssets *assets, QJsonObject *config, QStringList *icons = nullptr)
    {
        QVERIFY(store->load());
        QString error;
        QStringList generated;
        for (int shade = 1; shade <= 3; ++shade) {
            const QString id = makeIcon(*assets, shade, &error);
            QVERIFY2(PanelAssets::validId(id), qPrintable(error));
            generated.append(id);
        }
        *config = makeConfig(generated);
        QVERIFY(store->save(*config, store->document().revision()));
        *config = store->document().privateConfig();
        if (icons)
            *icons = generated;
        QVERIFY(PanelPortable::exportTo(bundleUrl(), *store, *assets).value(QStringLiteral("ok")).toBool());
    }

private Q_SLOTS:
    void init()
    {
        directory = std::make_unique<QTemporaryDir>();
        QVERIFY(directory->isValid());
    }
    void cleanup()
    {
        directory.reset();
    }

    void roundTripPreservesAllPagesButtonsAndIcons()
    {
        PanelStore store(configDir());
        PanelAssets assets(configDir());
        QJsonObject expected;
        QStringList icons;
        prepareSource(&store, &assets, &expected, &icons);
        const qint64 exportedRevision = store.document().revision();
        QVERIFY(exportedRevision > 0);

        const QJsonObject root = readBundle();
        QCOMPARE(root.value(QStringLiteral("format")).toString(), QStringLiteral("kdeconnect-controlpanel"));
        QCOMPARE(root.value(QStringLiteral("formatVersion")).toInt(), 1);
        QCOMPARE(root.value(QStringLiteral("assets")).toArray().size(), 3);
        QCOMPARE(root.value(QStringLiteral("config")).toObject().value(QStringLiteral("pages")).toArray().size(), 2);
        QCOMPARE(root.value(QStringLiteral("summary")).toObject().value(QStringLiteral("buttons")).toInt(), 4);

        const auto inspected = PanelPortable::inspect(bundleUrl(), assets);
        QVERIFY2(inspected.value(QStringLiteral("ok")).toBool(), qPrintable(inspected.value(QStringLiteral("error")).toString()));
        QCOMPARE(inspected.value(QStringLiteral("pages")).toInt(), 2);
        QCOMPARE(inspected.value(QStringLiteral("buttons")).toInt(), 4);
        QCOMPARE(inspected.value(QStringLiteral("assets")).toInt(), 3);
        QCOMPARE(inspected.value(QStringLiteral("pageDetails")).toList().size(), 2);

        const QString target = QDir(directory->path()).filePath(QStringLiteral("restored"));
        PanelStore destination(target);
        PanelAssets destinationAssets(target);
        QVERIFY(destination.load());
        const qint64 before = destination.document().revision();
        const auto imported = PanelPortable::importFrom(bundleUrl(), destination, destinationAssets);
        QVERIFY2(imported.value(QStringLiteral("ok")).toBool(), qPrintable(imported.value(QStringLiteral("error")).toString()));
        QCOMPARE(destination.document().revision(), before + 1);
        QJsonObject actual = destination.document().privateConfig();
        QJsonObject reference = expected;
        actual.insert(QStringLiteral("revision"), 0);
        reference.insert(QStringLiteral("revision"), 0);
        QCOMPARE(QJsonDocument(actual).toJson(QJsonDocument::Compact), QJsonDocument(reference).toJson(QJsonDocument::Compact));
        for (const QString &id : icons)
            QVERIFY(destinationAssets.read(id).has_value());
    }

    void tamperedIconIsRejectedWithoutTouchingTheStore()
    {
        PanelStore store(configDir());
        PanelAssets assets(configDir());
        QJsonObject config;
        prepareSource(&store, &assets, &config);

        QJsonObject root = readBundle();
        QJsonArray assetArray = root.value(QStringLiteral("assets")).toArray();
        QJsonObject first = assetArray[0].toObject();
        QByteArray decoded = QByteArray::fromBase64(first.value(QStringLiteral("data")).toString().toLatin1());
        decoded[20] = char(decoded.at(20) ^ 0x01);
        first.insert(QStringLiteral("data"), QString::fromLatin1(decoded.toBase64()));
        assetArray[0] = first;
        root.insert(QStringLiteral("assets"), assetArray);
        QVERIFY(writeBundle(root));
        QCOMPARE(PanelPortable::inspect(bundleUrl(), assets).value(QStringLiteral("error")).toString(),
                 QStringLiteral("asset_hash_mismatch"));

        const QString target = QDir(directory->path()).filePath(QStringLiteral("restored"));
        PanelStore destination(target);
        PanelAssets destinationAssets(target);
        QVERIFY(destination.load());
        const QJsonObject before = destination.document().privateConfig();
        const auto result = PanelPortable::importFrom(bundleUrl(), destination, destinationAssets);
        QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("asset_hash_mismatch"));
        QCOMPARE(QJsonDocument(destination.document().privateConfig()).toJson(QJsonDocument::Compact),
                 QJsonDocument(before).toJson(QJsonDocument::Compact));
    }

    void missingReferencedIconIsRejectedOnAFreshMachine()
    {
        PanelStore store(configDir());
        PanelAssets assets(configDir());
        QJsonObject config;
        QStringList icons;
        prepareSource(&store, &assets, &config, &icons);

        QJsonObject root = readBundle();
        QJsonArray assetArray = root.value(QStringLiteral("assets")).toArray();
        for (qsizetype index = 0; index < assetArray.size(); ++index) {
            if (assetArray[index].toObject().value(QStringLiteral("id")).toString() == icons.first()) {
                assetArray.removeAt(index);
                break;
            }
        }
        root.insert(QStringLiteral("assets"), assetArray);
        QVERIFY(writeBundle(root));

        const QString target = QDir(directory->path()).filePath(QStringLiteral("restored"));
        PanelStore destination(target);
        PanelAssets destinationAssets(target);
        QVERIFY(destination.load());
        QCOMPARE(PanelPortable::inspect(bundleUrl(), destinationAssets).value(QStringLiteral("error")).toString(),
                 QStringLiteral("asset_missing_in_bundle"));
        QCOMPARE(PanelPortable::importFrom(bundleUrl(), destination, destinationAssets).value(QStringLiteral("error")).toString(),
                 QStringLiteral("asset_missing_in_bundle"));
    }

    void rejectsWrongFormatVersionAndMissingSections()
    {
        PanelStore store(configDir());
        PanelAssets assets(configDir());
        QJsonObject config;
        prepareSource(&store, &assets, &config);

        QJsonObject root = readBundle();
        root.insert(QStringLiteral("format"), QStringLiteral("not-a-panel-bundle"));
        QVERIFY(writeBundle(root));
        QCOMPARE(PanelPortable::inspect(bundleUrl(), assets).value(QStringLiteral("error")).toString(),
                 QStringLiteral("unsupported_bundle"));

        root.insert(QStringLiteral("format"), QStringLiteral("kdeconnect-controlpanel"));
        root.insert(QStringLiteral("formatVersion"), 2);
        QVERIFY(writeBundle(root));
        QCOMPARE(PanelPortable::inspect(bundleUrl(), assets).value(QStringLiteral("error")).toString(),
                 QStringLiteral("unsupported_bundle_version"));

        root.insert(QStringLiteral("formatVersion"), 1);
        root.insert(QStringLiteral("config"), QStringLiteral("broken"));
        QVERIFY(writeBundle(root));
        QCOMPARE(PanelPortable::inspect(bundleUrl(), assets).value(QStringLiteral("error")).toString(),
                 QStringLiteral("bundle_missing_config"));
    }

    void handEditedRevisionIsIgnored()
    {
        PanelStore store(configDir());
        PanelAssets assets(configDir());
        QJsonObject config;
        prepareSource(&store, &assets, &config);

        QJsonObject root = readBundle();
        QJsonObject documentConfig = root.value(QStringLiteral("config")).toObject();
        documentConfig.insert(QStringLiteral("revision"), QStringLiteral("edited-by-hand"));
        root.insert(QStringLiteral("config"), documentConfig);
        QVERIFY(writeBundle(root));

        const QString target = QDir(directory->path()).filePath(QStringLiteral("restored"));
        PanelStore destination(target);
        PanelAssets destinationAssets(target);
        QVERIFY(destination.load());
        const qint64 before = destination.document().revision();
        const auto result = PanelPortable::importFrom(bundleUrl(), destination, destinationAssets);
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(), qPrintable(result.value(QStringLiteral("error")).toString()));
        QCOMPARE(destination.document().revision(), before + 1);
        QCOMPARE(destination.document().privateConfig().value(QStringLiteral("mode")).toString(), QStringLiteral("pages"));
    }

    void exportRefusesBrokenLocalIcon()
    {
        PanelStore store(configDir());
        PanelAssets assets(configDir());
        QJsonObject config;
        QStringList icons;
        prepareSource(&store, &assets, &config, &icons);
        QVERIFY(QFile::remove(QDir(configDir()).filePath(QStringLiteral("assets/") + icons.first() + QStringLiteral(".png"))));
        const auto result = PanelPortable::exportTo(QUrl::fromLocalFile(bundlePath()), store, assets);
        QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("asset_missing"));
    }

    void installRejectsMismatchedHashAndIsIdempotent()
    {
        PanelAssets assets(configDir());
        QString error;
        const QString id = makeIcon(assets, 1, &error);
        const auto bytes = assets.read(id);
        QVERIFY(bytes.has_value());
        const QString wrong = QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000");
        QVERIFY(!assets.install(wrong, *bytes, &error));
        QCOMPARE(error, QStringLiteral("asset_hash_mismatch"));
        QVERIFY(assets.install(id, *bytes, &error));
        QVERIFY(PanelAssets::validPng(*bytes));
        QVERIFY(!PanelAssets::validPng(QByteArrayLiteral("not-a-png")));
    }

    void exportGuardsTargetPathAndAddsSuffix()
    {
        PanelStore store(configDir());
        PanelAssets assets(configDir());
        QJsonObject config;
        prepareSource(&store, &assets, &config);

        QCOMPARE(PanelPortable::exportTo(QUrl::fromLocalFile(store.filePath()), store, assets).value(QStringLiteral("error")).toString(),
                 QStringLiteral("invalid_target"));
        const QString plain = QDir(directory->path()).filePath(QStringLiteral("panel-backup"));
        const auto result = PanelPortable::exportTo(QUrl::fromLocalFile(plain), store, assets);
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(), qPrintable(result.value(QStringLiteral("error")).toString()));
        QCOMPARE(result.value(QStringLiteral("path")).toString(), plain + QStringLiteral(".kdpanel.json"));
        QVERIFY(QFile::exists(plain + QStringLiteral(".kdpanel.json")));
    }
};

QTEST_GUILESS_MAIN(PanelPortableTest)
#include "panelportabletest.moc"
