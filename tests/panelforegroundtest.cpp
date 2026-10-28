/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QSignalSpy>
#include <QTest>
#include <panelforeground.h>

using namespace ControlPanel;

class PanelForegroundTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void namesAreCaseInsensitiveAndNeverContainPaths()
    {
        QCOMPARE(ForegroundFilter::applicationName(QStringLiteral("  C:\\Program Files\\Editor\\NOTEpad++.EXE  ")), QStringLiteral("notepad++.exe"));
        QCOMPARE(ForegroundFilter::applicationName(QStringLiteral("/opt/program.exe")), QStringLiteral("program.exe"));
        QVERIFY(ForegroundFilter::applicationName(QStringLiteral("bad\n.exe")).isEmpty());
        QVERIFY(ForegroundFilter::applicationName(QString(261, QLatin1Char('x'))).isEmpty());
    }
    void changesRequireContinuousStableFocus()
    {
        ForegroundFilter filter;
        QVERIFY(!filter.observe({QStringLiteral("first.exe"), false}, 0));
        QVERIFY(!filter.observe({QStringLiteral("first.exe"), false}, 399));
        QCOMPARE(filter.observe({QStringLiteral("FIRST.EXE"), false}, 400).value(), QStringLiteral("first.exe"));
        QVERIFY(!filter.observe({QStringLiteral("first.exe"), false}, 900));
        QVERIFY(!filter.observe({QStringLiteral("transient.exe"), false}, 1000));
        QVERIFY(!filter.observe({QStringLiteral("second.exe"), false}, 1200));
        QVERIFY(!filter.observe({QStringLiteral("second.exe"), false}, 1500));
        QCOMPARE(filter.observe({QStringLiteral("second.exe"), false}, 1600).value(), QStringLiteral("second.exe"));
    }
    void ignoredOrUnreadableWindowsCancelPendingButKeepStableApplication()
    {
        ForegroundFilter filter;
        filter.observe({QStringLiteral("first.exe"), false}, 0);
        QVERIFY(filter.observe({QStringLiteral("first.exe"), false}, 400));
        filter.observe({QStringLiteral("popup.exe"), false}, 500);
        QVERIFY(!filter.observe({}, 900));
        QVERIFY(!filter.observe({QStringLiteral("popup.exe"), false}, 1200));
        QVERIFY(!filter.observe({QStringLiteral("first.exe"), false}, 1400));
        QVERIFY(!filter.observe({QString(), false}, 2000));
        QVERIFY(!filter.observe({QStringLiteral("first.exe"), false}, 3000));
    }
    void cancelAndClockRollbackRestartDebounce()
    {
        ForegroundFilter filter;
        filter.observe({QStringLiteral("app.exe"), false}, 1000);
        filter.cancelPending();
        QVERIFY(!filter.observe({QStringLiteral("app.exe"), false}, 1500));
        QVERIFY(!filter.observe({QStringLiteral("app.exe"), false}, 100));
        QCOMPARE(filter.observe({QStringLiteral("app.exe"), false}, 500).value(), QStringLiteral("app.exe"));
    }
    void monitorPollsOnlyWhenEnabledAndStopsCleanly()
    {
        int reads = 0;
        PanelForeground monitor(nullptr, [&reads] {
            ++reads;
            return ForegroundSample{QStringLiteral("test.exe"), false};
        });
        QSignalSpy changed(&monitor, &PanelForeground::applicationChanged);
        QCOMPARE(reads, 0);
        monitor.setEnabled(true);
        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, 2000);
        QCOMPARE(monitor.application(), QStringLiteral("test.exe"));
        monitor.setEnabled(false);
        const int stoppedReads = reads;
        QTest::qWait(450);
        QCOMPARE(reads, stoppedReads);
        monitor.setEnabled(true);
        QTest::qWait(500);
        QCOMPARE(changed.size(), 1);
    }
};

QTEST_GUILESS_MAIN(PanelForegroundTest)
#include "panelforegroundtest.moc"
