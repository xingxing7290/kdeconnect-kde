/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include <core/kdeconnectplugin.h>
#include <panelpeer.h>

class ControlPanelPlugin : public KdeConnectPlugin
{
    Q_OBJECT
public:
    explicit ControlPanelPlugin(QObject *parent, const QVariantList &args);
    void connected() override;
    void receivePacket(const NetworkPacket &packet) override;

private:
    QString executeButton(const QJsonObject &button);
    ControlPanel::PanelPeer *m_peer;
};
