#include "app/DeviceActionAvailability.h"

#include <QtTest>

class DeviceActionAvailabilityTest : public QObject {
    Q_OBJECT

private slots:
    void mapsSessionStateToRealActions_data();
    void mapsSessionStateToRealActions();
    void readySessionDefaultsToRunHold();
    void onlyVerifiedBaselineWithFreshPrecheckCanRun();
};

void DeviceActionAvailabilityTest::mapsSessionStateToRealActions_data()
{
    QTest::addColumn<MriSdkSessionState>("state");
    QTest::addColumn<bool>("canLoadSdk");
    QTest::addColumn<bool>("canConnect");
    QTest::addColumn<bool>("canRun");
    QTest::addColumn<bool>("canAbort");

    QTest::newRow("unloaded") << MriSdkSessionState::Unloaded << true << false << false << false;
    QTest::newRow("loaded") << MriSdkSessionState::Loaded << true << true << false << false;
    QTest::newRow("initializing") << MriSdkSessionState::Initializing << false << false << false << false;
    QTest::newRow("ready") << MriSdkSessionState::Ready << true << false << false << false;
    QTest::newRow("scanning") << MriSdkSessionState::Scanning << false << false << false << true;
    QTest::newRow("stopping") << MriSdkSessionState::Stopping << false << false << false << false;
    QTest::newRow("fault") << MriSdkSessionState::Fault << true << false << false << false;
    QTest::newRow("closed") << MriSdkSessionState::Closed << true << false << false << false;
}

void DeviceActionAvailabilityTest::mapsSessionStateToRealActions()
{
    QFETCH(MriSdkSessionState, state);
    QFETCH(bool, canLoadSdk);
    QFETCH(bool, canConnect);
    QFETCH(bool, canRun);
    QFETCH(bool, canAbort);

    const DeviceActionAvailability actions = actionsForState(state);

    QCOMPARE(actions.canLoadSdk, canLoadSdk);
    QCOMPARE(actions.canConnect, canConnect);
    QCOMPARE(actions.canRun, canRun);
    QCOMPARE(actions.canAbort, canAbort);
}

void DeviceActionAvailabilityTest::readySessionDefaultsToRunHold()
{
    const DeviceActionAvailability actions = actionsForState(MriSdkSessionState::Ready);

    QVERIFY(!actions.canRun);
}

void DeviceActionAvailabilityTest::onlyVerifiedBaselineWithFreshPrecheckCanRun()
{
    QVERIFY(!actionsForState(MriSdkSessionState::Ready, ExecutionGate::Hold, true).canRun);
    QVERIFY(!actionsForState(MriSdkSessionState::Ready, ExecutionGate::VerifiedBaseline, false).canRun);
    QVERIFY(!actionsForState(MriSdkSessionState::Ready, ExecutionGate::VerifiedScene, true).canRun);
    QVERIFY(actionsForState(MriSdkSessionState::Ready, ExecutionGate::VerifiedBaseline, true).canRun);
}

QTEST_MAIN(DeviceActionAvailabilityTest)
#include "test_device_actions.moc"
