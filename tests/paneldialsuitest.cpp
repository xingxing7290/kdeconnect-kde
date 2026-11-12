/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <paneleditor.h>

class PanelDialsUiTest : public QObject
{
    Q_OBJECT
    static QObject *visualFind(QQuickItem *item, const QString &name)
    {
        if (item->objectName() == name)
            return item;
        for (auto *child : item->childItems())
            if (auto *found = visualFind(child, name))
                return found;
        return nullptr;
    }
private Q_SLOTS:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
#ifdef Q_OS_WIN
        QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc")) >= 0);
        QGuiApplication::setFont(QFont(QStringLiteral("Microsoft YaHei")));
#endif
    }
    void delayedDialEditsKeepDialogAndInputsOnConflictOrStoreFailure()
    {
        QTest::failOnWarning();
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
        auto *featured = qobject_cast<QQuickItem *>(find(QStringLiteral("panelDialFeatured")));
        auto *confirmation = find(QStringLiteral("panelDialConfirmation"));
        auto *settings = find(QStringLiteral("panelDialSettings"));
        QVERIFY(featured && confirmation && settings);
        featured->forceActiveFocus();
        QTest::keyClick(&view, Qt::Key_Return);
        QTRY_VERIFY(confirmation->property("visible").toBool());
        QVERIFY(editor.setAppearance(QStringLiteral("showLabels"), false));
        const auto concurrent = editor.config();
        click(QStringLiteral("panelDialSelectSave"));
        QVERIFY(confirmation->property("visible").toBool());
        QVERIFY(!confirmation->property("validationError").toString().isEmpty());
        QCOMPARE(editor.config(), concurrent);
        click(QStringLiteral("panelDialSelectCancel"));
        QTRY_VERIFY(!confirmation->property("visible").toBool());
        click(QStringLiteral("panelDialFeatured"));
        QTRY_VERIFY(confirmation->property("visible").toBool());
        ControlPanel::PanelStore store(directory.path());
        QLockFile lock(store.filePath() + QStringLiteral(".lock"));
        QVERIFY(lock.tryLock(0));
        click(QStringLiteral("panelDialSelectSave"));
        QVERIFY(confirmation->property("visible").toBool());
        QCOMPARE(editor.error(), QStringLiteral("store_busy"));
        QCOMPARE(editor.config(), concurrent);
        lock.unlock();
        click(QStringLiteral("panelDialSelectSave"));
        QTRY_VERIFY(!confirmation->property("visible").toBool());
        QCOMPARE(editor.config().value(QStringLiteral("idle")).toMap().value(QStringLiteral("dialId")).toString(), QStringLiteral("analog"));
        click(QStringLiteral("panelDialSettingsButton"));
        QTRY_VERIFY(settings->property("visible").toBool());
        auto *city = find(QStringLiteral("panelDialCity"));
        QVERIFY(city);
        city->setProperty("text", QStringLiteral("Unsaved city"));
        QVERIFY(editor.setAppearance(QStringLiteral("showVolume"), false));
        const auto beforeSettings = editor.config();
        click(QStringLiteral("panelDialSettingsSave"));
        QVERIFY(settings->property("visible").toBool());
        QCOMPARE(city->property("text").toString(), QStringLiteral("Unsaved city"));
        QCOMPARE(editor.config(), beforeSettings);
        click(QStringLiteral("panelDialSettingsCancel"));
        QTRY_VERIFY(!settings->property("visible").toBool());
        click(QStringLiteral("panelDialSettingsButton"));
        QTRY_VERIFY(settings->property("visible").toBool());
        city->setProperty("text", QStringLiteral("Retained city"));
        QVERIFY(lock.tryLock(0));
        click(QStringLiteral("panelDialSettingsSave"));
        QVERIFY(settings->property("visible").toBool());
        QVERIFY(!settings->property("validationError").toString().isEmpty());
        QCOMPARE(city->property("text").toString(), QStringLiteral("Retained city"));
        QCOMPARE(editor.config(), beforeSettings);
        lock.unlock();
        click(QStringLiteral("panelDialSettingsSave"));
        QTRY_VERIFY(!settings->property("visible").toBool());
        QCOMPARE(editor.config().value(QStringLiteral("idle")).toMap().value(QStringLiteral("city")).toString(), QStringLiteral("Retained city"));
    }
    void selectionSettingsAndPreviewsUseProductionQml()
    {
        QTest::failOnWarning();
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
                if (qstrcmp(dial, "info") == 0) {
                    auto *info = current->findChild<QObject *>(QStringLiteral("panelInformationDial"));
                    QVERIFY(info);
                    QCOMPARE(info->findChild<QObject *>(QStringLiteral("panelInfoTemperature"))->property("text"), temperature->property("text"));
                    QCOMPARE(info->findChild<QObject *>(QStringLiteral("panelInfoBattery"))->property("text").toString(), QStringLiteral("--%"));
                    const auto days = observation.value(QStringLiteral("forecast")).toArray();
                    if (!days.isEmpty()) {
                        for (int index = 0; index < days.size(); ++index) {
                            const auto day = days[index].toObject();
                            auto *range = visualFind(qobject_cast<QQuickItem *>(info), QStringLiteral("panelInfoForecastRange-%1").arg(index));
                            QVERIFY(range);
                            QCOMPARE(range->property("text").toString(),
                                     QStringLiteral("%1° / %2°")
                                         .arg(qRound(day.value(QStringLiteral("minimum")).toDouble()))
                                         .arg(qRound(day.value(QStringLiteral("maximum")).toDouble())));
                        }
                    }
                }
                QTest::qWait(50);
                if (!screenshots.isEmpty())
                    QVERIFY(view.grabWindow().save(QDir(screenshots).filePath(QStringLiteral("desktop-dial-%1-live.png").arg(QString::fromLatin1(dial)))));
            }
        }
    }
    void informationDialNeverInventsForecastOrBatteryAndHidesExpiredData()
    {
        QTest::failOnWarning();
#ifdef Q_OS_WIN
        QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc")) >= 0);
        QGuiApplication::setFont(QFont(QStringLiteral("Microsoft YaHei")));
#endif
        const auto now = QDateTime::currentDateTime();
        const auto cityDate = QDateTime::fromSecsSinceEpoch(now.toSecsSinceEpoch() + 28800, QTimeZone::UTC).date();
        QJsonArray days;
        for (int index = 0; index < 3; ++index)
            days.append(QJsonObject{{QStringLiteral("date"), cityDate.addDays(index).toString(Qt::ISODate)},
                                    {QStringLiteral("minimum"), 11.2},
                                    {QStringLiteral("maximum"), 22.4},
                                    {QStringLiteral("weatherCode"), 3}});
        QJsonObject observation{{QStringLiteral("city"), QStringLiteral("Test City")},
                                {QStringLiteral("resolvedCity"), QStringLiteral("Test City / Test Country")},
                                {QStringLiteral("source"), QStringLiteral("open-meteo")},
                                {QStringLiteral("status"), QStringLiteral("ready")},
                                {QStringLiteral("fetchedAt"), now.toSecsSinceEpoch()},
                                {QStringLiteral("observedAt"), now.toSecsSinceEpoch() - 300},
                                {QStringLiteral("temperature"), 18.2},
                                {QStringLiteral("weatherCode"), 3},
                                {QStringLiteral("forecast"), days},
                                {QStringLiteral("timezone"), QStringLiteral("Test/City")},
                                {QStringLiteral("utcOffsetSeconds"), 28800}};
        QQuickView view;
        view.setInitialProperties({{QStringLiteral("dialId"), QStringLiteral("info")},
                                   {QStringLiteral("city"), QStringLiteral("Test City")},
                                   {QStringLiteral("message"), QStringLiteral("Synthetic UI fixture - not a live observation")},
                                   {QStringLiteral("now"), now},
                                   {QStringLiteral("weather"), observation.toVariantMap()}});
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../app/qml/PanelDialPreview.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(1000, 520);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *info = view.rootObject()->findChild<QObject *>(QStringLiteral("panelInformationDial"));
        QVERIFY(info);
        auto *temperature = info->findChild<QObject *>(QStringLiteral("panelInfoTemperature"));
        auto *range = visualFind(qobject_cast<QQuickItem *>(info), QStringLiteral("panelInfoForecastRange-0"));
        auto *status = info->findChild<QObject *>(QStringLiteral("panelInfoStatus"));
        QVERIFY(temperature && range && status);
        QTRY_COMPARE(temperature->property("text").toString(), QStringLiteral("18°C"));
        QCOMPARE(range->property("text").toString(), QStringLiteral("11° / 22°"));
        QCOMPARE(info->findChild<QObject *>(QStringLiteral("panelInfoBattery"))->property("text").toString(), QStringLiteral("--%"));
        view.rootObject()->setProperty("message", QStringLiteral("<img src='https://invalid.example/not-loaded.png'>literal text</img>"));
        auto *message = info->findChild<QObject *>(QStringLiteral("panelInfoMessage"));
        QVERIFY(message);
        QCOMPARE(message->property("textFormat").toInt(), 0); // QQuickText::PlainText
        QVERIFY(message->property("text").toString().startsWith(QStringLiteral("<img")));
        observation.insert(QStringLiteral("status"), QStringLiteral("unavailable"));
        view.rootObject()->setProperty("weather", observation.toVariantMap());
        QTRY_VERIFY(status->property("text").toString().contains(QStringLiteral("缓存")));
        QCOMPARE(temperature->property("text").toString(), QStringLiteral("18°C"));
        view.rootObject()->setProperty("now", now.addSecs(7201));
        QTRY_COMPARE(temperature->property("text").toString(), QStringLiteral("--°C"));
        QCOMPARE(range->property("text").toString(), QStringLiteral("--° / --°"));
        observation.remove(QStringLiteral("forecast"));
        view.rootObject()->setProperty("weather", observation.toVariantMap());
        view.rootObject()->setProperty("now", now);
        QTRY_COMPARE(temperature->property("text").toString(), QStringLiteral("18°C"));
        QCOMPARE(range->property("text").toString(), QStringLiteral("--° / --°"));
        view.rootObject()->setProperty("weather", QVariantMap());
        QTRY_COMPARE(temperature->property("text").toString(), QStringLiteral("--°C"));
        QCOMPARE(range->property("text").toString(), QStringLiteral("--° / --°"));
    }
};
QTEST_MAIN(PanelDialsUiTest)
#include "paneldialsuitest.moc"
