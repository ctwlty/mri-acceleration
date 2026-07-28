#include "app/MockWorkflow.h"

#include <QtTest>

namespace {
MockParameterDraft validDraft()
{
    MockParameterDraft draft;
    draft.scene = QStringLiteral("结构与形态成像");
    draft.object = QStringLiteral("标准水模");
    draft.sampleId = QStringLiteral("WATER-PHANTOM-001");
    draft.templateId = QStringLiteral("water-phantom-axial");
    draft.templateName = QStringLiteral("水模横断位成像模板");
    draft.protocolChain = {QStringLiteral("LOC"), QStringLiteral("FSE A")};
    draft.orientation = QStringLiteral("横断");
    draft.fovReadMm = 50.0;
    draft.fovPhaseMm = 50.0;
    draft.matrixRead = 128;
    draft.matrixPhase = 128;
    draft.trMs = 3000.0;
    draft.teMs = 12.9;
    draft.sliceThicknessMm = 3.5;
    draft.sliceGapMm = 1.0;
    draft.sliceCount = 11;
    draft.nex = 1;
    draft.coverageX = 0.16;
    draft.coverageY = 0.20;
    draft.coverageWidth = 0.66;
    draft.coverageHeight = 0.56;
    draft.coverageCenterX = 0.50;
    draft.coverageCenterY = 0.50;
    draft.slicePosition = 0.50;
    draft.outputRoot = QStringLiteral("C:/tmp/mock-runs");
    return draft;
}

MockPreparationEvidence writablePreparation()
{
    MockPreparationEvidence evidence;
    evidence.preparationConfirmed = true;
    evidence.protocolConfirmed = true;
    evidence.localizationConfirmed = true;
    evidence.outputRootWritable = true;
    return evidence;
}

MockStartConfirmations completeConfirmations()
{
    MockStartConfirmations confirmations;
    confirmations.mockSourceConfirmed = true;
    confirmations.outputConfirmed = true;
    confirmations.noDeviceSideEffectsConfirmed = true;
    return confirmations;
}

MockWorkflowDependencies deterministicDependencies(int& runCounter, int& snapshotCounter)
{
    MockWorkflowDependencies dependencies;
    dependencies.nextRunId = [&runCounter] {
        return QStringLiteral("RUN-MOCK-%1").arg(++runCounter, 3, 10, QLatin1Char('0'));
    };
    dependencies.nextSnapshotId = [&snapshotCounter] {
        return QStringLiteral("SNAP-MOCK-%1")
            .arg(++snapshotCounter, 3, 10, QLatin1Char('0'));
    };
    dependencies.nowUtc = [] {
        return QDateTime::fromString(
            QStringLiteral("2026-07-29T01:02:03.004Z"), Qt::ISODateWithMs);
    };
    return dependencies;
}

MockReconstructionArtifact reconstructionFor(const QByteArray& hash)
{
    MockReconstructionArtifact artifact;
    artifact.logicalSource = QStringLiteral(":/mock-reconstruction.png");
    artifact.pngSha256 = hash;
    artifact.byteSize = 4096;
    return artifact;
}

MockQcMetrics qcFor(const QByteArray& hash)
{
    MockQcMetrics qc;
    qc.snrDb = 31.25;
    qc.uniformityPercent = 82.5;
    qc.objectSizePixels = QSize(42, 58);
    qc.imageSha256 = hash;
    return qc;
}
}

class MockWorkflowTest : public QObject {
    Q_OBJECT

private slots:
    void givenHistoricalRaw_whenStartRequested_thenItIsBlockedWithoutIdentity();
    void givenLiveSource_whenStartRequested_thenAllLiveGatesAreReportedWithoutIdentity();
    void givenUpstreamEvidenceMissing_whenPrepared_thenRunIdentityIsNotCreated();
    void givenPreparedMock_whenStarted_thenIdentityIsUniqueAndSnapshotIsFrozen();
    void givenRunningMock_whenPausedResumedAndCancelled_thenEvidenceRemainsButResultsAreEmpty();
    void givenCompletedExecution_whenReconstructionAndQcSucceed_thenPackagingCanComplete();
    void givenAWorkflowFailure_whenResultsAlreadyExist_thenImageAndQcAreCleared();
    void givenMismatchedQcImage_whenQcIsRecorded_thenWorkflowFailsAndClearsResults();
};

