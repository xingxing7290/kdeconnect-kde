// SPDX-License-Identifier: GPL-2.0-or-later
#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QTextStream>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>
#include <windows.h>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (app.arguments().size() != 4) {
        return 2;
    }
    const QString package = QDir(app.arguments()[1]).absolutePath();
    const QString file = app.arguments()[2];
    const QString mode = app.arguments()[3];
    if (mode != QStringLiteral("audio") && mode != QStringLiteral("video") && mode != QStringLiteral("seek") && mode != QStringLiteral("invalid")) {
        return 2;
    }
    QCoreApplication::setLibraryPaths({package + QStringLiteral("/plugins")});
    QMediaPlayer player;
    QVideoSink video;
    QAudioBufferOutput audio;
    // Capture decoded audio in memory only. Never create QAudioOutput or touch host audio.
    player.setAudioBufferOutput(&audio);
    player.setVideoSink(&video);
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    int audioBuffers = 0;
    int videoFrames = 0;
    qint64 audioFrames = 0;
    bool timeout = false;
    bool ended = false;
    bool failed = false;
    bool seekRequested = false;
    bool paused = false;
    bool seekObserved = false;
    QByteArray imageHash;
    QObject::connect(&audio, &QAudioBufferOutput::audioBufferReceived, &loop, [&](const QAudioBuffer &buffer) {
        if (buffer.isValid() && buffer.byteCount() > 0) {
            ++audioBuffers;
            audioFrames += buffer.frameCount();
        }
    });
    QObject::connect(&video, &QVideoSink::videoFrameChanged, &loop, [&](const QVideoFrame &frame) {
        if (!frame.isValid()) {
            return;
        }
        const QImage image = frame.toImage();
        if (!image.isNull() && image.width() > 0 && image.height() > 0) {
            ++videoFrames;
            if (imageHash.isEmpty()) {
                imageHash =
                    QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()), QCryptographicHash::Sha256)
                        .toHex();
            }
        }
    });
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop, [&](QMediaPlayer::Error, const QString &) {
        failed = true;
        loop.quit();
    });
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            ended = true;
            loop.quit();
        }
    });
    QObject::connect(&player, &QMediaPlayer::positionChanged, &loop, [&](qint64 position) {
        if (mode == QStringLiteral("seek") && !seekRequested && position >= 150 && player.isSeekable()) {
            seekRequested = true;
            player.pause();
            paused = player.playbackState() == QMediaPlayer::PausedState;
            player.setPosition(700);
            player.setPlaybackRate(1.25);
            QTimer::singleShot(50, &player, &QMediaPlayer::play);
        }
        if (seekRequested && position >= 650) {
            seekObserved = true;
        }
    });
    QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
        timeout = true;
        loop.quit();
    });
    deadline.start(15000);
    player.setSource(QUrl::fromLocalFile(QFileInfo(file).absoluteFilePath()));
    player.play();
    loop.exec();
    const qint64 duration = player.duration();
    player.stop();
    const HMODULE codec = GetModuleHandleW(L"avcodec-62.dll");
    wchar_t moduleName[32768] = {};
    GetModuleFileNameW(codec, moduleName, 32768);
    const QString codecPath = QDir::fromNativeSeparators(QString::fromWCharArray(moduleName));
    wchar_t runtimeName[32768] = {};
    GetModuleFileNameW(GetModuleHandleW(L"vcruntime140.dll"), runtimeName, 32768);
    const QString runtimePath = QDir::fromNativeSeparators(QString::fromWCharArray(runtimeName));
    using StringFunction = const char *(*)();
    const auto licenseFunction = reinterpret_cast<StringFunction>(GetProcAddress(codec, "avcodec_license"));
    const auto configFunction = reinterpret_cast<StringFunction>(GetProcAddress(codec, "avcodec_configuration"));
    const QString license = licenseFunction ? QString::fromUtf8(licenseFunction()) : QString();
    const QString configuration = configFunction ? QString::fromUtf8(configFunction()) : QString();
    const bool correctLibrary = codec && codecPath.compare(package + QStringLiteral("/bin/avcodec-62.dll"), Qt::CaseInsensitive) == 0
        && runtimePath.compare(package + QStringLiteral("/bin/vcruntime140.dll"), Qt::CaseInsensitive) == 0;
    const bool distributable = !license.isEmpty() && !license.contains(QStringLiteral("nonfree"), Qt::CaseInsensitive)
        && !license.contains(QStringLiteral("unredistributable"), Qt::CaseInsensitive) && !configuration.contains(QStringLiteral("--enable-nonfree"));
    const bool invalid = mode == QStringLiteral("invalid");
    const bool contentPass = invalid ? failed
                                     : (!failed && ended && duration >= 500 && audioFrames > 0 && (mode == QStringLiteral("audio") || videoFrames > 0)
                                        && (mode != QStringLiteral("seek") || (paused && seekObserved)));
    const bool passed = !timeout && correctLibrary && distributable && contentPass;
    const QJsonObject result{{QStringLiteral("file"), QFileInfo(file).fileName()},
                             {QStringLiteral("mode"), mode},
                             {QStringLiteral("passed"), passed},
                             {QStringLiteral("contentPass"), contentPass},
                             {QStringLiteral("correctLibrary"), correctLibrary},
                             {QStringLiteral("distributable"), distributable},
                             {QStringLiteral("timeout"), timeout},
                             {QStringLiteral("error"), player.errorString()},
                             {QStringLiteral("ended"), ended},
                             {QStringLiteral("durationMs"), duration},
                             {QStringLiteral("audioBuffers"), audioBuffers},
                             {QStringLiteral("audioFrames"), audioFrames},
                             {QStringLiteral("videoFrames"), videoFrames},
                             {QStringLiteral("imageSha256"), QString::fromLatin1(imageHash)},
                             {QStringLiteral("paused"), paused},
                             {QStringLiteral("seekObserved"), seekObserved},
                             {QStringLiteral("codecPath"), codecPath},
                             {QStringLiteral("runtimePath"), runtimePath},
                             {QStringLiteral("license"), license}};
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << Qt::endl;
    return passed ? 0 : 1;
}
