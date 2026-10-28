/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <memory>

namespace SystemVolume
{
struct Sink {
    QString id;
    QString description;
    int volume = 0;
    bool muted = false;
    bool isDefault = false;
    bool operator==(const Sink &) const = default;
};

class AudioSystem
{
public:
    virtual ~AudioSystem() = default;
    virtual QList<Sink> read(QString *error) = 0;
    virtual QString setVolume(const QString &id, int volume) = 0;
    virtual QString setMuted(const QString &id, bool muted) = 0;
    virtual QString setDefault(const QString &id) = 0;
};
std::unique_ptr<AudioSystem> createNativeAudioSystem();

class VolumeController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(QString defaultId READ defaultId NOTIFY changed)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY changed)
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit VolumeController(QObject *parent = nullptr, std::unique_ptr<AudioSystem> system = {}, bool polling = true);
    ~VolumeController() override;
    static VolumeController *instance();
    QJsonArray sinkList() const;
    QVariantList outputs() const;
    QString defaultId() const;
    QString deviceName() const;
    int volume() const;
    bool muted() const;
    bool available() const;
    QString error() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setSinkVolume(const QString &id, int value);
    Q_INVOKABLE bool setSinkMuted(const QString &id, bool muted);
    Q_INVOKABLE bool setDefaultSink(const QString &id);
    QString applyRequest(const QJsonObject &request);
Q_SIGNALS:
    void changed();

private:
    const Sink *sink(const QString &id) const;
    const Sink *defaultSink() const;
    bool finish(const QString &error);
    std::unique_ptr<AudioSystem> m_system;
    QList<Sink> m_sinks;
    QString m_error;
    QString m_operationError;
    QTimer m_poll;
};
}
