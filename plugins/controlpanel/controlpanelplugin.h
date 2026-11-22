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
    void publishWeather();
    void publishProgress();
    void publishHass();
    /** Older peers reject an unknown dialId, so the config is downgraded for them before sending. */
    QJsonObject configForPeer(const QJsonObject &body) const;
    void recordPeerCompatibility(bool supportsHass) const;
    QString executeButton(const QJsonObject &button);
    ControlPanel::PanelPeer *m_peer;
};
