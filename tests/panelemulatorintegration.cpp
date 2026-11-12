/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
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
    if (args[1] != QLatin1String("--prepare") || args.size() != 3)
        return 9;
    ControlPanel::PanelEditor editor(profile.filePath(QStringLiteral("panel")));
    const auto config = editor.config();
    if (!editor.error().isEmpty() || config.value(QStringLiteral("pages")).toList().size() != 1)
        return 10;
    const auto slots = editor.currentPage().value(QStringLiteral("slots")).toList();
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
