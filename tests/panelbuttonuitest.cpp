/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <BreezeIcons>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QMimeData>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <paneleditor.h>
#include <volumecontroller.h>

class ButtonTestAudio : public SystemVolume::AudioSystem
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

class PanelButtonUiTest : public QObject
{
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> directory;
    std::unique_ptr<ControlPanel::PanelEditor> editor;
    std::unique_ptr<SystemVolume::VolumeController> volume;
    std::unique_ptr<QQuickView> view;
    QObject *dialog = nullptr;
    QString documentPath;
    QString folderPath;

    static QQuickItem *visualFind(QQuickItem *item, const QString &name)
    {
        if (item->objectName() == name)
            return item;
        for (auto *child : item->childItems())
            if (auto *found = visualFind(child, name))
                return found;
        return nullptr;
    }
    QQuickItem *find(const QString &name) const
    {
        if (auto *item = visualFind(view->contentItem(), name))
            return item;
        return view->rootObject()->findChild<QQuickItem *>(name);
    }
    bool set(const QString &name, const char *property, const QVariant &value)
    {
        auto *item = find(name);
        return item && item->setProperty(property, value);
    }
    bool click(const QString &name)
    {
        view->grabWindow();
        auto *item = find(name);
        if (!item || !item->isVisible() || !item->isEnabled())
            return false;
        const auto point = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
        if (!QRect(QPoint(), view->size()).contains(point))
            return false;
        QTest::mouseClick(view.get(), Qt::LeftButton, Qt::NoModifier, point);
        QCoreApplication::processEvents();
        return true;
    }
    QVariantMap button(int index = 0) const
    {
        return editor->currentPage().value(QStringLiteral("slots")).toList()[index].toMap();
    }
    QVariantMap website(const QString &name) const
    {
        return {{QStringLiteral("kind"), QStringLiteral("website")},
                {QStringLiteral("name"), name},
                {QStringLiteral("target"), QVariantMap{{QStringLiteral("url"), QStringLiteral("https://example.com")}}}};
    }
    bool saveEvidence(const QString &name)
    {
        const auto output = qEnvironmentVariable("KDECONNECT_PANEL_BUTTON_EVIDENCE_DIR");
        if (output.isEmpty())
            return true;
        return QDir().mkpath(output) && view->grabWindow().save(QDir(output).filePath(name + QStringLiteral(".png")));
    }
    Qt::DropAction drop(int index, const QMimeData &data, Qt::DropActions actions)
    {
        auto *slot = find(QStringLiteral("panelSlot-%1").arg(index));
        if (!slot)
            return Qt::IgnoreAction;
        const auto point = slot->mapToScene(QPointF(slot->width() / 2, slot->height() / 2)).toPoint();
        QDragEnterEvent enter(point, actions, &data, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(view.get(), &enter);
        QDragMoveEvent move(point, actions, &data, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(view.get(), &move);
        QDropEvent event(point, actions, &data, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(view.get(), &event);
        return event.isAccepted() ? event.dropAction() : Qt::IgnoreAction;
    }
private Q_SLOTS:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        BreezeIcons::initIcons();
        QIcon::setThemeName(QStringLiteral("breeze"));
#ifdef Q_OS_WIN
        QVERIFY(QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc")) >= 0);
        QFont font(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(14);
        QGuiApplication::setFont(font);
#endif
    }
    void init()
    {
        QTest::failOnWarning();
        directory = std::make_unique<QTemporaryDir>();
        QVERIFY(directory->isValid());
        documentPath = QDir(directory->path()).filePath(QStringLiteral("测试 document.txt"));
        folderPath = QDir(directory->path()).filePath(QStringLiteral("folder.with.dots"));
        QVERIFY(QDir().mkpath(folderPath));
        QFile file(documentPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("owned fixture\n"), 14);
        file.close();
        editor = std::make_unique<ControlPanel::PanelEditor>(QDir(directory->path()).filePath(QStringLiteral("config")));
        volume = std::make_unique<SystemVolume::VolumeController>(nullptr, std::make_unique<ButtonTestAudio>(), false);
        view = std::make_unique<QQuickView>();
        view->engine()->rootContext()->setContextProperty(QStringLiteral("panelEditor"), editor.get());
        view->engine()->rootContext()->setContextProperty(QStringLiteral("localVolume"), volume.get());
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->setSource(QUrl::fromLocalFile(QFINDTESTDATA("../app/qml/ControlDashboard.qml")));
        QCOMPARE(view->status(), QQuickView::Ready);
        view->resize(1240, 800);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
        dialog = view->rootObject()->findChild<QObject *>(QStringLiteral("panelButtonDialog"));
        QVERIFY(dialog);
    }
    void cleanup()
    {
        if (view)
            view->setSource(QUrl());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        view.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        volume.reset();
        editor.reset();
        QCoreApplication::processEvents();
        directory.reset();
    }
    void allKindsSaveFromProductionForm_data()
    {
        QTest::addColumn<int>("type");
        QTest::addColumn<QString>("kind");
        QTest::newRow("program") << 0 << QStringLiteral("program");
        QTest::newRow("website") << 1 << QStringLiteral("website");
        QTest::newRow("file") << 2 << QStringLiteral("file");
        QTest::newRow("folder") << 3 << QStringLiteral("folder");
        QTest::newRow("shortcut") << 4 << QStringLiteral("shortcut");
        QTest::newRow("system") << 5 << QStringLiteral("system");
        QTest::newRow("command") << 6 << QStringLiteral("command");
    }
    void allKindsSaveFromProductionForm()
    {
        QFETCH(int, type);
        QFETCH(QString, kind);
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(set(QStringLiteral("panelButtonType"), "currentIndex", type));
        QString target;
        if (type == 0)
            target = QLatin1Char('"') + QCoreApplication::applicationFilePath() + QLatin1Char('"');
        if (type == 1)
            target = QStringLiteral("example.com/path?q=1#part");
        if (type == 2)
            target = documentPath;
        if (type == 3)
            target = folderPath;
        if (type == 6)
            target = QStringLiteral("test-command-key");
        QVERIFY(set(QStringLiteral("panelButtonTarget"), "text", target));
        if (type == 4) {
            QVERIFY(set(QStringLiteral("panelShortcutCtrl"), "checked", true));
            QVERIFY(set(QStringLiteral("panelShortcutShift"), "checked", true));
        }
        if (type == 5)
            QVERIFY(set(QStringLiteral("panelButtonSystem"), "currentIndex", 4));
        QVERIFY(click(QStringLiteral("panelButtonSave")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        const auto saved = button();
        QCOMPARE(saved.value(QStringLiteral("kind")).toString(), kind);
        QVERIFY(!saved.value(QStringLiteral("id")).toString().isEmpty());
        QVERIFY(!saved.value(QStringLiteral("name")).toString().isEmpty());
        const auto value = saved.value(QStringLiteral("target")).toMap();
        if (type == 0)
            QCOMPARE(value.value(QStringLiteral("path")).toString(), QCoreApplication::applicationFilePath());
        if (type == 1) {
            QCOMPARE(value.value(QStringLiteral("url")).toString(), QStringLiteral("https://example.com/path?q=1#part"));
            QCOMPARE(saved.value(QStringLiteral("name")).toString(), QStringLiteral("example.com"));
        }
        if (type == 2)
            QCOMPARE(value.value(QStringLiteral("path")).toString(), documentPath);
        if (type == 3)
            QCOMPARE(saved.value(QStringLiteral("name")).toString(), QStringLiteral("folder.with.dots"));
        if (type == 4) {
            const auto keys = value.value(QStringLiteral("keys")).toStringList();
            QCOMPARE(keys.size(), 3);
            QCOMPARE(keys.mid(0, 2), QStringList({QStringLiteral("CTRL"), QStringLiteral("SHIFT")}));
        }
        if (type == 5)
            QCOMPARE(value.value(QStringLiteral("action")).toString(), QStringLiteral("showDesktop"));
        if (type == 6)
            QCOMPARE(value.value(QStringLiteral("key")).toString(), target);
        ControlPanel::PanelEditor restored(QDir(directory->path()).filePath(QStringLiteral("config")));
        QCOMPARE(restored.config(), editor->config());
    }
    void programRenameRetainsArgumentsAndDirectoryAndValidatesInline()
    {
        const QVariantMap target{{QStringLiteral("path"), QCoreApplication::applicationFilePath()},
                                 {QStringLiteral("arguments"), QVariantList{QStringLiteral("--profile"), QStringLiteral("工作 spaces"), QString()}},
                                 {QStringLiteral("workingDirectory"), folderPath}};
        QVERIFY(editor->setButton(
            0,
            {{QStringLiteral("kind"), QStringLiteral("program")}, {QStringLiteral("name"), QStringLiteral("Original")}, {QStringLiteral("target"), target}}));
        const auto id = button().value(QStringLiteral("id"));
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(find(QStringLiteral("panelProgramAdvanced"))->property("checked").toBool());
        QVERIFY(saveEvidence(QStringLiteral("program-advanced")));
        QVERIFY(set(QStringLiteral("panelButtonName"), "text", QStringLiteral("Renamed")));
        QVERIFY(set(QStringLiteral("panelProgramAdvanced"), "checked", false));
        QVERIFY(click(QStringLiteral("panelButtonSave")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(button().value(QStringLiteral("target")).toMap(), target);
        QCOMPARE(button().value(QStringLiteral("id")), id);
        const auto saved = editor->config();
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(set(QStringLiteral("panelProgramArguments"), "text", QStringLiteral("[1]")));
        QVERIFY(click(QStringLiteral("panelButtonSave")));
        QVERIFY(dialog->property("visible").toBool());
        QVERIFY(!dialog->property("validationError").toString().isEmpty());
        QCOMPARE(editor->config(), saved);
        QVERIFY(click(QStringLiteral("panelButtonCancel")));
    }
    void cancelDeleteAndConcurrentEditPreserveCorrectConfiguration()
    {
        QVERIFY(editor->setButton(0, website(QStringLiteral("Original"))));
        QVERIFY(editor->setButton(3, website(QStringLiteral("Unchanged"))));
        const auto saved = editor->config();
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(set(QStringLiteral("panelButtonName"), "text", QStringLiteral("Discarded")));
        QVERIFY(click(QStringLiteral("panelButtonCancel")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(editor->config(), saved);
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(editor->setButton(0, website(QStringLiteral("New content"))));
        const auto newer = editor->config();
        QVERIFY(click(QStringLiteral("panelButtonSave")));
        QVERIFY(dialog->property("visible").toBool());
        QCOMPARE(editor->error(), QStringLiteral("edit_conflict"));
        QCOMPARE(editor->config(), newer);
        QVERIFY(click(QStringLiteral("panelButtonDelete")));
        QCOMPARE(editor->config(), newer);
        QVERIFY(click(QStringLiteral("panelButtonCancel")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        const auto untouched = button(3);
        QVERIFY(click(QStringLiteral("panelButtonDelete")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(button().isEmpty());
        QCOMPARE(button(3), untouched);
        QCOMPARE(editor->currentPage().value(QStringLiteral("slots")).toList().size(), 18);
    }
    void websiteAutoNameDoesNotExposeCredentials()
    {
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(set(QStringLiteral("panelButtonType"), "currentIndex", 1));
        QVERIFY(set(QStringLiteral("panelButtonTarget"), "text", QStringLiteral("HTTPS://user:secret@example.com/path?q=1#hash")));
        QVERIFY(click(QStringLiteral("panelButtonSave")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(button().value(QStringLiteral("name")).toString(), QStringLiteral("example.com"));
    }
    void invalidFileRemainsInDialogWithInlineError()
    {
        const auto saved = editor->config();
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(set(QStringLiteral("panelButtonType"), "currentIndex", 2));
        QVERIFY(set(QStringLiteral("panelButtonTarget"), "text", QString(documentPath + QStringLiteral(".missing"))));
        QVERIFY(click(QStringLiteral("panelButtonSave")));
        QVERIFY(dialog->property("visible").toBool());
        QVERIFY(!dialog->property("validationError").toString().isEmpty());
        QCOMPARE(editor->config(), saved);
        QVERIFY(click(QStringLiteral("panelButtonCancel")));
    }
    void smallWindowKeepsFooterReachableAndFormScrollable()
    {
        QVERIFY(click(QStringLiteral("panelSlot-0")));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(set(QStringLiteral("panelProgramAdvanced"), "checked", true));
        view->resize(760, 420);
        QTRY_VERIFY(dialog->property("height").toReal() <= 388);
        auto *scroll = find(QStringLiteral("panelButtonScroll"));
        QVERIFY(scroll);
        QTRY_VERIFY(scroll->property("contentHeight").toReal() > scroll->height());
        QVERIFY(saveEvidence(QStringLiteral("small-window")));
        QVERIFY(click(QStringLiteral("panelButtonCancel")));
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }
    void singleModeHidesPageControlsAndKeepsPages()
    {
        QVERIFY(editor->setMode(QStringLiteral("pages")));
        QVERIFY(editor->addPage());
        const auto second = editor->currentPageId();
        QTRY_VERIFY(find(QStringLiteral("panelAddPage"))->isVisible());
        QVERIFY(editor->setMode(QStringLiteral("single")));
        QTRY_VERIFY(!find(QStringLiteral("panelAddPage"))->isVisible());
        QVERIFY(!find(QStringLiteral("panelDeletePage"))->isVisible());
        QVERIFY(!find(QStringLiteral("panelPage-1"))->isVisible());
        QCOMPARE(editor->currentPageId(), editor->config().value(QStringLiteral("homePageId")).toString());
        QCOMPARE(editor->config().value(QStringLiteral("pages")).toList().size(), 2);
        QVERIFY(editor->setMode(QStringLiteral("pages")));
        QTRY_VERIFY(find(QStringLiteral("panelPage-1"))->isVisible());
        QVERIFY(click(QStringLiteral("panelPage-1")));
        QCOMPARE(editor->currentPageId(), second);
        QSignalSpy configChanges(editor.get(), &ControlPanel::PanelEditor::configChanged);
        auto *home = find(QStringLiteral("panelPage-0"));
        QVERIFY(home);
        QTest::mouseMove(view.get(), home->mapToScene(QPointF(home->width() / 2, home->height() / 2)).toPoint());
        QTRY_COMPARE(editor->currentPageId(), editor->config().value(QStringLiteral("homePageId")).toString());
        QCOMPARE(configChanges.count(), 0);
        QVERIFY(saveEvidence(QStringLiteral("multiple-pages")));
    }
    void fileDropIsCopyOnlyAndNeverChangesSource()
    {
        QMimeData data;
        data.setUrls({QUrl::fromLocalFile(documentPath)});
        QCOMPARE(drop(4, data, Qt::CopyAction | Qt::MoveAction), Qt::CopyAction);
        QCOMPARE(button(4).value(QStringLiteral("target")).toMap().value(QStringLiteral("path")).toString(), documentPath);
        QFile file(documentPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("owned fixture\n"));
        const auto saved = editor->config();
        QCOMPARE(drop(5, data, Qt::MoveAction), Qt::IgnoreAction);
        QCOMPARE(editor->config(), saved);
        data.setUrls({QUrl::fromLocalFile(documentPath), QUrl::fromLocalFile(folderPath)});
        QCOMPARE(drop(5, data, Qt::CopyAction), Qt::IgnoreAction);
        QCOMPARE(editor->config(), saved);
    }
    void internalDropChecksContextAndRejectsMalformedPayload()
    {
        QVERIFY(editor->setButton(0, website(QStringLiteral("Source"))));
        const auto source = button();
        QMimeData data;
        const QString format = QStringLiteral("application/x-kdeconnect-slot");
        data.setData(format, QJsonDocument::fromVariant(editor->editContext(0)).toJson(QJsonDocument::Compact));
        QCOMPARE(drop(17, data, Qt::MoveAction), Qt::MoveAction);
        QCOMPARE(button(17), source);
        QVERIFY(button().isEmpty());
        const auto saved = editor->config();
        QCOMPARE(drop(5, data, Qt::MoveAction), Qt::IgnoreAction);
        QCOMPARE(editor->config(), saved);
        for (const auto &invalid : {QByteArray(""), QByteArray("0"), QByteArray("{"), QByteArray("[]")}) {
            data.setData(format, invalid);
            QCOMPARE(drop(5, data, Qt::MoveAction), Qt::IgnoreAction);
            QCOMPARE(editor->config(), saved);
        }
        data.clear();
        data.setData(QStringLiteral("application/x-kdeconnect-preset"), "{broken");
        QCOMPARE(drop(5, data, Qt::CopyAction), Qt::IgnoreAction);
    }
    void libraryDropCreatesIndependentButtonAndLeavesLibraryIntact()
    {
        const auto presets = view->rootObject()->property("presets").value<QJSValue>().toVariant().toList();
        QCOMPARE(presets.size(), 14);
        QMimeData data;
        data.setData(QStringLiteral("application/x-kdeconnect-preset"), QJsonDocument::fromVariant(presets[0]).toJson(QJsonDocument::Compact));
        QCOMPARE(drop(2, data, Qt::CopyAction), Qt::CopyAction);
        QCOMPARE(button(2).value(QStringLiteral("target")).toMap().value(QStringLiteral("action")).toString(), QStringLiteral("thisPc"));
        QCOMPARE(drop(3, data, Qt::CopyAction), Qt::CopyAction);
        QVERIFY(button(2).value(QStringLiteral("id")) != button(3).value(QStringLiteral("id")));
        QCOMPARE(view->rootObject()->property("presets").value<QJSValue>().toVariant().toList(), presets);
    }
};
QTEST_MAIN(PanelButtonUiTest)
#include "panelbuttonuitest.moc"
