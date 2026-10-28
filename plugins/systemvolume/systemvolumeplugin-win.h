/**
 * SPDX-FileCopyrightText: 2018 Jun Bo Bi <jambonmcyeah@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include <core/kdeconnectplugin.h>
#define PACKET_TYPE_SYSTEMVOLUME QStringLiteral("kdeconnect.systemvolume")
#define PACKET_TYPE_SYSTEMVOLUME_REQUEST QStringLiteral("kdeconnect.systemvolume.request")
namespace SystemVolume
{
class VolumeController;
}
class SystemvolumePlugin : public KdeConnectPlugin
{
    Q_OBJECT
public:
    explicit SystemvolumePlugin(QObject *parent, const QVariantList &args);
    void receivePacket(const NetworkPacket &packet) override;
    void connected() override;

private:
    void sendSinkList();
    SystemVolume::VolumeController *m_volume;
};
