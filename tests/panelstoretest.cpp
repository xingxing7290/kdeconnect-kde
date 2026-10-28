/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <panelstore.h>

using ControlPanel::PanelDocument;
using ControlPanel::PanelStore;

class PanelStoreTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void persistsAndDetectsConcurrentEditors()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        PanelStore first(directory.path());
        PanelStore second(directory.path());
        QVERIFY(first.load());
        QVERIFY(second.load());
        const auto original = first.document();
        auto updated = original.privateConfig();
        updated.insert(QStringLiteral("mode"), QStringLiteral("pages"));
        QVERIFY(first.save(updated, original.revision()));
        QVERIFY(!second.save(updated, original.revision()));
        QCOMPARE(second.lastError(), QStringLiteral("stale_revision"));
        QVERIFY(second.load());
        QCOMPARE(first.document().privateConfig(), second.document().privateConfig());
        QCOMPARE(first.document().revision(), original.revision() + 1);
        QFile backup(first.filePath() + QStringLiteral(".bak"));
        QVERIFY(backup.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(backup.readAll()).object(), original.privateConfig());
    }

    void invalidSavePreservesDisk()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        QVERIFY(store.load());
        const auto original = store.document();
        auto invalid = original.privateConfig();
        invalid.insert(QStringLiteral("pages"), false);
        QVERIFY(!store.save(invalid, original.revision()));
        QVERIFY(store.load());
        QCOMPARE(store.document().privateConfig(), original.privateConfig());
    }

    void explicitRecoveryPreservesCorruptFileAndIncrementsRevision()
    {
        QTemporaryDir directory;
        PanelStore store(directory.path());
        QVERIFY(store.load());
        const auto original = store.document();
        auto changed = original.privateConfig();
        changed.insert(QStringLiteral("mode"), QStringLiteral("pages"));
        QVERIFY(store.save(changed, original.revision()));
        const qint64 before = store.document().revision();
        QFile corrupted(store.filePath());
        QVERIFY(corrupted.open(QIODevice::WriteOnly));
        corrupted.write("broken");
        corrupted.close();
        QVERIFY(!store.load());
        QVERIFY(!store.isUsable());
        QVERIFY(!store.save(changed, before));
        QVERIFY(store.restoreBackup());
        QVERIFY(store.isUsable());
        QVERIFY(store.document().revision() > before);
        QCOMPARE(store.document().privateConfig().value(QStringLiteral("mode")).toString(), QStringLiteral("single"));
        QCOMPARE(QDir(directory.path()).entryList({QStringLiteral("panel.json.rejected-*")}, QDir::Files).size(), 1);
    }

    void directoryWatcherSurvivesAtomicReplacement()
    {
        QTemporaryDir directory;
        PanelStore editor(directory.path());
        PanelStore daemon(directory.path());
        QVERIFY(editor.load());
        QVERIFY(daemon.load());
        QSignalSpy changed(&daemon, &PanelStore::changed);
        for (int i = 0; i < 2; ++i) {
            auto config = editor.document().privateConfig();
            config.insert(QStringLiteral("mode"), i == 0 ? QStringLiteral("pages") : QStringLiteral("follow"));
            QVERIFY(editor.save(config, editor.document().revision()));
            QTRY_COMPARE_WITH_TIMEOUT(daemon.document().revision(), editor.document().revision(), 5000);
        }
        QCOMPARE(changed.count(), 2);
    }
};

QTEST_GUILESS_MAIN(PanelStoreTest)
#include "panelstoretest.moc"
