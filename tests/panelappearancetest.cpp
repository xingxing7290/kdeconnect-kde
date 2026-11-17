/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "../app/appearance.h"
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using ControlPanel::Appearance;

class PanelAppearanceTest : public QObject
{
    Q_OBJECT
    QTemporaryDir directory;

    static QQuickItem *findVisual(QQuickItem *item, const QString &name)
    {
        if (item->objectName() == name)
            return item;
        for (auto *child : item->childItems()) {
            if (auto *found = findVisual(child, name))
                return found;
        }
        return nullptr;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    }

    void init()
    {
        QSettings settings(QStringLiteral("KDEConnect"), QStringLiteral("ControlPanel"));
        settings.clear();
        settings.sync();
    }

    void themesPersistAndExposePalettes()
    {
        Appearance appearance;
        QCOMPARE(appearance.themeIndex(), 0);
        QCOMPARE(appearance.themeCount(), 6);
        QCOMPARE(appearance.themeNames().size(), appearance.themeCount());
        QSignalSpy changes(&appearance, &Appearance::appearanceChanged);
        for (int index = 1; index < appearance.themeCount(); ++index) {
            appearance.setThemeIndex(index);
            QCOMPARE(appearance.themeIndex(), index);
            QVERIFY(!appearance.themeId().isEmpty());
            QVERIFY(!appearance.colors().value(QStringLiteral("window")).toString().isEmpty());
            QCOMPARE(appearance.dark(), index >= 3);
            Appearance reloaded;
            QCOMPARE(reloaded.themeIndex(), index);
            QCOMPARE(reloaded.themeId(), appearance.themeId());
        }
        QCOMPARE(changes.size(), 5);
        appearance.setThemeIndex(5);
        QCOMPARE(changes.size(), 5);
    }

    void invalidThemeIndexesAreClamped()
    {
        Appearance appearance;
        appearance.setThemeIndex(999);
        QCOMPARE(appearance.themeIndex(), 5);
        appearance.setThemeIndex(-1);
        QCOMPARE(appearance.themeIndex(), 0);
        QSettings settings(QStringLiteral("KDEConnect"), QStringLiteral("ControlPanel"));
        settings.setValue(QStringLiteral("appearance/theme"), 999);
        settings.sync();
        Appearance reloaded;
        QCOMPARE(reloaded.themeIndex(), 5);
    }

    void qmlThemeTracksAppearanceChanges()
    {
        Appearance appearance;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("appearance"), &appearance);
        const QString path = QFINDTESTDATA("../app/qml/AppTheme.qml");
        QVERIFY(!path.isEmpty());
        QQmlComponent component(&engine, QUrl::fromLocalFile(path));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> theme(component.create());
        QVERIFY2(theme != nullptr, qPrintable(component.errorString()));

        appearance.setThemeIndex(1);
        QCOMPARE(theme->property("themeIndex").toInt(), 1);
        QCOMPARE(theme->property("window").value<QColor>(), QColor(QStringLiteral("#f4f7fc")));
        QCOMPARE(theme->property("dark").toBool(), false);
        appearance.setThemeIndex(3);
        QCOMPARE(theme->property("themeIndex").toInt(), 3);
        QCOMPARE(theme->property("window").value<QColor>(), QColor(QStringLiteral("#121826")));
        QCOMPARE(theme->property("dark").toBool(), true);
    }

    void themeMenuSelectsAndPersists()
    {
        Appearance appearance;
        QQuickView view;
        view.rootContext()->setContextProperty(QStringLiteral("appearance"), &appearance);
        const QString path = QFINDTESTDATA("../app/qml/ThemeSwitcher.qml");
        QVERIFY(!path.isEmpty());
        QQmlComponent component(view.engine());
        const QUrl harnessUrl = QUrl::fromLocalFile(QFileInfo(path).dir().filePath(QStringLiteral("PanelAppearanceHarness.qml")));
        component.setData(QByteArrayLiteral("import QtQuick\nItem { width: 300; height: 400; ThemeSwitcher { x: 20; y: 300; width: 214; height: 84 } }"),
                          harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QObject *root = component.create();
        QVERIFY2(root, qPrintable(component.errorString()));
        view.setContent(harnessUrl, &component, root);
        view.resize(300, 400);
        view.show();
        QTest::qWait(100);
        auto *button = root->findChild<QQuickItem *>(QStringLiteral("panelThemeSwitcher"));
        QVERIFY(button);
        const QPoint buttonPoint = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, buttonPoint);
        QCoreApplication::processEvents();
        QQuickItem *option = nullptr;
        QTRY_VERIFY((option = findVisual(view.contentItem(), QStringLiteral("panelThemeOption-3"))) != nullptr);
        QVERIFY(option->isVisible());
        const QPoint optionPoint = option->mapToScene(QPointF(option->width() / 2, option->height() / 2)).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, optionPoint);
        QTRY_COMPARE(appearance.themeIndex(), 3);
        Appearance reloaded;
        QCOMPARE(reloaded.themeIndex(), 3);
    }
};

QTEST_MAIN(PanelAppearanceTest)
#include "panelappearancetest.moc"
