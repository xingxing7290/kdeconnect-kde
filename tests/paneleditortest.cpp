/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTcpServer>
#include <QTcpSocket>
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
    void websiteIconDoesNotReplaceManualChoice()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QImage fetched(64, 64, QImage::Format_ARGB32);
        fetched.fill(Qt::red);
        QByteArray png;
        QBuffer buffer(&png);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(fetched.save(&buffer, "PNG"));
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [socket, png] {
                    const auto request = socket->readAll();
                    const QByteArray body = request.startsWith("GET /favicon.ico ") ? png : QByteArrayLiteral("<html><head></head></html>");
                    socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
        PanelEditor editor(directory.path(), nullptr, true);
        QImage manual(64, 64, QImage::Format_ARGB32);
        manual.fill(Qt::blue);
        ControlPanel::PanelAssets assets(directory.path());
        const auto manualId = assets.importPixels(manual);
        QVERIFY(!manualId.isEmpty());
        auto button = website(QStringLiteral("Website"));
        const auto url = QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort());
        button.insert(QStringLiteral("target"), QVariantMap{{QStringLiteral("url"), url}});
        bool changed = false;
        const auto connection = connect(&editor, &PanelEditor::websiteIconReady, &editor, [&](const QString &, const QString &) {
            button.insert(QStringLiteral("icon"), manualId);
            changed = editor.setButton(0, button);
        });
        QVERIFY(editor.setButton(0, button));
        QTRY_VERIFY_WITH_TIMEOUT(changed, 5000);
        QCOMPARE(buttons(editor).at(0).toMap().value(QStringLiteral("icon")).toString(), manualId);
        disconnect(connection);
        button.remove(QStringLiteral("icon"));
        QVERIFY(editor.setButton(1, button));
        const auto automaticId = buttons(editor).at(1).toMap().value(QStringLiteral("icon")).toString();
        QVERIFY(!automaticId.isEmpty());
        QVERIFY(automaticId != manualId);

        QTemporaryDir pendingDirectory;
        QVERIFY(pendingDirectory.isValid());
        PanelEditor pendingEditor(pendingDirectory.path(), nullptr, true);
        ControlPanel::PanelAssets pendingAssets(pendingDirectory.path());
        const auto pendingManualId = pendingAssets.importPixels(manual);
        QVERIFY(!pendingManualId.isEmpty());
        QVERIFY(pendingEditor.setButton(0, button));
        button.insert(QStringLiteral("icon"), pendingManualId);
        QVERIFY(pendingEditor.setButton(0, button));
        QTRY_COMPARE_WITH_TIMEOUT(pendingEditor.websiteIconStatus(), QStringLiteral("ready"), 5000);
        QCOMPARE(buttons(pendingEditor).at(0).toMap().value(QStringLiteral("icon")).toString(), pendingManualId);

        QTemporaryDir automaticDirectory;
        QVERIFY(automaticDirectory.isValid());
        PanelEditor automaticEditor(automaticDirectory.path(), nullptr, true);
        button.remove(QStringLiteral("icon"));
        QVERIFY(automaticEditor.setButton(0, button));
        QTRY_COMPARE_WITH_TIMEOUT(automaticEditor.websiteIconStatus(), QStringLiteral("ready"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(buttons(automaticEditor).at(0).toMap().value(QStringLiteral("icon")).toString(), automaticId, 5000);
    }
    void checkedEditsRejectChangedRevisionPageSlotAndIdentity()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        QVERIFY(editor.setMode(QStringLiteral("pages")));
        QVERIFY(editor.setButton(0, website(QStringLiteral("First"))));
        const auto initial = editor.editContext(0);
        QVERIFY(editor.setButtonChecked(0, website(QStringLiteral("Renamed")), initial));
        const auto saved = editor.config();
        QVERIFY(!editor.clearButtonChecked(0, initial));
        QCOMPARE(editor.error(), QStringLiteral("edit_conflict"));
        QCOMPARE(editor.config(), saved);
        auto context = editor.editContext(0);
        QVERIFY(!editor.setButtonChecked(1, website(QStringLiteral("Wrong slot")), context));
        context.insert(QStringLiteral("buttonId"), QStringLiteral("wrong-id"));
        QVERIFY(!editor.clearButtonChecked(0, context));
        context = editor.editContext(0);
        context.insert(QStringLiteral("revision"), context.value(QStringLiteral("revision")).toString());
        QVERIFY(!editor.clearButtonChecked(0, context));
        QCOMPARE(editor.config(), saved);
        const auto home = editor.currentPageId();
        QVERIFY(editor.addPage());
        const auto secondPage = editor.currentPageId();
        context = editor.editContext(0);
        QVERIFY(editor.selectPage(home));
        QVERIFY(!editor.clearButtonChecked(0, context));
        QVERIFY(editor.selectPage(secondPage));
        QVERIFY(editor.setButtonChecked(0, website(QStringLiteral("Second page")), context));
        QVERIFY(editor.clearButtonChecked(0, editor.editContext(0)));
        QVERIFY(editor.editContext(-1).isEmpty());
        QVERIFY(editor.editContext(18).isEmpty());
    }
    void checkedMoveRejectsStaleDragAndKeepsSourceIdentity()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        QVERIFY(editor.setButton(0, website(QStringLiteral("Source"))));
        auto context = editor.editContext(0);
        QVERIFY(editor.setButton(1, website(QStringLiteral("Destination"))));
        const auto saved = editor.config();
        QVERIFY(!editor.moveButtonChecked(0, 1, context));
        QCOMPARE(editor.config(), saved);
        context = editor.editContext(0);
        const auto before = buttons(editor);
        QVERIFY(editor.moveButtonChecked(0, 1, context));
        QCOMPARE(buttons(editor)[1], before[0]);
        QCOMPARE(buttons(editor)[0], before[1]);
    }
    void secondEditorCannotOverwriteDiskRevisionWithOldForm()
    {
        QTemporaryDir directory;
        PanelEditor first(directory.path());
        QVERIFY(first.setButton(0, website(QStringLiteral("Original"))));
        PanelEditor second(directory.path());
        const auto context = first.editContext(0);
        QVERIFY(second.setButton(0, website(QStringLiteral("New content"))));
        QVERIFY(!first.setButtonChecked(0, website(QStringLiteral("Stale form")), context));
        PanelEditor restored(directory.path());
        QCOMPARE(restored.config(), second.config());
    }
    void singleModeReturnsToHomeWithoutDeletingOtherPages()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        const auto home = editor.currentPageId();
        QVERIFY(editor.setMode(QStringLiteral("pages")));
        QVERIFY(editor.addPage());
        const auto second = editor.currentPageId();
        QVERIFY(editor.setButton(0, website(QStringLiteral("Retained"))));
        QVERIFY(editor.setMode(QStringLiteral("single")));
        QCOMPARE(editor.currentPageId(), home);
        QCOMPARE(editor.config().value(QStringLiteral("pages")).toList().size(), 2);
        QVERIFY(editor.setMode(QStringLiteral("pages")));
        QVERIFY(editor.selectPage(second));
        QCOMPARE(buttons(editor)[0].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Retained"));
    }
    void automaticFileNamesNeverContainPrivateParentPath()
    {
        QTemporaryDir directory;
        PanelEditor editor(QDir(directory.path()).filePath(QStringLiteral("config")));
        const auto path = QDir(directory.path()).filePath(QStringLiteral(".env"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const auto info = editor.inspectFile(QUrl::fromLocalFile(path));
        QCOMPARE(info.value(QStringLiteral("name")).toString(), QStringLiteral(".env"));
        const auto folder = QDir(directory.path()).filePath(QStringLiteral("folder.with.dots"));
        QVERIFY(QDir().mkpath(folder));
        QCOMPARE(editor.inspectFile(QUrl::fromLocalFile(folder)).value(QStringLiteral("name")).toString(), QStringLiteral("folder.with.dots"));
    }
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
    void delayedAppearanceAndIdleEditsRejectChangedRevision()
    {
        QTemporaryDir directory;
        PanelEditor editor(directory.path());
        const auto revision = editor.config().value(QStringLiteral("revision")).toDouble();
        QVERIFY(editor.setIdleOptionsChecked({{QStringLiteral("city"), QStringLiteral("First city")}}, revision));
        const auto saved = editor.config();
        QVERIFY(!editor.setIdleOptionsChecked({{QStringLiteral("dialId"), QStringLiteral("analog")}}, revision));
        QCOMPARE(editor.error(), QStringLiteral("edit_conflict"));
        QCOMPARE(editor.config(), saved);
        QVERIFY(!editor.setAppearanceChecked(QStringLiteral("background"), QStringLiteral("#112233"), revision));
        QCOMPARE(editor.config(), saved);
        const auto current = saved.value(QStringLiteral("revision")).toDouble();
        QVERIFY(!editor.setIdleOptionsChecked({{QStringLiteral("timeoutMs"), 0}}, current));
        QCOMPARE(editor.config(), saved);
        QVERIFY(editor.setAppearanceChecked(QStringLiteral("background"), QStringLiteral("#112233"), current));
        QCOMPARE(editor.config().value(QStringLiteral("appearance")).toMap().value(QStringLiteral("background")).toString(), QStringLiteral("#112233"));
        ControlPanel::PanelEditor restarted(directory.path());
        QCOMPARE(restarted.config(), editor.config());
    }
};

QTEST_GUILESS_MAIN(PanelEditorTest)
#include "paneleditortest.moc"
