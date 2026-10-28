/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "volumecontroller.h"
#include <QCoreApplication>
#include <QPointer>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace SystemVolume
{
VolumeController::VolumeController(QObject *parent, std::unique_ptr<AudioSystem> system, bool polling)
    : QObject(parent)
    , m_system(system ? std::move(system) : createNativeAudioSystem())
{
    m_poll.setInterval(500);
    connect(&m_poll, &QTimer::timeout, this, &VolumeController::refresh);
    refresh();
    if (polling)
        m_poll.start();
}
VolumeController::~VolumeController() = default;

VolumeController *VolumeController::instance()
{
    static QPointer<VolumeController> controller;
    if (!controller)
        controller = new VolumeController(QCoreApplication::instance());
    return controller;
}

const Sink *VolumeController::sink(const QString &id) const
{
    for (const auto &entry : m_sinks) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}
const Sink *VolumeController::defaultSink() const
{
    for (const auto &entry : m_sinks) {
        if (entry.isDefault)
            return &entry;
    }
    return nullptr;
}
QString VolumeController::defaultId() const
{
    const auto *entry = defaultSink();
    return entry ? entry->id : QString();
}
QString VolumeController::deviceName() const
{
    const auto *entry = defaultSink();
    return entry ? entry->description : QString();
}
int VolumeController::volume() const
{
    const auto *entry = defaultSink();
    return entry ? entry->volume : 0;
}
bool VolumeController::muted() const
{
    const auto *entry = defaultSink();
    return entry && entry->muted;
}
bool VolumeController::available() const
{
    return defaultSink() != nullptr;
}
QString VolumeController::error() const
{
    return m_error.isEmpty() ? m_operationError : m_error;
}

QJsonArray VolumeController::sinkList() const
{
    QJsonArray result;
    for (const auto &entry : m_sinks) {
        result.append(QJsonObject{{QStringLiteral("name"), entry.id},
                                  {QStringLiteral("description"), entry.description},
                                  {QStringLiteral("volume"), entry.volume},
                                  {QStringLiteral("muted"), entry.muted},
                                  {QStringLiteral("maxVolume"), 100},
                                  {QStringLiteral("enabled"), entry.isDefault}});
    }
    return result;
}
QVariantList VolumeController::outputs() const
{
    return sinkList().toVariantList();
}

void VolumeController::refresh()
{
    QString error;
    auto outputs = m_system->read(&error);
    if (!error.isEmpty())
        outputs.clear();
    std::sort(outputs.begin(), outputs.end(), [](const Sink &left, const Sink &right) {
        return left.id < right.id;
    });
    if (outputs == m_sinks && error == m_error)
        return;
    m_sinks = std::move(outputs);
    m_error = error;
    Q_EMIT changed();
}
bool VolumeController::finish(const QString &error)
{
    const bool changedError = m_operationError != error;
    m_operationError = error;
    refresh();
    if (changedError) {
        Q_EMIT changed();
    }
    return error.isEmpty();
}
bool VolumeController::setSinkVolume(const QString &id, int value)
{
    refresh();
    if (!sink(id))
        return finish(QStringLiteral("output_unavailable"));
    if (value < 0 || value > 100)
        return finish(QStringLiteral("invalid_volume"));
    return finish(m_system->setVolume(id, value));
}
bool VolumeController::setSinkMuted(const QString &id, bool value)
{
    refresh();
    return finish(sink(id) ? m_system->setMuted(id, value) : QStringLiteral("output_unavailable"));
}
bool VolumeController::setDefaultSink(const QString &id)
{
    refresh();
    return finish(sink(id) ? m_system->setDefault(id) : QStringLiteral("output_unavailable"));
}

QString VolumeController::applyRequest(const QJsonObject &request)
{
    static const QSet<QString> fields{QStringLiteral("requestSinks"),
                                      QStringLiteral("name"),
                                      QStringLiteral("volume"),
                                      QStringLiteral("muted"),
                                      QStringLiteral("enabled")};
    for (auto it = request.begin(); it != request.end(); ++it) {
        if (!fields.contains(it.key()))
            return QStringLiteral("invalid_request");
    }
    if (request.contains(QStringLiteral("requestSinks"))) {
        if (request.size() != 1 || request.value(QStringLiteral("requestSinks")) != QJsonValue(true))
            return QStringLiteral("invalid_request");
        refresh();
        return {};
    }
    if (!request.value(QStringLiteral("name")).isString() || request.size() < 2)
        return QStringLiteral("invalid_request");
    const auto value = request.value(QStringLiteral("volume"));
    if (!value.isUndefined()
        && (!value.isDouble() || !std::isfinite(value.toDouble()) || std::floor(value.toDouble()) != value.toDouble() || value.toDouble() < 0
            || value.toDouble() > 100))
        return QStringLiteral("invalid_volume");
    for (const auto &key : {QStringLiteral("muted"), QStringLiteral("enabled")}) {
        if (request.contains(key) && !request.value(key).isBool())
            return QStringLiteral("invalid_request");
    }
    refresh();
    const auto id = request.value(QStringLiteral("name")).toString();
    if (!sink(id))
        return QStringLiteral("output_unavailable");
    if (!value.isUndefined() && !setSinkVolume(id, value.toInt()))
        return error();
    if (request.contains(QStringLiteral("muted")) && !setSinkMuted(id, request.value(QStringLiteral("muted")).toBool()))
        return error();
    if (request.value(QStringLiteral("enabled")).toBool() && !setDefaultSink(id))
        return error();
    return {};
}
}
#include "moc_volumecontroller.cpp"