void MockWorkflowTest::
    givenHistoricalRaw_whenStartRequested_thenItIsBlockedWithoutIdentity()
{
    MockWorkflow workflow;
    QVERIFY(workflow.selectDataSource(DataSourceKind::HistoricalRaw).ok);
    QVERIFY(workflow.prepare(validDraft(), writablePreparation()).ok);

    const MockActionResult result = workflow.start(completeConfirmations());

    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("历史 RAW")));
    QVERIFY(result.error.contains(QStringLiteral("来源绑定")));
    QVERIFY(workflow.runId().isEmpty());
    QVERIFY(workflow.snapshotId().isEmpty());
    QVERIFY(workflow.state() == MockWorkflowState::Prepared);
    QVERIFY(!workflow.hasReconstruction());
    QVERIFY(!workflow.hasQc());
}

void MockWorkflowTest::
    givenLiveSource_whenStartRequested_thenAllLiveGatesAreReportedWithoutIdentity()
{
    MockWorkflow workflow;
    QVERIFY(workflow.selectDataSource(DataSourceKind::LiveBlocked).ok);
    QVERIFY(workflow.prepare(validDraft(), writablePreparation()).ok);

    const MockActionResult result = workflow.start(completeConfirmations());

    QVERIFY(!result.ok);
    QCOMPARE(workflow.executionBlockReasons().size(), 7);
    QVERIFY(result.error.contains(QStringLiteral("SDK")));
    QVERIFY(result.error.contains(QStringLiteral("IDLE")));
    QVERIFY(result.error.contains(QStringLiteral("横断位")));
    QVERIFY(result.error.contains(QStringLiteral("唯一 Run owner")));
    QVERIFY(result.error.contains(QStringLiteral("人工授权")));
    QVERIFY(workflow.runId().isEmpty());
    QVERIFY(workflow.snapshotId().isEmpty());
    QVERIFY(workflow.state() == MockWorkflowState::Prepared);
}

void MockWorkflowTest::
    givenUpstreamEvidenceMissing_whenPrepared_thenRunIdentityIsNotCreated()
{
    MockWorkflow workflow;
    MockPreparationEvidence evidence = writablePreparation();
    evidence.localizationConfirmed = false;

    const MockActionResult result =
        workflow.prepare(validDraft(), evidence);

    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("定位")));
    QVERIFY(workflow.runId().isEmpty());
    QVERIFY(workflow.snapshotId().isEmpty());
    QVERIFY(workflow.state() == MockWorkflowState::Empty);
}

void MockWorkflowTest::
    givenPreparedMock_whenStarted_thenIdentityIsUniqueAndSnapshotIsFrozen()
{
    int runCounter = 0;
    int snapshotCounter = 0;
    const MockWorkflowDependencies dependencies =
        deterministicDependencies(runCounter, snapshotCounter);
    MockParameterDraft firstDraft = validDraft();
    MockWorkflow first(dependencies);
    QVERIFY(first.prepare(firstDraft, writablePreparation()).ok);
    firstDraft.trMs = 9999.0;

    QVERIFY(first.start(completeConfirmations()).ok);

    QCOMPARE(first.runId(), QStringLiteral("RUN-MOCK-001"));
    QCOMPARE(first.snapshotId(), QStringLiteral("SNAP-MOCK-001"));
    QCOMPARE(first.snapshot().parameters.trMs, 3000.0);
    QVERIFY(first.state() == MockWorkflowState::Running);
    const MockActionResult duplicateStart = first.start(completeConfirmations());
    QVERIFY(!duplicateStart.ok);
    QCOMPARE(first.runId(), QStringLiteral("RUN-MOCK-001"));
    QCOMPARE(first.snapshotId(), QStringLiteral("SNAP-MOCK-001"));

    MockWorkflow second(dependencies);
    QVERIFY(second.prepare(validDraft(), writablePreparation()).ok);
    QVERIFY(second.start(completeConfirmations()).ok);
    QCOMPARE(second.runId(), QStringLiteral("RUN-MOCK-002"));
    QCOMPARE(second.snapshotId(), QStringLiteral("SNAP-MOCK-002"));
    QVERIFY(first.runId() != second.runId());
    QVERIFY(first.snapshotId() != second.snapshotId());
}

