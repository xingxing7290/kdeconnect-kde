/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QTemporaryDir>
#include <QTest>
#include <paneleditor.h>
#include <volumecontroller.h>

class NoUiAudio : public SystemVolume::AudioSystem
{
public:
    QList<SystemVolume::Sink> read(QString *error) override
    {
        error->clear();
        return {};
    }
    QString setVolume(const QString &, int) override
    {
        return QStringLiteral("unavailable");
    }
    QString setMuted(const QString &, bool) override
    {
        return QStringLiteral("unavailable");
    }
    QString setDefault(const QString &) override
    {
        return QStringLiteral("unavailable");
    }
};
class PanelAssetsUiTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void cancelDiscardsIconAndSaveCommitsItInProductionDialog()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QTemporaryDir directory;
        ControlPanel::PanelEditor editor(directory.path());
        QVERIFY(editor.setButton(0,
                                 {{QStringLiteral("kind"), QStringLiteral("website")},
                                  {QStringLiteral("name"), QStringLiteral("Original")},
                                  {QStringLiteral("target"), QVariantMap{{QStringLiteral("url"), QStringLiteral("https://example.com")}}}}));
        QImage image(320, 160, QImage::Format_ARGB32);
        image.fill(Qt::blue);
        const auto source = QDir(directory.path()).filePath(QStringLiteral("custom.png"));
        QVERIFY(image.save(source));
        const auto original = editor.config();
        const auto button = editor.currentPage().value(QStringLiteral("slots")).toList()[0];
        SystemVolume::VolumeController volume(nullptr, std::make_unique<NoUiAudio>(), false);
        QQuickView view;
        view.engine()->rootContext()->setContextProperty(QStringLiteral("panelEditor"), &editor);
        view.engine()->rootContext()->setContextProperty(QStringLiteral("localVolume"), &volume);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../app/qml/ControlDashboard.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(1240, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *dialog = view.rootObject()->findChild<QObject *>(QStringLiteral("panelButtonDialog"));
        QVERIFY(dialog);
        auto open = [&] {
            const auto result = view.engine()->newQObject(dialog).property(QStringLiteral("openAt")).call({0, view.engine()->toScriptValue(button)});
            QVERIFY2(!result.isError(), qPrintable(result.toString()));
        };
        auto click = [&](const QString &name) {
            auto *control = dialog->findChild<QQuickItem *>(name);
            QVERIFY(control);
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, control->mapToScene(QPointF(control->width() / 2, control->height() / 2)).toPoint());
        };
        open();
        QTRY_VERIFY(dialog->property("visible").toBool());
        const auto icon = editor.importIcon(QUrl::fromLocalFile(source));
        QVERIFY(!icon.isEmpty());
        dialog->setProperty("iconId", icon);
        click(QStringLiteral("panelButtonCancel"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(editor.config(), original);
        open();
        QTRY_VERIFY(dialog->property("visible").toBool());
        QCOMPARE(dialog->property("iconId").toString(), QString());
        dialog->setProperty("iconId", icon);
        click(QStringLiteral("panelButtonSave"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        const auto saved = editor.currentPage().value(QStringLiteral("slots")).toList()[0].toMap();
        QCOMPARE(saved.value(QStringLiteral("icon")).toString(), icon);
        const auto committed = editor.config();
        open();
        QTRY_VERIFY(dialog->property("visible").toBool());
        dialog->setProperty("iconId", QString(64, QLatin1Char('a')));
        click(QStringLiteral("panelButtonSave"));
        QVERIFY(dialog->property("visible").toBool());
        QCOMPARE(editor.config(), committed);
        QVERIFY(!editor.error().isEmpty());
        click(QStringLiteral("panelButtonCancel"));
    }
};
QTEST_MAIN(PanelAssetsUiTest)
#include "panelassetsuitest.moc"
