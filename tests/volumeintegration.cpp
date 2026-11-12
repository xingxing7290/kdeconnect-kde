/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "../plugins/systemvolume/PolicyConfig.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <algorithm>
#include <array>
#include <mmdeviceapi.h>
#include <qt_windows.h>
#include <utility>
#include <volumecontroller.h>
#include <wrl/client.h>

using namespace SystemVolume;
using Microsoft::WRL::ComPtr;
static const std::array<std::pair<QString, ERole>, 3> roles{
    {{QStringLiteral("console"), eConsole}, {QStringLiteral("multimedia"), eMultimedia}, {QStringLiteral("communications"), eCommunications}}};
static bool waitFor(const std::function<bool()> &condition);
static QJsonObject defaultRoles()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator))))
        return {};
    QJsonObject result;
    for (const auto &[name, role] : roles) {
        ComPtr<IMMDevice> device;
        if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, role, &device)))
            return {};
        LPWSTR text = nullptr;
        const auto status = device->GetId(&text);
        const auto id = SUCCEEDED(status) && text ? QString::fromWCharArray(text) : QString();
        CoTaskMemFree(text);
        if (id.isEmpty())
            return {};
        result.insert(name, id);
    }
    return result;
}
static QJsonObject snapshot(const VolumeController &controller)
{
    const auto defaults = defaultRoles();
    if (!controller.available() || !controller.error().isEmpty() || defaults.size() != 3
        || defaults.value(QStringLiteral("multimedia")) != controller.defaultId())
        return {};
    const auto sinks = controller.sinkList();
    for (const auto &role : roles) {
        const auto id = defaults.value(role.first).toString();
        if (std::none_of(sinks.cbegin(), sinks.cend(), [&id](const QJsonValue &value) {
                return value.toObject().value(QStringLiteral("name")) == id;
            }))
            return {};
    }
    return {{QStringLiteral("schemaVersion"), 2}, {QStringLiteral("sinks"), sinks}, {QStringLiteral("defaults"), defaults}};
}
static bool restore(const QJsonObject &snapshot)
{
    const auto baseline = snapshot.value(QStringLiteral("sinks")).toArray();
    const auto defaults = snapshot.value(QStringLiteral("defaults")).toObject();
    // Old snapshots recorded only multimedia and cannot safely restore all roles.
    if (snapshot.value(QStringLiteral("schemaVersion")) != QJsonValue(2) || baseline.isEmpty() || baseline.size() > 128 || defaults.size() != 3)
        return false;
    int defaultCount = 0;
    for (const auto &value : baseline) {
        const auto entry = value.toObject();
        const auto volume = entry.value(QStringLiteral("volume"));
        if (!entry.value(QStringLiteral("name")).isString() || entry.value(QStringLiteral("name")).toString().isEmpty() || !volume.isDouble()
            || volume.toDouble() < 0 || volume.toDouble() > 100 || volume.toInt(-1) != volume.toDouble() || !entry.value(QStringLiteral("muted")).isBool()
            || !entry.value(QStringLiteral("enabled")).isBool())
            return false;
        defaultCount += entry.value(QStringLiteral("enabled")).toBool();
        if (entry.value(QStringLiteral("enabled")).toBool() && entry.value(QStringLiteral("name")) != defaults.value(QStringLiteral("multimedia")))
            return false;
    }
    if (defaultCount != 1)
        return false;
    auto native = createNativeAudioSystem();
    QString readError;
    const auto current = native->read(&readError);
    if (!readError.isEmpty() || defaultRoles().size() != 3)
        return false;
    for (const auto &value : baseline) {
        const auto id = value.toObject().value(QStringLiteral("name")).toString();
        if (std::none_of(current.cbegin(), current.cend(), [&id](const Sink &sink) {
                return sink.id == id;
            }))
            return false;
    }
    for (const auto &[name, role] : roles) {
        Q_UNUSED(role)
        const auto id = defaults.value(name).toString();
        if (id.isEmpty() || std::none_of(current.cbegin(), current.cend(), [&id](const Sink &sink) {
                return sink.id == id;
            }))
            return false;
    }
    ComPtr<IPolicyConfigVista> policy;
    if (FAILED(CoCreateInstance(__uuidof(CPolicyConfigVistaClient), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policy))))
        return false;
    bool success = true;
    for (const auto &[name, role] : roles) {
        const auto id = defaults.value(name).toString();
        success = SUCCEEDED(policy->SetDefaultEndpoint(reinterpret_cast<LPCWSTR>(id.utf16()), role)) && success;
    }
    success = waitFor([&] {
                  return defaultRoles() == defaults;
              })
        && success;
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
    QJsonObject result{{QStringLiteral("sinks"), controller.sinkList()},
                       {QStringLiteral("defaultRoles"), defaultRoles()},
                       {QStringLiteral("error"), controller.error()}};
    bool success = true;
    if (args.size() == 3 && args[1] == QLatin1String("--snapshot")) {
        const auto baseline = snapshot(controller);
        QSaveFile file(args[2]);
        success = !baseline.isEmpty() && !QFileInfo::exists(args[2]) && file.open(QIODevice::WriteOnly);
        if (success)
            success = file.write(QJsonDocument(baseline).toJson()) > 0 && file.commit();
    } else if (args.size() == 3 && args[1] == QLatin1String("--restore")) {
        QFile file(args[2]);
        success = file.open(QIODevice::ReadOnly) && file.size() <= 1024 * 1024;
        const auto baseline = success ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
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
        const auto baseline = snapshot(controller);
        if (baseline.isEmpty())
            return 2;
        const auto baselineSinks = baseline.value(QStringLiteral("sinks")).toArray();
        struct RestoreGuard {
            QJsonObject values;
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
        for (const auto &value : baselineSinks) {
            const auto other = value.toObject().value(QStringLiteral("name")).toString();
            if (other == id)
                continue;
            if (controller.setSinkMuted(other, true) && controller.setSinkVolume(other, 0))
                switched =
                    controller.setDefaultSink(other) && waitFor([&] {
                        const auto defaults = defaultRoles();
                        return controller.defaultId() == other && defaults.size() == 3 && std::all_of(roles.cbegin(), roles.cend(), [&](const auto &role) {
                                   return defaults.value(role.first).toString() == other;
                               });
                    });
            break;
        }
        const bool restored = restore(baseline);
        guard.done = restored;
        controller.refresh();
        const bool equal = controller.sinkList() == baselineSinks && defaultRoles() == baseline.value(QStringLiteral("defaults")).toObject();
        result.insert(QStringLiteral("volumeWrite"), volume);
        result.insert(QStringLiteral("externalPoll"), externalPoll);
        result.insert(QStringLiteral("muteWrite"), mute);
        result.insert(QStringLiteral("defaultSwitchAvailable"), baselineSinks.size() > 1);
        result.insert(QStringLiteral("defaultSwitch"), switched);
        result.insert(QStringLiteral("restored"), restored && equal);
        success = volume && externalPoll && mute && restored && equal && (baselineSinks.size() <= 1 || switched);
    } else if (args.size() != 1) {
        return 4;
    }
    controller.refresh();
    result.insert(QStringLiteral("currentSinks"), controller.sinkList());
    result.insert(QStringLiteral("currentDefaultRoles"), defaultRoles());
    result.insert(QStringLiteral("success"), success);
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly))
        return 3;
    output.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
    output.write("\n");
    output.flush();
    return success ? 0 : 1;
}