void MockWorkflowTest::
    givenRunningMock_whenPausedResumedAndCancelled_thenEvidenceRemainsButResultsAreEmpty()
{
    int runCounter = 0;
    int snapshotCounter = 0;
    MockWorkflow workflow(deterministicDependencies(runCounter, snapshotCounter));
    QVERIFY(workflow.prepare(validDraft(), writablePreparation()).ok);
    QVERIFY(workflow.start(completeConfirmations()).ok);
    QVERIFY(workflow.setProgress(25).ok);
    QVERIFY(workflow.pause().ok);
    QVERIFY(workflow.state() == MockWorkflowState::Paused);

    const MockActionResult progressWhilePaused = workflow.setProgress(50);
    QVERIFY(!progressWhilePaused.ok);
    QCOMPARE(workflow.progressPercent(), 25);
    QVERIFY(workflow.resume().ok);
    QVERIFY(workflow.setProgress(50).ok);
    QVERIFY(workflow.cancel().ok);

    QVERIFY(workflow.state() == MockWorkflowState::Cancelled);
    QCOMPARE(workflow.runId(), QStringLiteral("RUN-MOCK-001"));
    QCOMPARE(workflow.snapshotId(), QStringLiteral("SNAP-MOCK-001"));
    QVERIFY(!workflow.auditEvents().isEmpty());
    QVERIFY(!workflow.hasReconstruction());
    QVERIFY(!workflow.hasQc());
    QVERIFY(workflow.packagePath().isEmpty());
}

void MockWorkflowTest::
    givenCompletedExecution_whenReconstructionAndQcSucceed_thenPackagingCanComplete()
{
    MockWorkflow workflow;
    QVERIFY(workflow.prepare(validDraft(), writablePreparation()).ok);
    QVERIFY(workflow.start(completeConfirmations()).ok);
    QVERIFY(workflow.setProgress(100).ok);
    QVERIFY(workflow.state() == MockWorkflowState::Processing);

    const QByteArray imageHash(64, 'A');
    QVERIFY(workflow.bindReconstruction(reconstructionFor(imageHash)).ok);
    QVERIFY(workflow.state() == MockWorkflowState::Reconstructed);
    QVERIFY(workflow.hasReconstruction());
    QVERIFY(!workflow.hasQc());
    QVERIFY(workflow.recordQcSuccess(qcFor(imageHash)).ok);
    QVERIFY(workflow.state() == MockWorkflowState::QcReady);
    QVERIFY(workflow.hasQc());
    QVERIFY(workflow.confirmResult().ok);
    QVERIFY(workflow.markPackaged(QStringLiteral("C:/tmp/mock-runs/RUN-MOCK-001")).ok);
    QVERIFY(workflow.state() == MockWorkflowState::Packaged);
    QVERIFY(!workflow.packagePath().isEmpty());
}

void MockWorkflowTest::
    givenAWorkflowFailure_whenResultsAlreadyExist_thenImageAndQcAreCleared()
{
    MockWorkflow workflow;
    QVERIFY(workflow.prepare(validDraft(), writablePreparation()).ok);
    QVERIFY(workflow.start(completeConfirmations()).ok);
    QVERIFY(workflow.setProgress(100).ok);
    const QByteArray imageHash(64, 'B');
    QVERIFY(workflow.bindReconstruction(reconstructionFor(imageHash)).ok);
    QVERIFY(workflow.recordQcSuccess(qcFor(imageHash)).ok);

    QVERIFY(workflow.fail(QStringLiteral("Mock QC downstream failure")).ok);

    QVERIFY(workflow.state() == MockWorkflowState::Failed);
    QVERIFY(!workflow.hasReconstruction());
    QVERIFY(!workflow.hasQc());
    QVERIFY(workflow.packagePath().isEmpty());
    QVERIFY(workflow.lastError().contains(QStringLiteral("failure")));
}

void MockWorkflowTest::
    givenMismatchedQcImage_whenQcIsRecorded_thenWorkflowFailsAndClearsResults()
{
    MockWorkflow workflow;
    QVERIFY(workflow.prepare(validDraft(), writablePreparation()).ok);
    QVERIFY(workflow.start(completeConfirmations()).ok);
    QVERIFY(workflow.setProgress(100).ok);
    QVERIFY(workflow.bindReconstruction(reconstructionFor(QByteArray(64, 'C'))).ok);

    const MockActionResult result =
        workflow.recordQcSuccess(qcFor(QByteArray(64, 'D')));

    QVERIFY(!result.ok);
    QVERIFY(workflow.state() == MockWorkflowState::Failed);
    QVERIFY(!workflow.hasReconstruction());
    QVERIFY(!workflow.hasQc());
    QVERIFY(workflow.lastError().contains(QStringLiteral("哈希")));
}

QTEST_APPLESS_MAIN(MockWorkflowTest)
#include "test_mock_workflow.moc"
