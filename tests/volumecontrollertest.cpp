/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include <QSignalSpy>
#include <QTest>
#include <volumecontroller.h>
using namespace SystemVolume;

class FakeAudioSystem : public AudioSystem
{
public:
    QList<Sink> outputs{{QStringLiteral("one"), QStringLiteral("Same name"), 23, false, true},
                        {QStringLiteral("two"), QStringLiteral("Same name"), 42, true, false}};
    QString failure;
    QString writeFailure;
    int writes = 0;
    QList<Sink> read(QString *error) override
    {
        *error = failure;
        return outputs;
    }
    QString setVolume(const QString &id, int volume) override
    {
        ++writes;
        if (!writeFailure.isEmpty())
            return writeFailure;
        for (auto &entry : outputs)
            if (entry.id == id)
                entry.volume = volume;
        return {};
    }
    QString setMuted(const QString &id, bool muted) override
    {
        ++writes;
        if (!writeFailure.isEmpty())
            return writeFailure;
        for (auto &entry : outputs)
            if (entry.id == id)
                entry.muted = muted;
        return {};
    }
    QString setDefault(const QString &id) override
    {
        ++writes;
        if (!writeFailure.isEmpty())
            return writeFailure;
        for (auto &entry : outputs)
            entry.isDefault = entry.id == id;
        return {};
    }
};

class VolumeControllerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void duplicateFriendlyNamesRemainIndependentAndExternalChangesArrive()
    {
        auto system = std::make_unique<FakeAudioSystem>();
        auto *fake = system.get();
        VolumeController controller(nullptr, std::move(system), false);
        QCOMPARE(controller.sinkList().size(), 2);
        QCOMPARE(controller.defaultId(), QStringLiteral("one"));
        QVERIFY(controller.setSinkVolume(QStringLiteral("two"), 19));
        QCOMPARE(controller.volume(), 23);
        QCOMPARE(fake->outputs[1].volume, 19);
        fake->outputs[0].volume = 31;
        fake->outputs[0].muted = true;
        QSignalSpy changed(&controller, &VolumeController::changed);
        controller.refresh();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(controller.volume(), 31);
        QVERIFY(controller.muted());
        controller.refresh();
        QCOMPARE(changed.size(), 1);
    }
    void changingDefaultAndRemovingOutputsNeverFallsBackToArbitrarySink()
    {
        auto system = std::make_unique<FakeAudioSystem>();
        auto *fake = system.get();
        VolumeController controller(nullptr, std::move(system), false);
        QVERIFY(controller.setDefaultSink(QStringLiteral("two")));
        QCOMPARE(controller.defaultId(), QStringLiteral("two"));
        QCOMPARE(controller.volume(), 42);
        fake->outputs.removeLast();
        controller.refresh();
        QVERIFY(!controller.available());
        QVERIFY(controller.defaultId().isEmpty());
        QVERIFY(!controller.setSinkMuted(QStringLiteral("two"), false));
        QVERIFY(controller.setDefaultSink(QStringLiteral("one")));
        QVERIFY(controller.available());
        fake->outputs.clear();
        controller.refresh();
        QVERIFY(!controller.available());
        QVERIFY(controller.sinkList().isEmpty());
    }
    void invalidRequestsAreFullyValidatedBeforeAnyWrite()
    {
        auto system = std::make_unique<FakeAudioSystem>();
        auto *fake = system.get();
        VolumeController controller(nullptr, std::move(system), false);
        const QList<QJsonObject> requests{
            {{QStringLiteral("name"), QStringLiteral("one")}, {QStringLiteral("volume"), -1}},
            {{QStringLiteral("name"), QStringLiteral("one")}, {QStringLiteral("volume"), 101}},
            {{QStringLiteral("name"), QStringLiteral("one")}, {QStringLiteral("volume"), 20.5}},
            {{QStringLiteral("name"), QStringLiteral("one")}, {QStringLiteral("volume"), QStringLiteral("20")}},
            {{QStringLiteral("name"), QStringLiteral("one")}, {QStringLiteral("volume"), 20}, {QStringLiteral("muted"), QStringLiteral("true")}},
            {{QStringLiteral("name"), QStringLiteral("missing")}, {QStringLiteral("muted"), true}},
            {{QStringLiteral("requestSinks"), false}},
            {{QStringLiteral("requestSinks"), true}, {QStringLiteral("volume"), 20}},
            {{QStringLiteral("name"), QStringLiteral("one")}},
            {{QStringLiteral("name"), QStringLiteral("one")}, {QStringLiteral("command"), QStringLiteral("anything")}}};
        for (const auto &request : requests)
            QVERIFY(!controller.applyRequest(request).isEmpty());
        QCOMPARE(fake->writes, 0);
        QVERIFY(controller.applyRequest({{QStringLiteral("requestSinks"), true}}).isEmpty());
        QVERIFY(controller.applyRequest({{QStringLiteral("name"), QStringLiteral("one")}, {QStringLiteral("volume"), 0}, {QStringLiteral("muted"), true}})
                    .isEmpty());
        QCOMPARE(fake->writes, 2);
        QCOMPARE(controller.volume(), 0);
        QVERIFY(controller.muted());
    }
    void failuresClearStaleStateAndRecoveryIsVisible()
    {
        auto system = std::make_unique<FakeAudioSystem>();
        auto *fake = system.get();
        VolumeController controller(nullptr, std::move(system), false);
        fake->failure = QStringLiteral("audio_unavailable");
        controller.refresh();
        QVERIFY(!controller.available());
        QVERIFY(controller.sinkList().isEmpty());
        fake->failure.clear();
        controller.refresh();
        QVERIFY(controller.available());
        fake->writeFailure = QStringLiteral("volume_failed");
        QVERIFY(!controller.setSinkVolume(QStringLiteral("one"), 51));
        QCOMPARE(controller.error(), QStringLiteral("volume_failed"));
        QCOMPARE(controller.volume(), 23);
        fake->writeFailure.clear();
        QVERIFY(controller.setSinkVolume(QStringLiteral("one"), 51));
        QCOMPARE(controller.volume(), 51);
        QVERIFY(controller.error().isEmpty());
    }
};
QTEST_GUILESS_MAIN(VolumeControllerTest)
#include "volumecontrollertest.moc"
