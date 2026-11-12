/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "controlpanelplugin.h"
#include <KPluginFactory>
#include <QBuffer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPointer>
#include <panelactions.h>
#include <panelassets.h>
#include <panelforeground.h>

K_PLUGIN_CLASS_WITH_JSON(ControlPanelPlugin, "kdeconnect_controlpanel.json")

namespace
{
ControlPanel::PanelRuntime *runtime()
{
    static QPointer<ControlPanel::PanelRuntime> shared;
    if (!shared) {
        auto *store = new ControlPanel::PanelStore({}, QCoreApplication::instance());
        store->load();
        shared = new ControlPanel::PanelRuntime(store, store);
        auto *foreground = new ControlPanel::PanelForeground(shared);
        QObject::connect(foreground, &ControlPanel::PanelForeground::applicationChanged, shared, [foreground] {
            shared->setForegroundApplication(foreground->application());
        });
        const auto updateMonitor = [store, foreground] {
            foreground->setEnabled(store->isUsable() && store->document().privateConfig().value(QStringLiteral("mode")).toString() == QLatin1String("follow"));
        };
        QObject::connect(store, &ControlPanel::PanelStore::changed, foreground, updateMonitor);
        QObject::connect(store, &ControlPanel::PanelStore::errorOccurred, foreground, updateMonitor);
        updateMonitor();
    }
    return shared;
}
}

ControlPanelPlugin::ControlPanelPlugin(QObject *parent, const QVariantList &args)
    : KdeConnectPlugin(parent, args)
    , m_peer(new ControlPanel::PanelPeer(
          runtime(),
          [this](const QJsonObject &button) {
              return executeButton(button);
          },
          this))
{
    connect(m_peer, &ControlPanel::PanelPeer::packetReady, this, [this](const QString &type, const QJsonObject &body) {
        if (device()->isPaired() && device()->isReachable()) {
            NetworkPacket packet(type, body.toVariantMap());
            sendPacket(packet);
        }
    });
    connect(m_peer, &ControlPanel::PanelPeer::assetRequested, this, [this](const QString &id, qint64 revision, const QString &session) {
        if (!device()->isPaired() || !device()->isReachable())
            return;
        const ControlPanel::PanelAssets assets(QFileInfo(runtime()->store()->filePath()).absolutePath());
        const auto bytes = assets.read(id);
        NetworkPacket packet(QStringLiteral("kdeconnect.controlpanel.asset"),
                             {{QStringLiteral("protocolVersion"), 1},
                              {QStringLiteral("sessionId"), session},
                              {QStringLiteral("revision"), revision},
                              {QStringLiteral("assetId"), id},
                              {QStringLiteral("mime"), QStringLiteral("image/png")},
                              {QStringLiteral("size"), bytes ? bytes->size() : 0},
                              {QStringLiteral("error"), bytes ? QString() : QStringLiteral("asset_missing")}});
        if (bytes) {
            auto buffer = QSharedPointer<QBuffer>::create();
            buffer->setData(*bytes);
            buffer->open(QIODevice::ReadOnly);
            packet.setPayload(buffer, buffer->size());
        }
        sendPacket(packet);
    });
}

void ControlPanelPlugin::connected()
{
    if (device()->isPaired())
        m_peer->sendConfiguration();
}

void ControlPanelPlugin::receivePacket(const NetworkPacket &packet)
{
    if (device()->isPaired() && device()->isReachable() && packet.type() == QLatin1String("kdeconnect.controlpanel.request") && !packet.hasPayload()) {
        m_peer->receive(QJsonObject::fromVariantMap(packet.body()));
    }
}

QString ControlPanelPlugin::executeButton(const QJsonObject &button)
{
    if (button.value(QStringLiteral("kind")).toString() != QLatin1String("command"))
        return ControlPanel::PanelActions::execute(button);
    QString error;
    const auto action = ControlPanel::PanelActions::resolve(button, &error);
    if (!action)
        return error;
    auto *command = device()->plugin(QStringLiteral("kdeconnect_runcommand"));
    if (!command)
        return QStringLiteral("command_unavailable");
    const auto commands = QJsonDocument::fromJson(command->config()->getByteArray(QStringLiteral("commands"), QByteArrayLiteral("{}"))).object();
    const auto configured = commands.value(action->target).toObject();
    if (!configured.value(QStringLiteral("command")).isString() || configured.value(QStringLiteral("command")).toString().isEmpty()) {
        return QStringLiteral("unknown_command");
    }
    command->receivePacket(NetworkPacket(QStringLiteral("kdeconnect.runcommand.request"), {{QStringLiteral("key"), action->target}}));
    return {};
}

#include "controlpanelplugin.moc"
#include "moc_controlpanelplugin.cpp"
