/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <paneleditor.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 3 || !QDir::isAbsolutePath(args[2]))
        return 2;
    const QDir profile(args[2]);
    QFile marker(profile.filePath(QStringLiteral(".kde-controlpanel-profile")));
    if (!marker.open(QIODevice::ReadOnly) || marker.readAll() != QByteArrayLiteral("KDE Control Panel private profile v1"))
        return 3;
    if (args[1] == QLatin1String("--probe")) {
        if (args.size() != 5 || !QRegularExpression(QStringLiteral("^[a-f0-9]{32}$")).match(args[3]).hasMatch())
            return 4;
        const QDir directory(profile.filePath(QStringLiteral("e2e-probe")));
        if (!directory.exists())
            return 5;
        const auto path = directory.filePath(args[3] + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".json"));
        if (QFile::exists(path))
            return 6;
        QSaveFile report(path);
        if (!report.open(QIODevice::WriteOnly))
            return 7;
        const auto bytes = QJsonDocument(QJsonObject{{QStringLiteral("token"), args[3]},
                                                     {QStringLiteral("argument"), args[4]},
                                                     {QStringLiteral("workingDirectory"), QDir::currentPath()}})
                               .toJson(QJsonDocument::Compact);
        if (report.write(bytes) != bytes.size() || !report.commit())
            return 8;
        return 0;
    }
    if ((args[1] != QLatin1String("--prepare") && args[1] != QLatin1String("--extend-pages")) || args.size() != 3)
        return 9;
    ControlPanel::PanelEditor editor(profile.filePath(QStringLiteral("panel")));
    const auto config = editor.config();
    if (!editor.error().isEmpty() || config.value(QStringLiteral("pages")).toList().size() != 1)
        return 10;
    const auto slots = editor.currentPage().value(QStringLiteral("slots")).toList();
    if (args[1] == QLatin1String("--extend-pages")) {
        const auto first = slots.value(0).toMap();
        const auto target = first.value(QStringLiteral("target")).toMap();
        const auto originalArguments = target.value(QStringLiteral("arguments")).toList();
        if (first.value(QStringLiteral("name")).toString() != QStringLiteral("E2E 参数验证")
            || target.value(QStringLiteral("path")).toString() != app.applicationFilePath() || originalArguments.size() != 4
            || originalArguments[0].toString() != QLatin1String("--probe") || originalArguments[1].toString() != profile.absolutePath())
            return 15;
        for (int index = 1; index < slots.size(); ++index) {
            if (!slots[index].isNull())
                return 16;
        }
        const QDir directory(profile.filePath(QStringLiteral("e2e-probe")));
        if (!directory.exists())
            return 17;
        QImage image(512, 256, QImage::Format_ARGB32);
        image.fill(QColor(QStringLiteral("#1457cc")));
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#ffca28")));
        painter.drawEllipse(160, 32, 192, 192);
        painter.end();
        const auto imagePath = directory.filePath(QStringLiteral("custom-source.png"));
        if (QFile::exists(imagePath) || !image.save(imagePath))
            return 18;
        const auto icon = editor.importIcon(QUrl::fromLocalFile(imagePath));
        if (icon.isEmpty())
            return 19;
        auto firstButton = first;
        firstButton.insert(QStringLiteral("name"), QStringLiteral("E2E 首页图标"));
        firstButton.insert(QStringLiteral("icon"), icon);
        const auto token = QUuid::createUuid().toString(QUuid::Id128);
        const auto homePage = editor.currentPageId();
        if (!editor.setButton(0, firstButton) || !editor.setMode(QStringLiteral("pages"))
            || !editor.setAppearance(QStringLiteral("background"), QStringLiteral("#DDEBFF")) || !editor.setAppearance(QStringLiteral("showVolume"), false)
            || !editor.setAppearance(QStringLiteral("swipeDirection"), QStringLiteral("vertical")) || !editor.addPage())
            return 20;
        const auto secondPage = editor.currentPageId();
        auto secondTarget = target;
        auto arguments = originalArguments;
        arguments[2] = token;
        secondTarget.insert(QStringLiteral("arguments"), arguments);
        if (!editor.renamePage(secondPage, QStringLiteral("E2E 第二页"), QString())
            || !editor.setButton(0,
                                 {{QStringLiteral("name"), QStringLiteral("E2E 第二页图标")},
                                  {QStringLiteral("kind"), QStringLiteral("program")},
                                  {QStringLiteral("icon"), icon},
                                  {QStringLiteral("target"), secondTarget}}))
            return 21;
        QFile output;
        if (!output.open(stdout, QIODevice::WriteOnly))
            return 22;
        output.write(QJsonDocument(QJsonObject{{QStringLiteral("icon"), icon},
                                               {QStringLiteral("homePage"), homePage},
                                               {QStringLiteral("secondPage"), secondPage},
                                               {QStringLiteral("token"), token},
                                               {QStringLiteral("revision"), QJsonValue::fromVariant(editor.config().value(QStringLiteral("revision")))}})
                         .toJson(QJsonDocument::Compact));
        output.flush();
        return 0;
    }
    for (const auto &slot : slots) {
        if (!slot.isNull())
            return 11;
    }
    const auto token = QUuid::createUuid().toString(QUuid::Id128);
    const auto argument = QStringLiteral("E2E 中文参数 with spaces");
    const auto workingDirectory = profile.filePath(QStringLiteral("e2e-probe/中文 工作目录"));
    if (!QDir().mkpath(workingDirectory))
        return 12;
    if (!editor.setIdle(QStringLiteral("enabled"), false)
        || !editor.setButton(0,
                             {{QStringLiteral("name"), QStringLiteral("E2E 参数验证")},
                              {QStringLiteral("kind"), QStringLiteral("program")},
                              {QStringLiteral("target"),
                               QVariantMap{{QStringLiteral("path"), app.applicationFilePath()},
                                           {QStringLiteral("arguments"), QVariantList{QStringLiteral("--probe"), profile.absolutePath(), token, argument}},
                                           {QStringLiteral("workingDirectory"), workingDirectory}}}}))
        return 13;
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly))
        return 14;
    output.write(QJsonDocument(QJsonObject{{QStringLiteral("token"), token},
                                           {QStringLiteral("argument"), argument},
                                           {QStringLiteral("workingDirectory"), workingDirectory},
                                           {QStringLiteral("revision"), QJsonValue::fromVariant(editor.config().value(QStringLiteral("revision")))}})
                     .toJson(QJsonDocument::Compact));
    output.flush();
    return 0;
}
