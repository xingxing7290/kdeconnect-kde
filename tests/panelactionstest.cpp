/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>
#include <panelactions.h>

using namespace ControlPanel;

class PanelActionsTest : public QObject
{
    Q_OBJECT
private:
    QJsonObject button(const QString &kind, const QJsonObject &target)
    {
        return {{QStringLiteral("kind"), kind}, {QStringLiteral("target"), target}};
    }
private Q_SLOTS:
    void websitePreservesFullQueryAndRejectsOtherSchemes()
    {
        const auto valid = PanelActions::resolve(
            button(QStringLiteral("website"), {{QStringLiteral("url"), QStringLiteral("https://example.com/a?x=1&next=%2Fdocs#chapter")}}));
        QVERIFY(valid);
        QCOMPARE(valid->target, QStringLiteral("https://example.com/a?x=1&next=%2Fdocs#chapter"));
        for (const auto &url :
             {QStringLiteral("file:///private.txt"), QStringLiteral("javascript:alert(1)"), QStringLiteral("https://"), QStringLiteral("example.com")}) {
            QVERIFY(!PanelActions::resolve(button(QStringLiteral("website"), {{QStringLiteral("url"), url}})));
        }
    }
    void filesFoldersAndProgramArgumentsAreTyped()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = QDir(directory.path()).filePath(QStringLiteral("a file with spaces.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(PanelActions::resolve(button(QStringLiteral("file"), {{QStringLiteral("path"), path}})));
        QVERIFY(!PanelActions::resolve(button(QStringLiteral("folder"), {{QStringLiteral("path"), path}})));
        QVERIFY(PanelActions::resolve(button(QStringLiteral("folder"), {{QStringLiteral("path"), directory.path()}})));
        QVERIFY(!PanelActions::resolve(button(QStringLiteral("file"), {{QStringLiteral("path"), QStringLiteral("relative.txt")}})));
        auto target = QJsonObject{{QStringLiteral("path"), QCoreApplication::applicationFilePath()},
                                  {QStringLiteral("arguments"), QJsonArray{QStringLiteral("--option"), QStringLiteral("value with spaces")}},
                                  {QStringLiteral("workingDirectory"), directory.path()}};
        const auto program = PanelActions::resolve(button(QStringLiteral("program"), target));
        QVERIFY(program);
        QCOMPARE(program->arguments.size(), 2);
        target.insert(QStringLiteral("arguments"), QStringLiteral("--unparsed shell text"));
        QVERIFY(!PanelActions::resolve(button(QStringLiteral("program"), target)));
        target.insert(QStringLiteral("arguments"), QJsonArray{true});
        QVERIFY(!PanelActions::resolve(button(QStringLiteral("program"), target)));
    }
    void shortcutsRequireModifiersBeforeOneKnownKey()
    {
        auto target = QJsonObject{{QStringLiteral("keys"), QJsonArray{QStringLiteral("CTRL"), QStringLiteral("SHIFT"), QStringLiteral("S")}}};
        const auto shortcut = PanelActions::resolve(button(QStringLiteral("shortcut"), target));
        QVERIFY(shortcut);
        QCOMPARE(shortcut->keys, QList<int>({0x11, 0x10, 0x53}));
        for (const auto &keys : {QJsonArray{},
                                 QJsonArray{QStringLiteral("CTRL")},
                                 QJsonArray{QStringLiteral("S"), QStringLiteral("CTRL")},
                                 QJsonArray{QStringLiteral("CTRL"), QStringLiteral("CTRL"), QStringLiteral("S")},
                                 QJsonArray{QStringLiteral("A"), QStringLiteral("B")},
                                 QJsonArray{QStringLiteral("UNKNOWN")}}) {
            target.insert(QStringLiteral("keys"), keys);
            QVERIFY(!PanelActions::resolve(button(QStringLiteral("shortcut"), target)));
        }
    }
    void systemTargetsUseAllowlistAndMissingFilesFailBeforeLaunching()
    {
        for (const auto &name : PanelActions::systemActionNames()) {
            QVERIFY(PanelActions::resolve(button(QStringLiteral("system"), {{QStringLiteral("action"), name}})));
        }
        QVERIFY(!PanelActions::resolve(button(QStringLiteral("system"), {{QStringLiteral("action"), QStringLiteral("arbitrary shell command")}})));
        QTemporaryDir directory;
        const auto missing = QDir(directory.path()).filePath(QStringLiteral("missing.exe"));
        QCOMPARE(PanelActions::execute(button(QStringLiteral("program"), {{QStringLiteral("path"), missing}})), QStringLiteral("target_missing"));
        QCOMPARE(PanelActions::execute(button(QStringLiteral("website"), {{QStringLiteral("url"), QStringLiteral("file:///invalid")}})),
                 QStringLiteral("invalid_target"));
    }
};

QTEST_GUILESS_MAIN(PanelActionsTest)
#include "panelactionstest.moc"
