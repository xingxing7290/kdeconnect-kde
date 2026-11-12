/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QTemporaryDir>
#include <QTest>
#include <paneleditor.h>

class PanelDialsUiTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void selectionSettingsAndPreviewsUseProductionQml()
    {
        QTest::failOnWarning();
        QQuickStyle::setStyle(QStringLiteral("Basic"));
#ifdef Q_OS_WIN
        QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc")) >= 0);
        QGuiApplication::setFont(QFont(QStringLiteral("Microsoft YaHei")));
#endif
        QTemporaryDir directory;
        ControlPanel::PanelEditor editor(directory.path());
        QQuickView view;
        view.setInitialProperties({{QStringLiteral("editor"), QVariant::fromValue(&editor)}});
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../app/qml/PanelDialGallery.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(1000, 780);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto find = [&](const QString &name) {
            return view.rootObject()->findChild<QObject *>(name);
        };
        auto click = [&](const QString &name) {
            auto *item = qobject_cast<QQuickItem *>(find(name));
            QVERIFY2(item, qPrintable(name));
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        };
        const auto original = editor.config();
        click(QStringLiteral("panelDialFeatured"));
        auto *confirmation = find(QStringLiteral("panelDialConfirmation"));
        QVERIFY(confirmation);
        QTRY_VERIFY(confirmation->property("visible").toBool());
        click(QStringLiteral("panelDialSelectCancel"));
        QTRY_VERIFY(!confirmation->property("visible").toBool());
        QCOMPARE(editor.config(), original);
        click(QStringLiteral("panelDialFeatured"));
        QTRY_VERIFY(confirmation->property("visible").toBool());
        click(QStringLiteral("panelDialSelectSave"));
        QTRY_VERIFY(!confirmation->property("visible").toBool());
        auto *current = find(QStringLiteral("panelDialCurrent"));
        QVERIFY(current);
        QTRY_COMPARE(current->property("dialId").toString(), QStringLiteral("analog"));
        const auto afterSelection = editor.config();
        click(QStringLiteral("panelIdleEnabled"));
        QTRY_COMPARE(editor.config().value(QStringLiteral("idle")).toMap().value(QStringLiteral("enabled")).toBool(), false);
        click(QStringLiteral("panelDialSettingsButton"));
        auto *settings = find(QStringLiteral("panelDialSettings"));
        auto *city = find(QStringLiteral("panelDialCity"));
        auto *message = find(QStringLiteral("panelDialMessage"));
        QVERIFY(settings && city && message);
        QTRY_VERIFY(settings->property("visible").toBool());
        city->setProperty("text", QStringLiteral("Not saved"));
        click(QStringLiteral("panelDialSettingsCancel"));
        QTRY_VERIFY(!settings->property("visible").toBool());
        QCOMPARE(editor.config().value(QStringLiteral("idle")).toMap().value(QStringLiteral("city")),
                 afterSelection.value(QStringLiteral("idle")).toMap().value(QStringLiteral("city")));
        click(QStringLiteral("panelDialSettingsButton"));
        QTRY_VERIFY(settings->property("visible").toBool());
        city->setProperty("text", QStringLiteral("长沙市"));
        message->setProperty("text", QStringLiteral("Stay curious"));
        const auto revision = editor.config().value(QStringLiteral("revision")).toLongLong();
        click(QStringLiteral("panelDialSettingsSave"));
        QTRY_VERIFY(!settings->property("visible").toBool());
        QCOMPARE(editor.config().value(QStringLiteral("revision")).toLongLong(), revision + 1);
        QCOMPARE(editor.config().value(QStringLiteral("idle")).toMap().value(QStringLiteral("city")).toString(), QStringLiteral("长沙市"));
        QCOMPARE(editor.config().value(QStringLiteral("idle")).toMap().value(QStringLiteral("message")).toString(), QStringLiteral("Stay curious"));
        const auto screenshots = qEnvironmentVariable("KDECONNECT_PANEL_SCREENSHOTS");
        for (const auto &dial : {"digital", "analog", "weather", "info", "car"}) {
            QVERIFY(editor.setIdle(QStringLiteral("dialId"), QString::fromLatin1(dial)));
            QTRY_COMPARE(current->property("dialId").toString(), QString::fromLatin1(dial));
            QTest::qWait(50);
            const auto image = view.grabWindow();
            QVERIFY(!image.isNull());
            if (!screenshots.isEmpty()) {
                QVERIFY(QDir().mkpath(screenshots));
                QVERIFY(image.save(QDir(screenshots).filePath(QStringLiteral("desktop-dial-%1.png").arg(QString::fromLatin1(dial)))));
            }
        }
        ControlPanel::PanelEditor restarted(directory.path());
        QCOMPARE(restarted.config(), editor.config());
        const auto liveFixture = qEnvironmentVariable("KDECONNECT_WEATHER_LIVE_FIXTURE");
        if (!liveFixture.isEmpty()) {
            QFile file(liveFixture);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto observation = QJsonDocument::fromJson(file.read(16385)).object();
            QVERIFY(ControlPanel::PanelWeather::validObservation(observation, QDateTime::currentSecsSinceEpoch()));
            QFile cache(directory.filePath(QStringLiteral("weather.json")));
            QVERIFY(cache.open(QIODevice::WriteOnly));
            const auto bytes = QJsonDocument(observation).toJson(QJsonDocument::Compact);
            QCOMPARE(cache.write(bytes), bytes.size());
            cache.close();
            QVERIFY(editor.setIdleOptions({{QStringLiteral("city"), QString()}, {QStringLiteral("enabled"), true}}));
            QVERIFY(editor.setIdle(QStringLiteral("city"), observation.value(QStringLiteral("city")).toString()));
            QTRY_VERIFY(current->property("freshWeather").toBool());
            for (const auto &dial : {"weather", "info", "digital"}) {
                QVERIFY(editor.setIdle(QStringLiteral("dialId"), QString::fromLatin1(dial)));
                QTRY_COMPARE(current->property("dialId").toString(), QString::fromLatin1(dial));
                auto *temperature = current->findChild<QObject *>(QStringLiteral("panelWeatherTemperature"));
                QVERIFY(temperature);
                QCOMPARE(temperature->property("text").toString(),
                         QString::number(qRound(observation.value(QStringLiteral("temperature")).toDouble())) + QStringLiteral("°C"));
                QTest::qWait(50);
                if (!screenshots.isEmpty())
                    QVERIFY(view.grabWindow().save(QDir(screenshots).filePath(QStringLiteral("desktop-dial-%1-live.png").arg(QString::fromLatin1(dial)))));
            }
        }
    }
};
QTEST_MAIN(PanelDialsUiTest)
#include "paneldialsuitest.moc"
