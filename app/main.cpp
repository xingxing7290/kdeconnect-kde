/**
 * SPDX-FileCopyrightText: 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QImage>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>

#include "kdeconnect-version.h"
#include <BreezeIcons>
#include <KAboutData>
#include <KColorSchemeManager>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KWindowSystem>
#include <paneleditor.h>

#if defined(Q_OS_MAC)
#include "macosforegroundapp.h"
#endif

static void raiseWindow(QWindow *window)
{
#if defined(Q_OS_MAC)
    activateWindowForMacOS(window);
#else
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
#endif
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    BreezeIcons::initIcons();
    KLocalizedString::setApplicationDomain("kdeconnect-app");
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("kdeconnect")));
    KAboutData aboutData(QStringLiteral("kdeconnect.app"),
                         i18n("KDE Connect"),
                         QStringLiteral(KDECONNECT_VERSION_STRING),
                         i18n("KDE Connect"),
                         KAboutLicense::GPL,
                         i18n("© 2015–2025 KDE Connect Team"));
    aboutData.addAuthor(i18n("Aleix Pol Gonzalez"), {}, QStringLiteral("aleixpol@kde.org"));
    aboutData.addAuthor(i18n("Albert Vaca Cintora"), {}, QStringLiteral("albertvaka@kde.org"));
    aboutData.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"), i18nc("EMAIL OF TRANSLATORS", "Your emails"));
    aboutData.setBugAddress("https://bugs.kde.org/enter_bug.cgi?product=kdeconnect&component=common");
    aboutData.setProgramLogo(QIcon::fromTheme(QStringLiteral("kdeconnect")));
    KAboutData::setApplicationData(aboutData);

    KCrash::initialize();

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    // Ensure we have a suitable color theme set for light/dark mode. KColorSchemeManager implicitly applies
    // a suitable default theme.
    KColorSchemeManager::instance();
    // Force breeze style to ensure coloring works consistently in dark mode. Specifically tab colors have
    // troubles on windows.
    QApplication::setStyle(QStringLiteral("breeze"));
    // Force breeze icon theme to ensure we can correctly adapt icons to color changes WRT dark/light mode.
    // Without this we may end up with hicolor and fail to support icon recoloring.
    QIcon::setThemeName(QStringLiteral("breeze"));
#else
    QIcon::setFallbackThemeName(QStringLiteral("breeze"));
#endif

    // Keep the control panel and device manager styling consistent.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
    }

    QString device;
    QString config;
    QString screenshotPath;
    bool replace = false;
    {
        QCommandLineParser parser;
        QCommandLineOption deviceOption(QStringLiteral("device"), i18nc("@info:shell", "Device id to open the app in"), QStringLiteral("device-id"));
        parser.addOption(deviceOption);
        QCommandLineOption configOption(QStringLiteral("plugin-config"),
                                        i18nc("@info:shell", "Configuration module to show, requires --device to be set."),
                                        QStringLiteral("plugin-id"));
        parser.addOption(configOption);
        QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                            i18nc("@info:shell", "Save a window image and exit for visual regression testing."),
                                            QStringLiteral("png-path"));
        parser.addOption(screenshotOption);

        QCommandLineOption replaceOption({QStringLiteral("replace")}, i18nc("command line description", "Replace an existing instance"));
        parser.addOption(replaceOption);

        parser.addPositionalArgument(QStringLiteral("url"), i18n("URL to share"));
        aboutData.setupCommandLine(&parser);
        parser.process(app);
        aboutData.processCommandLine(&parser);
        if (parser.positionalArguments().count() == 1) {
            const QString urlToShare = parser.positionalArguments().constFirst();
            const QString kdeconnectHandlerExecutable =
                QStandardPaths::findExecutable(QStringLiteral("kdeconnect-handler"), {QCoreApplication::applicationDirPath()});
            if (!kdeconnectHandlerExecutable.isEmpty()) {
                QProcess::startDetached(kdeconnectHandlerExecutable, {urlToShare});
                return 0; // exit the app once kdeconnect-handler is started
            }
        }
        replace = parser.isSet(replaceOption);
        screenshotPath = parser.value(screenshotOption);
        if (parser.isSet(deviceOption)) {
            device = parser.value(deviceOption);
            replace = true;
        }
        if (parser.isSet(configOption)) {
            config = parser.value(configOption);
        }
    }

    KDBusService::StartupOptions flags = KDBusService::Unique;
    if (replace) {
        flags |= KDBusService::Replace;
    }
    KDBusService dbusService(flags);
    ControlPanel::PanelEditor panelEditor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("panelEditor"), &panelEditor);
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("org.kde.kdeconnect.app", "Main");

    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load the app" << engine.hasError();
        return 1;
    }
    auto obj = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    if (!device.isEmpty()) {
        auto mo = obj->metaObject();
        mo->invokeMethod(obj, "openDevice", Qt::QueuedConnection, Q_ARG(QVariant, device), Q_ARG(QVariant, config));
    }
    raiseWindow(obj);
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(2000, &app, [obj, screenshotPath] {
            auto *window = qobject_cast<QQuickWindow *>(obj);
            const bool saved = window && window->grabWindow().save(screenshotPath, "PNG");
            QCoreApplication::exit(saved ? 0 : 3);
        });
    }
    QObject::connect(&dbusService, &KDBusService::activateRequested, obj, [obj](const QStringList & /*arguments*/, const QString & /*workingDirectory*/) {
        raiseWindow(obj);
    });
    return app.exec();
}
