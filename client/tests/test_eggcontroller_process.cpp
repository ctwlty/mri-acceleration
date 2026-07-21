#include "app/EggControllerProcess.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class EggControllerProcessTest : public QObject {
    Q_OBJECT

private slots:
    void completeResultRequiresRawAndBothImages();
    void imagesMustBelongToReturnedTaskId();
};

void EggControllerProcessTest::completeResultRequiresRawAndBothImages()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    EggControllerLaunchConfig config;
    config.program = qEnvironmentVariable("FAKE_EGGCONTROLLER_PROXY_PATH");
    config.arguments = {
        QStringLiteral("--output"), temp.path(),
        QStringLiteral("--mode"), QStringLiteral("success")
    };
    config.workingDirectory = temp.path();

    EggControllerProcess process;
    QSignalSpy completed(&process, &EggControllerProcess::completed);
    QSignalSpy failed(&process, &EggControllerProcess::failed);

    QVERIFY(process.start(config));
    QVERIFY2(completed.wait(3000), "complete result was not emitted");
    QCOMPARE(failed.count(), 0);
    QCOMPARE(completed.count(), 1);

    const EggControllerArtifacts artifacts =
        qvariant_cast<EggControllerArtifacts>(completed.first().first());
    QCOMPARE(artifacts.taskId, QStringLiteral("test"));
    QVERIFY(QFileInfo(artifacts.rawPath).size() > 0);
    QVERIFY(QFileInfo(artifacts.kspaceImagePath).size() > 0);
    QVERIFY(QFileInfo(artifacts.finalImagePath).size() > 0);
}

void EggControllerProcessTest::imagesMustBelongToReturnedTaskId()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    EggControllerLaunchConfig config;
    config.program = qEnvironmentVariable("FAKE_EGGCONTROLLER_PROXY_PATH");
    config.arguments = {
        QStringLiteral("--output"), temp.path(),
        QStringLiteral("--mode"), QStringLiteral("mismatch")
    };
    config.workingDirectory = temp.path();

    EggControllerProcess process;
    QSignalSpy completed(&process, &EggControllerProcess::completed);
    QSignalSpy failed(&process, &EggControllerProcess::failed);

    QVERIFY(process.start(config));
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 3000);
    QCOMPARE(completed.count(), 0);
}

QTEST_MAIN(EggControllerProcessTest)
#include "test_eggcontroller_process.moc"
