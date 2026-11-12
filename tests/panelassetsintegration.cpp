/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QPainter>
#include <panelassets.h>
#include <paneleditor.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 4)
        return 2;
    if (args[1] == QLatin1String("--program")) {
        ControlPanel::PanelAssets assets(args[2]);
        const auto id = assets.importProgramIcon(QUrl::fromLocalFile(args[3]));
        QFile output;
        if (!output.open(stdout, QIODevice::WriteOnly))
            return 11;
        output.write(QJsonDocument(QJsonObject{{QStringLiteral("icon"), id}, {QStringLiteral("success"), !id.isEmpty()}}).toJson(QJsonDocument::Compact));
        output.flush();
        return id.isEmpty() ? 9 : 0;
    }
    if (args[1] != QLatin1String("--prepare"))
        return 2;
    ControlPanel::PanelEditor editor(args[2]);
    if (!editor.selectPage(QStringLiteral("home")) || !editor.selectPage(QStringLiteral("editor")))
        return 3;
    const auto original = editor.currentPage().value(QStringLiteral("slots")).toList()[0].toMap();
    if (original.value(QStringLiteral("name")).toString() != QStringLiteral("跟随匹配成功"))
        return 4;
    if (!QDir().mkpath(args[3]))
        return 5;
    QImage fixture(640, 320, QImage::Format_ARGB32);
    fixture.fill(QColor(QStringLiteral("#2266dd")));
    QPainter painter(&fixture);
    painter.setBrush(QColor(QStringLiteral("#ffd54a")));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(160, 40, 240, 240);
    painter.end();
    const auto path = QDir(args[3]).filePath(QStringLiteral("panel-custom.png"));
    if (!fixture.save(path))
        return 6;
    const auto icon = editor.importIcon(QUrl::fromLocalFile(path));
    if (icon.isEmpty())
        return 7;
    auto button = original;
    button.insert(QStringLiteral("icon"), icon);
    if (!editor.setButton(0, button))
        return 8;
    const auto program = QDir(app.applicationDirPath()).filePath(QStringLiteral("kdeconnect-app.exe"));
    const auto native = editor.programIcon(program);
    if (native.isEmpty())
        return 9;
    if (!editor.setButton(1,
                          {{QStringLiteral("name"), QStringLiteral("程序图标")},
                           {QStringLiteral("kind"), QStringLiteral("program")},
                           {QStringLiteral("target"), QVariantMap{{QStringLiteral("path"), program}}}}))
        return 10;
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly))
        return 11;
    output.write(QJsonDocument(QJsonObject{{QStringLiteral("custom"), icon},
                                           {QStringLiteral("native"), native},
                                           {QStringLiteral("revision"), QJsonValue::fromVariant(editor.config().value(QStringLiteral("revision")))},
                                           {QStringLiteral("success"), true}})
                     .toJson(QJsonDocument::Compact));
    output.flush();
    return 0;
}
