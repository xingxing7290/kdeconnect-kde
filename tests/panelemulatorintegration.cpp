/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <BreezeIcons>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <paneleditor.h>

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    BreezeIcons::initIcons();
    QIcon::setThemeName(QStringLiteral("breeze"));
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
    if (args[1] == QLatin1String("--unified-direction") || args[1] == QLatin1String("--unified-dial")) {
        const bool dialOperation = args[1] == QLatin1String("--unified-dial");
        const QStringList allowed = dialOperation ? QStringList{QStringLiteral("digital"),
                                                                QStringLiteral("analog"),
                                                                QStringLiteral("weather"),
                                                                QStringLiteral("info"),
                                                                QStringLiteral("car"),
                                                                QStringLiteral("disabled")}
                                                  : QStringList{QStringLiteral("horizontal"), QStringLiteral("vertical")};
        if (args.size() != 4 || !allowed.contains(args[3]))
            return 32;
        ControlPanel::PanelEditor editor(profile.filePath(QStringLiteral("panel")));
        const auto pages = editor.config().value(QStringLiteral("pages")).toList();
        if (!editor.error().isEmpty() || pages.size() != 10)
            return 33;
        for (int pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
            const auto buttons = pages[pageIndex].toMap().value(QStringLiteral("slots")).toList();
            if (buttons.size() != 18)
                return 34;
            const auto first = buttons[0].toMap();
            const auto target = first.value(QStringLiteral("target")).toMap();
            const auto arguments = target.value(QStringLiteral("arguments")).toList();
            const auto name = pageIndex == 0 ? QStringLiteral("E2E 首页图标") : QStringLiteral("E2E 第%1页图标").arg(pageIndex + 1);
            if (first.value(QStringLiteral("name")).toString() != name || first.value(QStringLiteral("kind")).toString() != QLatin1String("program")
                || target.value(QStringLiteral("path")).toString() != app.applicationFilePath() || arguments.size() != 4
                || arguments[0].toString() != QLatin1String("--probe") || arguments[1].toString() != profile.absolutePath()
                || !QRegularExpression(QStringLiteral("^[a-f0-9]{32}$")).match(arguments[2].toString()).hasMatch()
                || arguments[3].toString() != QStringLiteral("E2E 中文参数 with spaces"))
                return 35;
            for (int slotIndex = 1; slotIndex < buttons.size(); ++slotIndex) {
                if (pageIndex == 0 && slotIndex <= 2) {
                    const auto system = buttons[slotIndex].toMap();
                    const auto action = slotIndex == 1 ? QStringLiteral("thisPc") : QStringLiteral("recycleBin");
                    if (system.value(QStringLiteral("kind")).toString() != QLatin1String("system")
                        || system.value(QStringLiteral("target")).toMap().value(QStringLiteral("action")).toString() != action)
                        return 36;
                } else if (!buttons[slotIndex].isNull()) {
                    return 37;
                }
            }
        }
        const bool saved = dialOperation
            ? editor.setIdleOptions({{QStringLiteral("enabled"), args[3] != QLatin1String("disabled")},
                                     {QStringLiteral("timeoutMs"), 5000},
                                     {QStringLiteral("dialId"), args[3] == QLatin1String("disabled") ? QStringLiteral("digital") : args[3]},
                                     {QStringLiteral("city"), args[3] == QLatin1String("disabled") ? QString() : QStringLiteral("长沙市")},
                                     {QStringLiteral("message"), args[3] == QLatin1String("disabled") ? QString() : QStringLiteral("E2E 表盘唤醒验证")}})
            : editor.setAppearance(QStringLiteral("swipeDirection"), args[3]);
        if (!saved)
            return 38;
        QFile output;
        if (!output.open(stdout, QIODevice::WriteOnly))
            return 39;
        output.write(QJsonDocument(QJsonObject{{dialOperation ? QStringLiteral("dial") : QStringLiteral("direction"), args[3]},
                                               {QStringLiteral("revision"), QJsonValue::fromVariant(editor.config().value(QStringLiteral("revision")))}})
                         .toJson(QJsonDocument::Compact));
        output.flush();
        return 0;
    }
    if ((args[1] != QLatin1String("--prepare") && args[1] != QLatin1String("--extend-pages") && args[1] != QLatin1String("--extend-unified"))
        || args.size() != 3)
        return 9;
    ControlPanel::PanelEditor editor(profile.filePath(QStringLiteral("panel")));
    const auto config = editor.config();
    if (!editor.error().isEmpty() || config.value(QStringLiteral("pages")).toList().size() != 1)
        return 10;
    const auto slots = editor.currentPage().value(QStringLiteral("slots")).toList();
    if (args[1] == QLatin1String("--extend-pages") || args[1] == QLatin1String("--extend-unified")) {
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
        if (args[1] == QLatin1String("--extend-unified")) {
            QJsonArray expectedPages;
            expectedPages.append(QJsonObject{{QStringLiteral("id"), homePage},
                                             {QStringLiteral("name"), QStringLiteral("E2E 首页图标")},
                                             {QStringLiteral("token"), originalArguments[2].toString()}});
            const QList<QPair<QString, QString>> systemButtons{{QStringLiteral("E2E 此电脑"), QStringLiteral("thisPc")},
                                                               {QStringLiteral("E2E 回收站"), QStringLiteral("recycleBin")}};
            if (!editor.selectPage(homePage))
                return 23;
            QJsonArray systemIcons;
            for (int index = 0; index < systemButtons.size(); ++index) {
                const auto &[name, action] = systemButtons[index];
                if (!editor.setButton(index + 1,
                                      {{QStringLiteral("kind"), QStringLiteral("system")},
                                       {QStringLiteral("name"), name},
                                       {QStringLiteral("target"), QVariantMap{{QStringLiteral("action"), action}}}}))
                    return 24;
                const auto saved = editor.currentPage().value(QStringLiteral("slots")).toList()[index + 1].toMap();
                const auto defaultIcon = saved.value(QStringLiteral("icon")).toString();
                if (!ControlPanel::PanelAssets::validId(defaultIcon))
                    return 25;
                systemIcons.append(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("icon"), defaultIcon}});
            }
            if (systemIcons[0].toObject().value(QStringLiteral("icon")) == systemIcons[1].toObject().value(QStringLiteral("icon")))
                return 26;
            const auto secondPage = editor.config().value(QStringLiteral("pages")).toList()[1].toMap().value(QStringLiteral("id")).toString();
            if (!editor.selectPage(secondPage))
                return 27;
            for (int number = 2; number <= 10; ++number) {
                if (number > 2 && !editor.addPage())
                    return 28;
                const auto pageToken = QUuid::createUuid().toString(QUuid::Id128);
                auto pageTarget = target;
                auto pageArguments = originalArguments;
                pageArguments[2] = pageToken;
                pageTarget.insert(QStringLiteral("arguments"), pageArguments);
                const auto name = QStringLiteral("E2E 第%1页图标").arg(number);
                if (!editor.renamePage(editor.currentPageId(), name, QString())
                    || !editor.setButton(0,
                                         {{QStringLiteral("name"), name},
                                          {QStringLiteral("kind"), QStringLiteral("program")},
                                          {QStringLiteral("icon"), icon},
                                          {QStringLiteral("target"), pageTarget}}))
                    return 29;
                expectedPages.append(
                    QJsonObject{{QStringLiteral("id"), editor.currentPageId()}, {QStringLiteral("name"), name}, {QStringLiteral("token"), pageToken}});
            }
            const auto beforeLimit = editor.config();
            if (editor.addPage() || editor.config() != beforeLimit || !editor.selectPage(homePage))
                return 30;
            QFile output;
            if (!output.open(stdout, QIODevice::WriteOnly))
                return 31;
            output.write(QJsonDocument(QJsonObject{{QStringLiteral("icon"), icon},
                                                   {QStringLiteral("pages"), expectedPages},
                                                   {QStringLiteral("systemIcons"), systemIcons},
                                                   {QStringLiteral("eleventhPageRefused"), true},
                                                   {QStringLiteral("revision"), QJsonValue::fromVariant(editor.config().value(QStringLiteral("revision")))}})
                             .toJson(QJsonDocument::Compact));
            output.flush();
            return 0;
        }
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
