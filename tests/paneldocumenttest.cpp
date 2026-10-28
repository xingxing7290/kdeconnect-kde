/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>
#include <paneldocument.h>

using ControlPanel::PanelDocument;

class PanelDocumentTest : public QObject
{
    Q_OBJECT
private:
    QJsonObject fixture()
    {
        QFile file(QFINDTESTDATA("../docs/control-panel/fixtures/config-v1.json"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

private Q_SLOTS:
    void sharedFixtureRoundTrip()
    {
        const QJsonObject config = fixture();
        QVERIFY(!config.isEmpty());
        QString error;
        const auto document = PanelDocument::fromJson(QJsonDocument(config).toJson(), &error);
        QVERIFY2(document.has_value(), qPrintable(error));
        QCOMPARE(document->publicConfig(), config);
        QCOMPARE(document->revision(), 7);
        QCOMPARE(document->button(QStringLiteral("save")).value(QStringLiteral("kind")).toString(), QStringLiteral("shortcut"));
        QVERIFY(document->button(QStringLiteral("missing")).isEmpty());
        const auto empty = PanelDocument::empty();
        QVERIFY(PanelDocument::fromJson(QJsonDocument(empty.privateConfig()).toJson()).has_value());
    }

    void privateTargetsNeverLeaveDesktop()
    {
        QJsonObject config = fixture();
        QJsonArray pages = config.value(QStringLiteral("pages")).toArray();
        QJsonObject page = pages[0].toObject();
        QJsonArray buttonSlots = page.value(QStringLiteral("slots")).toArray();
        QJsonObject button = buttonSlots[0].toObject();
        button.insert(QStringLiteral("target"), QJsonObject{{QStringLiteral("path"), QStringLiteral("C:/private/program.exe")}});
        button.insert(QStringLiteral("futurePrivateField"), QStringLiteral("secret"));
        buttonSlots[0] = button;
        page.insert(QStringLiteral("slots"), buttonSlots);
        page.insert(QStringLiteral("privateField"), true);
        pages[0] = page;
        config.insert(QStringLiteral("pages"), pages);
        config.insert(QStringLiteral("privateField"), true);
        const auto document = PanelDocument::fromJson(QJsonDocument(config).toJson());
        QVERIFY(document.has_value());
        QVERIFY(document->button(QStringLiteral("docs")).contains(QStringLiteral("target")));
        QCOMPARE(document->publicConfig(), fixture());
    }

    void invalidDocuments_data()
    {
        QTest::addColumn<QJsonObject>("config");
        const auto add = [this](const char *name, const QString &key, const QJsonValue &value) {
            auto config = fixture();
            config.insert(key, value);
            QTest::newRow(name) << config;
        };
        add("future-version", QStringLiteral("schemaVersion"), 2);
        add("negative-revision", QStringLiteral("revision"), -1);
        add("fractional-revision", QStringLiteral("revision"), 7.5);
        add("large-revision", QStringLiteral("revision"), static_cast<double>(PanelDocument::MaxRevision) + 1);
        add("invalid-mode", QStringLiteral("mode"), QStringLiteral("other"));
        add("no-pages", QStringLiteral("pages"), QJsonArray{});
        add("wrong-page-type", QStringLiteral("pages"), QStringLiteral("pages"));
        add("unknown-home", QStringLiteral("homePageId"), QStringLiteral("missing"));
        add("unknown-active", QStringLiteral("activePageId"), QStringLiteral("missing"));
        auto page = fixture().value(QStringLiteral("pages")).toArray()[0].toObject();
        add("duplicate-page", QStringLiteral("pages"), QJsonArray{page, page});
        auto buttonSlots = page.value(QStringLiteral("slots")).toArray();
        buttonSlots.removeLast();
        page.insert(QStringLiteral("slots"), buttonSlots);
        add("missing-slot", QStringLiteral("pages"), QJsonArray{page});
        buttonSlots.append(buttonSlots[0]);
        page.insert(QStringLiteral("slots"), buttonSlots);
        add("duplicate-button", QStringLiteral("pages"), QJsonArray{page});
        auto appearance = fixture().value(QStringLiteral("appearance")).toObject();
        appearance.insert(QStringLiteral("showLabels"), QStringLiteral("true"));
        add("string-boolean", QStringLiteral("appearance"), appearance);
        auto idle = fixture().value(QStringLiteral("idle")).toObject();
        idle.insert(QStringLiteral("timeoutMs"), 999);
        add("short-timeout", QStringLiteral("idle"), idle);
    }

    void invalidDocuments()
    {
        QFETCH(QJsonObject, config);
        QString error;
        QVERIFY(!PanelDocument::fromJson(QJsonDocument(config).toJson(), &error).has_value());
        QVERIFY(!error.isEmpty());
    }

    void sizeLimitAndMaximumRevision()
    {
        QVERIFY(!PanelDocument::fromJson(QByteArray(PanelDocument::MaxBytes + 1, ' ')).has_value());
        QVERIFY(!PanelDocument::fromJson(QByteArrayLiteral("[]")).has_value());
        auto config = fixture();
        config.insert(QStringLiteral("revision"), PanelDocument::MaxRevision);
        const auto document = PanelDocument::fromJson(QJsonDocument(config).toJson());
        QVERIFY(document.has_value());
        QCOMPARE(document->revision(), PanelDocument::MaxRevision);
    }

    void deeplyNestedUnknownFieldsAreRejected()
    {
        QByteArray raw = QJsonDocument(fixture()).toJson(QJsonDocument::Compact);
        raw.chop(1);
        const QByteArray nested = raw + ",\"ignored\":" + QByteArray(64, '[') + "0" + QByteArray(64, ']') + "}";
        QString error;
        QVERIFY(!PanelDocument::fromJson(nested, &error));
        QCOMPARE(error, QStringLiteral("config_too_deep"));

        auto config = fixture();
        auto idle = config.value(QStringLiteral("idle")).toObject();
        const QString quoted = QString(120, QLatin1Char('[')) + QStringLiteral("\\\"");
        idle.insert(QStringLiteral("message"), quoted);
        config.insert(QStringLiteral("idle"), idle);
        QVERIFY(PanelDocument::fromJson(QJsonDocument(config).toJson()).has_value());
    }
};

QTEST_GUILESS_MAIN(PanelDocumentTest)
#include "paneldocumenttest.moc"
