/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <panelweather.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2)
        return 2;
    QTemporaryDir temporary;
    ControlPanel::PanelWeather weather(temporary.path(), nullptr, nullptr, true);
    QObject::connect(&weather, &ControlPanel::PanelWeather::changed, &app, [&] {
        const auto status = weather.state().value(QStringLiteral("status")).toString();
        if (status != QLatin1String("ready") && status != QLatin1String("unavailable"))
            return;
        QFile output;
        output.open(stdout, QIODevice::WriteOnly);
        output.write(QJsonDocument(weather.state()).toJson(QJsonDocument::Compact));
        output.flush();
        app.exit(status == QLatin1String("ready") ? 0 : 1);
    });
    QTimer::singleShot(55000, &app, [&] {
        app.exit(3);
    });
    QTimer::singleShot(0, &weather, [&] {
        weather.configure({{QStringLiteral("enabled"), true}, {QStringLiteral("city"), app.arguments()[1]}});
    });
    return app.exec();
}
