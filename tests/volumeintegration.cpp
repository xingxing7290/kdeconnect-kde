/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <volumecontroller.h>

using namespace SystemVolume;
static bool waitFor(const std::function<bool()> &condition);
static bool restore(const QJsonArray &baseline)
{
    if (baseline.isEmpty() || baseline.size() > 128)
        return false;
    int defaults = 0;
    for (const auto &value : baseline) {
        const auto entry = value.toObject();
        const auto volume = entry.value(QStringLiteral("volume"));
        if (!entry.value(QStringLiteral("name")).isString() || entry.value(QStringLiteral("name")).toString().isEmpty() || !volume.isDouble()
            || volume.toDouble() < 0 || volume.toDouble() > 100 || volume.toInt(-1) != volume.toDouble() || !entry.value(QStringLiteral("muted")).isBool()
            || !entry.value(QStringLiteral("enabled")).isBool())
            return false;
        defaults += entry.value(QStringLiteral("enabled")).toBool();
    }
    if (defaults != 1)
        return false;
    auto native = createNativeAudioSystem();
    bool success = true;
    for (const auto &value : baseline) {
        const auto entry = value.toObject();
        if (entry.value(QStringLiteral("enabled")).toBool()) {
            const auto id = entry.value(QStringLiteral("name")).toString();
            if (!native->setDefault(id).isEmpty())
                return false;
            if (!waitFor([&] {
                    QString error;
                    for (const auto &sink : native->read(&error))
                        if (sink.id == id && sink.isDefault)
                            return true;
                    return false;
                }))
                return false;
        }
    }
    for (const auto &value : baseline) {
        const auto entry = value.toObject();
        const auto id = entry.value(QStringLiteral("name")).toString();
        success = native->setVolume(id, entry.value(QStringLiteral("volume")).toInt()).isEmpty() && success;
        success = native->setMuted(id, entry.value(QStringLiteral("muted")).toBool()).isEmpty() && success;
    }
    return success;
}
static bool waitFor(const std::function<bool()> &condition)
{
    if (condition())
        return true;
    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (condition())
            loop.quit();
    });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    poll.start(25);
    loop.exec();
    return condition();
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    VolumeController controller;
    const auto args = app.arguments();
    QJsonObject result{{QStringLiteral("sinks"), controller.sinkList()}, {QStringLiteral("error"), controller.error()}};
    bool success = true;
    if (args.size() == 3 && args[1] == QLatin1String("--snapshot")) {
        QSaveFile file(args[2]);
        success = controller.available() && !QFileInfo::exists(args[2]) && file.open(QIODevice::WriteOnly);
        if (success)
            success = file.write(QJsonDocument(controller.sinkList()).toJson()) > 0 && file.commit();
    } else if (args.size() == 3 && args[1] == QLatin1String("--restore")) {
        QFile file(args[2]);
        success = file.open(QIODevice::ReadOnly) && file.size() <= 1024 * 1024;
        const auto baseline = success ? QJsonDocument::fromJson(file.readAll()).array() : QJsonArray();
        success = success && !baseline.isEmpty() && restore(baseline);
    } else if (args.size() == 3 && args[1] == QLatin1String("--volume")) {
        bool valid = false;
        const int value = args[2].toInt(&valid);
        success = valid && controller.setSinkVolume(controller.defaultId(), value);
    } else if (args.size() == 3 && args[1] == QLatin1String("--mute")) {
        success = (args[2] == QLatin1String("true") || args[2] == QLatin1String("false"))
            && controller.setSinkMuted(controller.defaultId(), args[2] == QLatin1String("true"));
    } else if (args.size() == 3 && args[1] == QLatin1String("--default")) {
        success = controller.setDefaultSink(args[2]);
    } else if (args.size() == 3 && args[1] == QLatin1String("--silence-output")) {
        success = controller.setSinkMuted(args[2], true) && controller.setSinkVolume(args[2], 0);
    } else if (args.size() == 2 && args[1] == QLatin1String("--exercise")) {
        const auto baseline = controller.sinkList();
        struct RestoreGuard {
            QJsonArray values;
            bool done = false;
            ~RestoreGuard()
            {
                if (!done)
                    restore(values);
            }
        } guard{baseline};
        const auto id = controller.defaultId();
        if (id.isEmpty())
            return 2;
        const int lower = qMax(0, controller.volume() - 1);
        const bool volume = controller.setSinkVolume(id, lower) && controller.volume() == lower;
        auto external = createNativeAudioSystem();
        const int externalValue = qMax(0, lower - 1);
        const bool externalPoll = external->setVolume(id, externalValue).isEmpty() && waitFor([&] {
                                      return controller.volume() == externalValue;
                                  });
        const bool mute = controller.setSinkMuted(id, true) && controller.muted();
        bool switched = false;
        for (const auto &value : baseline) {
            const auto other = value.toObject().value(QStringLiteral("name")).toString();
            if (other == id)
                continue;
            if (controller.setSinkMuted(other, true) && controller.setSinkVolume(other, 0))
                switched = controller.setDefaultSink(other) && waitFor([&] {
                               return controller.defaultId() == other;
                           });
            break;
        }
        const bool restored = restore(baseline);
        guard.done = restored;
        controller.refresh();
        const bool equal = controller.sinkList() == baseline;
        result.insert(QStringLiteral("volumeWrite"), volume);
        result.insert(QStringLiteral("externalPoll"), externalPoll);
        result.insert(QStringLiteral("muteWrite"), mute);
        result.insert(QStringLiteral("defaultSwitchAvailable"), baseline.size() > 1);
        result.insert(QStringLiteral("defaultSwitch"), switched);
        result.insert(QStringLiteral("restored"), restored && equal);
        success = volume && externalPoll && mute && restored && equal && (baseline.size() <= 1 || switched);
    } else if (args.size() != 1) {
        return 4;
    }
    controller.refresh();
    result.insert(QStringLiteral("currentSinks"), controller.sinkList());
    result.insert(QStringLiteral("success"), success);
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly))
        return 3;
    output.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
    output.write("\n");
    output.flush();
    return success ? 0 : 1;
}
