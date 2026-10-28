/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QWindow>
#include <panelforeground.h>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

// Manual interactive check: creates only its own temporary window and child process.
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--window"))) {
        QWindow window;
        window.setTitle(QStringLiteral("KDE Connect foreground integration fixture"));
        window.resize(540, 220);
        window.show();
        window.requestActivate();
        QTimer::singleShot(1000, &window, &QWindow::requestActivate);
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    ControlPanel::PanelForeground monitor;
    QProcess fixture;
    const auto expected = QFileInfo(app.applicationFilePath()).fileName().toLower();
    QObject::connect(&monitor, &ControlPanel::PanelForeground::applicationChanged, &app, [&] {
        if (monitor.application() != expected)
            return;
        QFile output;
        if (!output.open(stdout, QIODevice::WriteOnly)) {
            app.exit(4);
            return;
        }
        output.write(QJsonDocument(QJsonObject{{QStringLiteral("nativeForeground"), monitor.application()},
                                               {QStringLiteral("expected"), expected},
                                               {QStringLiteral("success"), true}})
                         .toJson(QJsonDocument::Compact));
        output.write("\n");
        output.flush();
        app.exit(0);
    });
    QObject::connect(&fixture, &QProcess::errorOccurred, &app, [&] {
        app.exit(2);
    });
    QTimer::singleShot(8000, &app, [&] {
        app.exit(3);
    });
    monitor.setEnabled(true);
#ifdef Q_OS_WIN
    fixture.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
        arguments->startupInfo->wShowWindow = SW_SHOWNORMAL;
    });
#endif
    fixture.start(app.applicationFilePath(), {QStringLiteral("--window")});
    const int result = app.exec();
    fixture.terminate();
    if (!fixture.waitForFinished(2000)) {
        fixture.kill();
        fixture.waitForFinished(2000);
    }
    return result;
}
