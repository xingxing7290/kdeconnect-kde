/**
 * SPDX-FileCopyrightText: 2018 Jun Bo Bi <jambonmcyeah@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "systemvolumeplugin-win.h"
#include "plugin_systemvolume_debug.h"
#include <KPluginFactory>
#include <QJsonDocument>
#include <volumecontroller.h>

K_PLUGIN_CLASS_WITH_JSON(SystemvolumePlugin, "kdeconnect_systemvolume.json")

SystemvolumePlugin::SystemvolumePlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_volume(SystemVolume::VolumeController::instance())
{
    connect(m_volume, &SystemVolume::VolumeController::changed, this, &SystemvolumePlugin::sendSinkList);
}
void SystemvolumePlugin::sendSinkList()
{
    if (!device()->isPaired() || !device()->isReachable())
        return;
    NetworkPacket packet(PACKET_TYPE_SYSTEMVOLUME);
    packet.set<QJsonDocument>(QStringLiteral("sinkList"), QJsonDocument(m_volume->sinkList()));
    sendPacket(packet);
}
void SystemvolumePlugin::connected()
{
    m_volume->refresh();
    sendSinkList();
}
void SystemvolumePlugin::receivePacket(const NetworkPacket &packet)
{
    if (!device()->isPaired() || !device()->isReachable() || packet.hasPayload())
        return;
    const auto error = m_volume->applyRequest(QJsonObject::fromVariantMap(packet.body()));
    if (!error.isEmpty())
        qCWarning(KDECONNECT_PLUGIN_SYSTEMVOLUME) << "Volume request rejected:" << error;
    sendSinkList();
}
#include "moc_systemvolumeplugin-win.cpp"
#include "systemvolumeplugin-win.moc"
