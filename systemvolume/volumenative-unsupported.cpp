/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "volumecontroller.h"
namespace SystemVolume
{
class UnsupportedAudioSystem : public AudioSystem
{
public:
    QList<Sink> read(QString *error) override
    {
        *error = QStringLiteral("unsupported_platform");
        return {};
    }
    QString setVolume(const QString &, int) override
    {
        return QStringLiteral("unsupported_platform");
    }
    QString setMuted(const QString &, bool) override
    {
        return QStringLiteral("unsupported_platform");
    }
    QString setDefault(const QString &) override
    {
        return QStringLiteral("unsupported_platform");
    }
};
std::unique_ptr<AudioSystem> createNativeAudioSystem()
{
    return std::make_unique<UnsupportedAudioSystem>();
}
}
