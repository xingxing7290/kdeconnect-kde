/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <BreezeIcons>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
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
#include <cmath>
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
    void informationDialShowsConfiguredNotesAndProgress()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        const QVariantMap info{{QStringLiteral("notes"), QStringList{QStringLiteral("First note"), QStringLiteral("Second note")}},
                               {QStringLiteral("showProgress"), true},
                               {QStringLiteral("showForecast"), false},
                               {QStringLiteral("staleMs"), 15000}};
        QVariantMap progress{{QStringLiteral("source"), QStringLiteral("workbuddy")},
                             {QStringLiteral("status"), QStringLiteral("running")},
                             {QStringLiteral("title"), QStringLiteral("Test task")},
                             {QStringLiteral("stage"), QStringLiteral("Test stage")},
                             {QStringLiteral("detail"), QStringLiteral("Test detail")},
                             {QStringLiteral("percent"), 42},
                             {QStringLiteral("fetchedAt"), now.toSecsSinceEpoch()}};
        QQuickView view;
        view.setInitialProperties({{QStringLiteral("dialId"), QStringLiteral("info")},
                                   {QStringLiteral("now"), now},
                                   {QStringLiteral("info"), info},
                                   {QStringLiteral("progress"), progress}});
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../app/qml/PanelDialPreview.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(1000, 520);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *dial = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("panelInformationDial"));
        QVERIFY(dial);
        QTRY_VERIFY(visualFind(dial, QStringLiteral("panelInfoNote-1")));
        auto *note = visualFind(dial, QStringLiteral("panelInfoNote-1"));
        auto *state = visualFind(dial, QStringLiteral("panelInfoProgressState"));
        auto *stage = visualFind(dial, QStringLiteral("panelInfoProgressStage"));
        auto *percent = qobject_cast<QQuickItem *>(visualFind(dial, QStringLiteral("panelInfoProgressPercent")));
        auto *forecast = qobject_cast<QQuickItem *>(visualFind(dial, QStringLiteral("panelInfoForecast-0")));
        QVERIFY(note && state && stage && percent && forecast);
        QVERIFY(note->property("text").toString().endsWith(QStringLiteral("Second note")));
        QCOMPARE(state->property("text").toString(), QStringLiteral("进行中"));
        QVERIFY(stage->property("text").toString().contains(QStringLiteral("Test stage")));
        QCOMPARE(percent->property("text").toString(), QStringLiteral("42%"));
        QVERIFY(percent->isVisible());
        QVERIFY(!forecast->isVisible());
        progress.remove(QStringLiteral("percent"));
        view.rootObject()->setProperty("progress", progress);
        QTRY_VERIFY(!percent->isVisible());
        view.rootObject()->setProperty("now", now.addSecs(20));
        QTRY_COMPARE(state->property("text").toString(), QStringLiteral("进度已过期，等待更新"));
        progress.insert(QStringLiteral("status"), QStringLiteral("unavailable"));
        view.rootObject()->setProperty("progress", progress);
        QTRY_COMPARE(state->property("text").toString(), QStringLiteral("等待 WorkBuddy 进度"));
    }

    void previewsKeepReadableTextContrast()
    {
        QTest::failOnWarning();
        const auto blend = [](QColor foreground, const QColor &background) {
            const auto alpha = foreground.alphaF();
            return QColor::fromRgbF(foreground.redF() * alpha + background.redF() * (1 - alpha),
                                    foreground.greenF() * alpha + background.greenF() * (1 - alpha),
                                    foreground.blueF() * alpha + background.blueF() * (1 - alpha));
        };
        const auto luminance = [](const QColor &color) {
            const auto channel = [](double value) {
                return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) + 0.0722 * channel(color.blueF());
        };
        QCOMPARE(luminance(QColor(Qt::black)), 0.0);
        QCOMPARE(luminance(QColor(Qt::white)), 1.0);
        const auto grayRatio = 1.05 / (luminance(QColor(QStringLiteral("#777777"))) + 0.05);
        QVERIFY(grayRatio < 4.5);
        const auto translucentBlack = blend(QColor(0, 0, 0, 127), QColor(Qt::white));
        const auto alphaRatio = 1.05 / (luminance(translucentBlack) + 0.05);
        QVERIFY(alphaRatio > 3.9 && alphaRatio < 4.1);
        const auto now = QDateTime::currentDateTimeUtc();
        const auto cityDate = QDateTime::fromSecsSinceEpoch(now.toSecsSinceEpoch() + 28800, QTimeZone::UTC).date();
        QVariantList forecast;
        for (int index = 0; index < 3; ++index)
            forecast.append(QVariantMap{{QStringLiteral("date"), cityDate.addDays(index).toString(Qt::ISODate)},
                                        {QStringLiteral("minimum"), 11.2},
                                        {QStringLiteral("maximum"), 22.4},
                                        {QStringLiteral("weatherCode"), 3}});
        const QVariantMap observation{{QStringLiteral("city"), QStringLiteral("Synthetic contrast city")},
                                      {QStringLiteral("resolvedCity"), QStringLiteral("Synthetic contrast city / Test country")},
                                      {QStringLiteral("source"), QStringLiteral("open-meteo")},
                                      {QStringLiteral("status"), QStringLiteral("ready")},
                                      {QStringLiteral("fetchedAt"), now.toSecsSinceEpoch()},
                                      {QStringLiteral("observedAt"), now.toSecsSinceEpoch() - 300},
                                      {QStringLiteral("temperature"), 18.2},
                                      {QStringLiteral("weatherCode"), 3},
                                      {QStringLiteral("forecast"), forecast},
                                      {QStringLiteral("timezone"), QStringLiteral("Test/City")},
                                      {QStringLiteral("utcOffsetSeconds"), 28800}};
        QVERIFY(ControlPanel::PanelWeather::validObservation(QJsonObject::fromVariantMap(observation), now.toSecsSinceEpoch()));
        const auto reportDirectory = qEnvironmentVariable("KDECONNECT_PANEL_CONTRAST_REPORTS");
        const auto screenshotDirectory = qEnvironmentVariable("KDECONNECT_PANEL_SCREENSHOTS");
        QStringList failures;
        for (const auto &size : {QSize(1000, 520), QSize(520, 280)}) {
            QQuickView view;
            view.setInitialProperties({{QStringLiteral("city"), observation.value(QStringLiteral("city"))},
                                       {QStringLiteral("message"), QStringLiteral("Synthetic contrast fixture - not live weather")},
                                       {QStringLiteral("now"), now},
                                       {QStringLiteral("weather"), observation}});
            view.setResizeMode(QQuickView::SizeRootObjectToView);
            view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../app/qml/PanelDialPreview.qml")));
            QCOMPARE(view.status(), QQuickView::Ready);
            view.resize(size);
            view.show();
            QVERIFY(QTest::qWaitForWindowExposed(&view));
            for (const auto &state : {QStringLiteral("ready"), QStringLiteral("cached"), QStringLiteral("expired")}) {
                auto weather = observation;
                if (state == QLatin1String("cached"))
                    weather.insert(QStringLiteral("status"), QStringLiteral("unavailable"));
                view.rootObject()->setProperty("weather", weather);
                view.rootObject()->setProperty("now", state == QLatin1String("expired") ? now.addSecs(7201) : now);
                for (const auto &dial :
                     {QStringLiteral("digital"), QStringLiteral("analog"), QStringLiteral("weather"), QStringLiteral("info"), QStringLiteral("car")}) {
                    view.rootObject()->setProperty("dialId", dial);
                    if (dial == QLatin1String("info"))
                        QTRY_VERIFY(view.rootObject()->findChild<QObject *>(QStringLiteral("panelInformationDial")));
                    QTest::qWait(20);
                    const auto image = view.grabWindow();
                    QVERIFY(!image.isNull());
                    QCOMPARE(image.pixelColor(image.width() / 2, 2).rgb(), view.rootObject()->property("color").value<QColor>().rgb());
                    const auto name = QStringLiteral("desktop-contrast-%1-%2-%3").arg(dial, state).arg(size.width());
                    if (!screenshotDirectory.isEmpty()) {
                        QVERIFY(QDir().mkpath(screenshotDirectory));
                        QVERIFY(image.save(QDir(screenshotDirectory).filePath(name + QStringLiteral(".png"))));
                    }
                    QStringList rows{QStringLiteral("text\tforegroundARGB\tbackgroundARGB\tcontrast")};
                    QList<QQuickItem *> pending{view.rootObject()};
                    int checked = 0;
                    while (!pending.isEmpty()) {
                        auto *item = pending.takeLast();
                        pending.append(item->childItems());
                        if (!item->isVisible() || item->property("text").toString().isEmpty() || !item->property("font").isValid()
                            || !item->property("color").canConvert<QColor>())
                            continue;
                        QList<QColor> layers;
                        for (auto *parent = item->parentItem(); parent; parent = parent->parentItem()) {
                            QCOMPARE(parent->opacity(), 1.0);
                            if (parent->property("color").canConvert<QColor>())
                                layers.prepend(parent->property("color").value<QColor>());
                        }
                        QColor background = view.color();
                        for (const auto &layer : layers)
                            background = blend(layer, background);
                        auto foreground = item->property("color").value<QColor>();
                        foreground.setAlphaF(foreground.alphaF() * item->opacity());
                        const auto first = luminance(blend(foreground, background));
                        const auto second = luminance(background);
                        const auto ratio = (std::max(first, second) + 0.05) / (std::min(first, second) + 0.05);
                        const auto text = item->property("text").toString();
                        rows.append(QStringLiteral("%1\t%2\t%3\t%4")
                                        .arg(text, foreground.name(QColor::HexArgb), background.name(QColor::HexArgb))
                                        .arg(ratio, 0, 'g', 16));
                        if (ratio < 4.5)
                            failures.append(QStringLiteral("%1: %2 has contrast %3").arg(name, text.left(40)).arg(ratio, 0, 'g', 12));
                        ++checked;
                    }
                    QVERIFY(checked > 0);
                    if (dial == QLatin1String("info"))
                        QVERIFY(checked > 10);
                    if (!reportDirectory.isEmpty()) {
                        QVERIFY(QDir().mkpath(reportDirectory));
                        QFile report(QDir(reportDirectory).filePath(name + QStringLiteral(".tsv")));
                        QVERIFY(report.open(QIODevice::WriteOnly));
                        const auto bytes = rows.join(QLatin1Char('\n')).toUtf8();
                        QCOMPARE(report.write(bytes), bytes.size());
                    }
                }
            }
        }
        const auto details = failures.join(QLatin1Char('\n')).toUtf8();
        QVERIFY2(failures.isEmpty(), details.constData());
    }
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        BreezeIcons::initIcons();
        QIcon::setThemeName(QStringLiteral("breeze"));
        QVERIFY(QIcon::hasThemeIcon(QStringLiteral("preferences-system")));
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
