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

class UiAudioSystem : public SystemVolume::AudioSystem
{
public:
    QList<SystemVolume::Sink> values{{QStringLiteral("one"), QStringLiteral("First output"), 23, false, true},
                                     {QStringLiteral("two"), QStringLiteral("Second output"), 42, false, false}};
    int writes = 0;
    QList<SystemVolume::Sink> read(QString *error) override
    {
        error->clear();
        return values;
    }
    QString setVolume(const QString &id, int volume) override
    {
        ++writes;
        for (auto &sink : values)
            if (sink.id == id)
                sink.volume = volume;
        return {};
    }
    QString setMuted(const QString &id, bool muted) override
    {
        ++writes;
        for (auto &sink : values)
            if (sink.id == id)
                sink.muted = muted;
        return {};
    }
    QString setDefault(const QString &id) override
    {
        ++writes;
        for (auto &sink : values)
            sink.isDefault = sink.id == id;
        return {};
    }
};
class VolumeUiTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void sliderMuteAndExternalDefaultChangesUseProductionQml()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QTemporaryDir directory;
        ControlPanel::PanelEditor editor(directory.path());
        auto system = std::make_unique<UiAudioSystem>();
        auto *fake = system.get();
        SystemVolume::VolumeController volume(nullptr, std::move(system), false);
        QQuickView view;
        view.engine()->rootContext()->setContextProperty(QStringLiteral("panelEditor"), &editor);
        view.engine()->rootContext()->setContextProperty(QStringLiteral("localVolume"), &volume);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../app/qml/ControlDashboard.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.resize(1240, 800);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *slider = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("panelLocalVolumeSlider"));
        auto *mute = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("panelLocalMuteButton"));
        QVERIFY(slider && mute);
        QVERIFY(slider->isEnabled());
        QTRY_COMPARE(slider->property("value").toInt(), 23);
        const auto sliderPoint = slider->mapToScene(QPointF(slider->width() / 2, slider->height() * 0.8)).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, sliderPoint);
        QTRY_VERIFY(fake->writes > 0);
        QVERIFY(volume.volume() != 23);
        const int beforeKey = volume.volume();
        slider->forceActiveFocus();
        QTest::keyClick(&view, Qt::Key_Up);
        QTRY_COMPARE(volume.volume(), beforeKey + 1);
        const auto mutePoint = mute->mapToScene(QPointF(mute->width() / 2, mute->height() / 2)).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, mutePoint);
        QTRY_VERIFY(volume.muted());
        fake->values[0].isDefault = false;
        fake->values[1].isDefault = true;
        volume.refresh();
        QTRY_COMPARE(slider->property("value").toInt(), 42);
        QVERIFY(!volume.muted());
        auto *selector = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("panelLocalOutputSelector"));
        QVERIFY(selector);
        QTRY_COMPARE(selector->property("currentIndex").toInt(), 1);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, selector->mapToScene(QPointF(selector->width() / 2, selector->height() / 2)).toPoint());
        QTest::keyClick(&view, Qt::Key_Home);
        QTest::keyClick(&view, Qt::Key_Return);
        QTRY_COMPARE(volume.defaultId(), QStringLiteral("one"));
        fake->values.clear();
        volume.refresh();
        QTRY_VERIFY(!slider->isEnabled());
        QVERIFY(!mute->isEnabled());
    }
};
QTEST_MAIN(VolumeUiTest)
#include "volumeuitest.moc"
