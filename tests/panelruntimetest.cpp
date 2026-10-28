/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <panelruntime.h>

using namespace ControlPanel;

class PanelRuntimeTest : public QObject
{
    Q_OBJECT
    void initialize(PanelStore &store, const QString &mode = QStringLiteral("follow"))
    {
        QVERIFY(store.load());
        QFile file(QFINDTESTDATA("../docs/control-panel/fixtures/config-v1.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto config = QJsonDocument::fromJson(file.readAll()).object();
        config.insert(QStringLiteral("mode"), mode);
        auto pages = config.value(QStringLiteral("pages")).toArray();
        auto editor = pages[1].toObject();
        editor.insert(QStringLiteral("application"), QStringLiteral("notepad++.exe"));
        pages[1] = editor;
        config.insert(QStringLiteral("pages"), pages);
        QVERIFY(store.save(config, store.document().revision()));
    }
    QString active(const PanelRuntime &runtime)
    {
        return runtime.state().value(QStringLiteral("activePageId")).toString();
    }
private Q_SLOTS:
    void matchingUnknownAndRepeatedApplications()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        QSignalSpy changed(&runtime, &PanelRuntime::stateChanged);
        QCOMPARE(active(runtime), QStringLiteral("home"));
        runtime.setForegroundApplication(QStringLiteral("C:\\Apps\\NOTEpad++.EXE"));
        QCOMPARE(active(runtime), QStringLiteral("editor"));
        QCOMPARE(runtime.state().value(QStringLiteral("foregroundApplication")).toString(), QStringLiteral("notepad++.exe"));
        QCOMPARE(changed.size(), 1);
        runtime.setForegroundApplication(QStringLiteral("notepad++.exe"));
        runtime.setForegroundApplication(QString());
        QCOMPARE(changed.size(), 1);
        runtime.setForegroundApplication(QStringLiteral("unknown.exe"));
        QCOMPARE(active(runtime), QStringLiteral("home"));
    }
    void manualPagePersistsUntilNextStableApplicationChange()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        runtime.setForegroundApplication(QStringLiteral("notepad++.exe"));
        QVERIFY(runtime.selectPage(QStringLiteral("home")));
        runtime.setForegroundApplication(QStringLiteral("NOTEPAD++.exe"));
        QCOMPARE(active(runtime), QStringLiteral("home"));
        auto config = store.document().privateConfig();
        auto appearance = config.value(QStringLiteral("appearance")).toObject();
        appearance.insert(QStringLiteral("showLabels"), false);
        config.insert(QStringLiteral("appearance"), appearance);
        QVERIFY(store.save(config, store.document().revision()));
        QCOMPARE(active(runtime), QStringLiteral("home"));
        runtime.setForegroundApplication(QStringLiteral("unknown.exe"));
        runtime.setForegroundApplication(QStringLiteral("notepad++.exe"));
        QCOMPARE(active(runtime), QStringLiteral("editor"));
        QVERIFY(runtime.selectPage(QStringLiteral("home")));
        config = store.document().privateConfig();
        config.insert(QStringLiteral("mode"), QStringLiteral("pages"));
        QVERIFY(store.save(config, store.document().revision()));
        config.insert(QStringLiteral("mode"), QStringLiteral("follow"));
        QVERIFY(store.save(config, store.document().revision()));
        QCOMPARE(active(runtime), QStringLiteral("editor"));
        QVERIFY(runtime.selectPage(QStringLiteral("editor")));
        auto pages = config.value(QStringLiteral("pages")).toArray();
        pages.removeAt(1);
        config.insert(QStringLiteral("pages"), pages);
        QVERIFY(store.save(config, store.document().revision()));
        QCOMPARE(active(runtime), QStringLiteral("home"));
    }
    void otherModesDoNotFollowAndEnablingFollowUsesCurrentApplication()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store, QStringLiteral("pages"));
        PanelRuntime runtime(&store);
        runtime.setForegroundApplication(QStringLiteral("notepad++.exe"));
        QCOMPARE(active(runtime), QStringLiteral("home"));
        auto config = store.document().privateConfig();
        config.insert(QStringLiteral("mode"), QStringLiteral("follow"));
        QVERIFY(store.save(config, store.document().revision()));
        QCOMPARE(active(runtime), QStringLiteral("editor"));
        config.insert(QStringLiteral("mode"), QStringLiteral("single"));
        QVERIFY(store.save(config, store.document().revision()));
        runtime.setForegroundApplication(QStringLiteral("unknown.exe"));
        runtime.setForegroundApplication(QStringLiteral("notepad++.exe"));
        QCOMPARE(active(runtime), QStringLiteral("home"));
        QVERIFY(!runtime.selectPage(QStringLiteral("editor")));
    }
    void editingBindingsAndDeletingMatchedPageRecomputesWithoutNewFocus()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        PanelRuntime runtime(&store);
        runtime.setForegroundApplication(QStringLiteral("notepad++.exe"));
        auto config = store.document().privateConfig();
        auto pages = config.value(QStringLiteral("pages")).toArray();
        auto editor = pages[1].toObject();
        editor.insert(QStringLiteral("application"), QStringLiteral("other.exe"));
        pages[1] = editor;
        config.insert(QStringLiteral("pages"), pages);
        QVERIFY(store.save(config, store.document().revision()));
        QCOMPARE(active(runtime), QStringLiteral("home"));
        runtime.setForegroundApplication(QStringLiteral("other.exe"));
        QCOMPARE(active(runtime), QStringLiteral("editor"));
        pages.removeAt(1);
        config.insert(QStringLiteral("pages"), pages);
        QVERIFY(store.save(config, store.document().revision()));
        QCOMPARE(active(runtime), QStringLiteral("home"));
    }
    void duplicateBindingsUseFirstConfiguredPage()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        initialize(store);
        auto config = store.document().privateConfig();
        auto pages = config.value(QStringLiteral("pages")).toArray();
        auto home = pages[0].toObject();
        home.insert(QStringLiteral("application"), QStringLiteral("NOTEPAD++.EXE"));
        pages[0] = home;
        config.insert(QStringLiteral("pages"), pages);
        QVERIFY(store.save(config, store.document().revision()));
        PanelRuntime runtime(&store);
        runtime.setForegroundApplication(QStringLiteral("notepad++.exe"));
        QCOMPARE(active(runtime), QStringLiteral("home"));
    }
};

QTEST_GUILESS_MAIN(PanelRuntimeTest)
#include "panelruntimetest.moc"
