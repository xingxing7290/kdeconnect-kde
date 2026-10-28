/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>
#include <paneleditor.h>

using ControlPanel::PanelEditor;

class PanelEditorTest : public QObject
{
    Q_OBJECT
private:
    QVariantMap website(const QString &name)
    {
        return {{QStringLiteral("kind"), QStringLiteral("website")},
                {QStringLiteral("name"), name},
                {QStringLiteral("target"), QVariantMap{{QStringLiteral("url"), QStringLiteral("https://example.com/a?q=1")}}}};
    }
    QVariantList buttons(const PanelEditor &editor)
    {
        return editor.currentPage().value(QStringLiteral("slots")).toList();
    }
private Q_SLOTS:
    void applicationBindingIsNormalizedWithoutChangingPageDisplayName()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        QVERIFY(editor.renamePage(editor.currentPageId(), QStringLiteral("My editor"), QStringLiteral(" C:\\Apps\\NOTEPAD++.EXE ")));
        QCOMPARE(editor.currentPage().value(QStringLiteral("name")).toString(), QStringLiteral("My editor"));
        QCOMPARE(editor.currentPage().value(QStringLiteral("application")).toString(), QStringLiteral("notepad++.exe"));
        const auto saved = editor.config();
        QVERIFY(!editor.renamePage(editor.currentPageId(), QStringLiteral("Bad"), QStringLiteral("bad\n.exe")));
        QCOMPARE(editor.config(), saved);
        QVERIFY(editor.renamePage(editor.currentPageId(), QStringLiteral("My editor"), QString()));
        QVERIFY(editor.currentPage().value(QStringLiteral("application")).toString().isEmpty());
    }
    void pageLimitAndDeletionKeepAllReferencesValid()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        const auto home = editor.currentPageId();
        for (int index = 1; index < 10; ++index)
            QVERIFY(editor.addPage());
        QCOMPARE(editor.config().value(QStringLiteral("pages")).toList().size(), 10);
        QVERIFY(!editor.addPage());
        QVERIFY(editor.selectPage(home));
        QVERIFY(editor.deletePage(home));
        QVERIFY(editor.currentPageId() != home);
        QVERIFY(editor.config().value(QStringLiteral("homePageId")).toString() != home);
        QVERIFY(editor.config().value(QStringLiteral("activePageId")).toString() != home);
        while (editor.config().value(QStringLiteral("pages")).toList().size() > 1)
            QVERIFY(editor.deletePage(editor.currentPageId()));
        QVERIFY(!editor.deletePage(editor.currentPageId()));
    }
    void buttonIdentitySurvivesEditAndSwapWithoutCompactingEmptySlots()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        QVERIFY(editor.setButton(0, website(QStringLiteral("First"))));
        const auto id = buttons(editor)[0].toMap().value(QStringLiteral("id")).toString();
        QVERIFY(editor.setButton(0, website(QStringLiteral("Renamed"))));
        QCOMPARE(buttons(editor)[0].toMap().value(QStringLiteral("id")).toString(), id);
        QVERIFY(editor.moveButton(0, 17));
        QCOMPARE(buttons(editor).size(), 18);
        QVERIFY(buttons(editor)[0].isNull());
        QCOMPARE(buttons(editor)[17].toMap().value(QStringLiteral("id")).toString(), id);
        QVERIFY(editor.clearButton(17));
        QVERIFY(buttons(editor)[17].isNull());
        QVERIFY(!editor.setButton(18, website(QStringLiteral("Invalid"))));
    }
    void invalidTargetsAndOptionsPreserveLastValidRevision()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        const auto config = editor.config();
        auto invalid = website(QStringLiteral("Bad"));
        invalid.insert(QStringLiteral("target"), QVariantMap{{QStringLiteral("url"), QStringLiteral("file:///invalid")}});
        QVERIFY(!editor.setButton(0, invalid));
        QCOMPARE(editor.config(), config);
        QVERIFY(!editor.setAppearance(QStringLiteral("background"), QStringLiteral("red")));
        QCOMPARE(editor.config(), config);
        QVERIFY(!editor.setIdleOptions({{QStringLiteral("city"), QStringLiteral("New city")}, {QStringLiteral("timeoutMs"), 0}}));
        QCOMPARE(editor.config(), config);
        QVERIFY(editor.setIdleOptions({{QStringLiteral("city"), QStringLiteral("New city")}, {QStringLiteral("message"), QStringLiteral("Hello")}}));
        QCOMPARE(editor.config().value(QStringLiteral("revision")).toLongLong(), config.value(QStringLiteral("revision")).toLongLong() + 1);
    }
    void externalFileDropAndRestartPreserveConfiguration()
    {
        QTemporaryDir directory;
        const auto path = QDir(directory.path()).filePath(QStringLiteral("a document with spaces.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        PanelEditor editor(QDir(directory.path()).filePath(QStringLiteral("config")));
        QVERIFY(editor.dropFile(4, QUrl::fromLocalFile(path)));
        QCOMPARE(buttons(editor)[4].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("a document with spaces"));
        QVERIFY(editor.setMode(QStringLiteral("pages")));
        QVERIFY(editor.setAppearance(QStringLiteral("showLabels"), false));
        PanelEditor restored(QDir(directory.path()).filePath(QStringLiteral("config")));
        QCOMPARE(restored.config(), editor.config());
        QVERIFY(!editor.dropFile(5, QUrl(QStringLiteral("https://example.com/file.exe"))));
    }
};

QTEST_GUILESS_MAIN(PanelEditorTest)
#include "paneleditortest.moc"
