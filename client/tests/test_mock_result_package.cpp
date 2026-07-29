#include "app/MockResultPackage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QByteArray testPngBytes()
{
    return QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8"
        "/x8AAusB9Y9Z6u0AAAAASUVORK5CYII=");
}

QByteArray sha256(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
        .toHex()
        .toUpper();
}

MockPackageInput validInput(const QString& rootDirectory,
                            const QString& runId = QStringLiteral("RUN-MOCK-001"),
                            const QString& snapshotId = QStringLiteral("SNAP-MOCK-001"))
{
    MockPackageInput input;
    input.rootDirectory = rootDirectory;
    input.softwareCommit = QStringLiteral("0123456789abcdef0123456789abcdef01234567");
    input.runId = runId;
    input.snapshot.snapshotId = snapshotId;
    input.snapshot.dataSource = DataSourceKind::Mock;
    input.snapshot.parameters.scene = QStringLiteral("结构与形态成像");
    input.snapshot.parameters.object = QStringLiteral("标准水模");
    input.snapshot.parameters.sampleId = QStringLiteral("WATER-PHANTOM-001");
    input.snapshot.parameters.templateId = QStringLiteral("water-phantom-axial");
    input.snapshot.parameters.templateName = QStringLiteral("水模横断位成像模板");
    input.snapshot.parameters.protocolChain =
        {QStringLiteral("LOC"), QStringLiteral("FSE A")};
    input.snapshot.parameters.orientation = QStringLiteral("横断");
    input.snapshot.parameters.imagingTarget = QStringLiteral("均衡");
    input.snapshot.parameters.fovReadMm = 50.0;
    input.snapshot.parameters.fovPhaseMm = 50.0;
    input.snapshot.parameters.matrixRead = 128;
    input.snapshot.parameters.matrixPhase = 128;
    input.snapshot.parameters.trMs = 3000.0;
    input.snapshot.parameters.teMs = 12.9;
    input.snapshot.parameters.sliceThicknessMm = 3.5;
    input.snapshot.parameters.sliceGapMm = 1.0;
    input.snapshot.parameters.sliceCount = 11;
    input.snapshot.parameters.nex = 1;
    input.snapshot.parameters.coverageX = 0.16;
    input.snapshot.parameters.coverageY = 0.20;
    input.snapshot.parameters.coverageWidth = 0.66;
    input.snapshot.parameters.coverageHeight = 0.56;
    input.snapshot.parameters.coverageCenterX = 0.50;
    input.snapshot.parameters.coverageCenterY = 0.50;
    input.snapshot.parameters.slicePosition = 0.50;
    input.snapshot.parameters.outputRoot = rootDirectory;
    input.standardResultPng = testPngBytes();
    input.reconstruction.logicalSource = QStringLiteral(":/mock-reconstruction.png");
    input.reconstruction.pngSha256 = sha256(input.standardResultPng);
    input.reconstruction.byteSize = input.standardResultPng.size();
    input.qc.snrDb = 30.5;
    input.qc.uniformityPercent = 81.25;
    input.qc.objectSizePixels = QSize(40, 55);
    input.qc.imageSha256 = input.reconstruction.pngSha256;
    MockAuditEvent event;
    event.name = QStringLiteral("MOCK_RUN_STARTED");
    event.occurredAtUtc = QDateTime::fromString(
        QStringLiteral("2026-07-29T01:02:03.004Z"), Qt::ISODateWithMs);
    event.runId = runId;
    event.state = MockWorkflowState::Running;
    event.dataSource = DataSourceKind::Mock;
    input.auditEvents.append(event);
    input.taskNote =
        QStringLiteral("MOCK 结果包；不来自设备、SDK、RAW 或 eggcontrollerV2。");
    input.createdAtUtc = QDateTime::fromString(
        QStringLiteral("2026-07-29T01:02:04.005Z"), Qt::ISODateWithMs);
    return input;
}

QMap<QString, QByteArray> directoryHashes(const QString& directory)
{
    QMap<QString, QByteArray> result;
    const QDir dir(directory);
    const QFileInfoList files =
        dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& info : files) {
        QFile file(info.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            result.insert(info.fileName(), sha256(file.readAll()));
        }
    }
    return result;
}
}

class MockResultPackageTest : public QObject {
    Q_OBJECT

private slots:
    void givenQcReadyMock_whenPackageIsWritten_thenSevenFilesAndVerifiedHashesExist();
    void givenAnExistingRunDirectory_whenWrittenAgain_thenNothingIsOverwritten();
    void givenMismatchedImageBinding_whenWriteIsRequested_thenNoPackageIsCreated();
    void givenAFileInsteadOfOutputRoot_whenWriteIsRequested_thenNoPartialPackageRemains();
    void givenAnEmptyRoot_whenHistoryIsLoaded_thenNoSampleRowsAreInvented();
    void givenActualManifests_whenHistoryIsLoaded_thenValidWarningAndErrorAreReported();
    void givenManifestIdentityTampered_whenVerified_thenPackageIsRejected();
};

void MockResultPackageTest::
    givenQcReadyMock_whenPackageIsWritten_thenSevenFilesAndVerifiedHashesExist()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const MockPackageInput input = validInput(root.path());

    const PackageWriteResult written = MockResultPackage::write(input);

    QVERIFY2(written.ok, qPrintable(written.error));
    QCOMPARE(QFileInfo(written.packageDirectory).fileName(), input.runId);
    const QStringList expectedFiles = {
        QStringLiteral("audit-events.json"),
        QStringLiteral("manifest.json"),
        QStringLiteral("mock-qc.json"),
        QStringLiteral("mock-source.json"),
        QStringLiteral("parameter-snapshot.json"),
        QStringLiteral("standard-mock-result.png"),
        QStringLiteral("task-note.txt")
    };
    QCOMPARE(QDir(written.packageDirectory)
                 .entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name),
             expectedFiles);

    const PackageVerification verification =
        MockResultPackage::verify(written.packageDirectory);
    QVERIFY2(verification.integrity == PackageIntegrity::Valid,
             qPrintable(verification.issues.join(QLatin1Char('\n'))));
    QCOMPARE(verification.manifest.value(QStringLiteral("dataSource")).toString(),
             QStringLiteral("MOCK"));
    QCOMPARE(verification.manifest.value(QStringLiteral("runId")).toString(),
             input.runId);
    QCOMPARE(verification.manifest.value(QStringLiteral("snapshotId")).toString(),
             input.snapshot.snapshotId);
    QCOMPARE(verification.manifest.value(QStringLiteral("artifacts"))
                 .toArray()
                 .size(),
             6);
    QCOMPARE(written.artifacts.size(), 6);
    for (const PackageArtifactInfo& artifact : written.artifacts) {
        QFile file(QDir(written.packageDirectory).filePath(artifact.relativePath));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QCOMPARE(artifact.byteSize, static_cast<qint64>(bytes.size()));
        QCOMPARE(artifact.sha256, sha256(bytes));
    }
    QFile auditFile(
        QDir(written.packageDirectory).filePath(QStringLiteral("audit-events.json")));
    QVERIFY(auditFile.open(QIODevice::ReadOnly));
    const QJsonArray audit =
        QJsonDocument::fromJson(auditFile.readAll()).array();
    QVERIFY(!audit.isEmpty());
    const QJsonObject lastAudit = audit.at(audit.size() - 1).toObject();
    QCOMPARE(lastAudit.value(QStringLiteral("event")).toString(),
             QStringLiteral("PACKAGE_SAVED"));
    QCOMPARE(lastAudit.value(QStringLiteral("state")).toString(),
             QStringLiteral("Packaged"));
    QCOMPARE(lastAudit.value(QStringLiteral("dataSource")).toString(),
             QStringLiteral("MOCK"));
}

void MockResultPackageTest::
    givenAnExistingRunDirectory_whenWrittenAgain_thenNothingIsOverwritten()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const MockPackageInput input = validInput(root.path());
    const PackageWriteResult first = MockResultPackage::write(input);
    QVERIFY2(first.ok, qPrintable(first.error));
    const QMap<QString, QByteArray> before = directoryHashes(first.packageDirectory);

    const PackageWriteResult second = MockResultPackage::write(input);

    QVERIFY(!second.ok);
    QVERIFY(second.error.contains(QStringLiteral("已存在")));
    QCOMPARE(directoryHashes(first.packageDirectory), before);
    const QStringList staging =
        QDir(root.path()).entryList({QStringLiteral("*.staging-*")}, QDir::Dirs);
    QVERIFY(staging.isEmpty());
}

void MockResultPackageTest::
    givenMismatchedImageBinding_whenWriteIsRequested_thenNoPackageIsCreated()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    MockPackageInput input = validInput(root.path());
    input.qc.imageSha256 = QByteArray(64, 'F');

    const PackageWriteResult result = MockResultPackage::write(input);

    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("哈希")));
    QVERIFY(!QFileInfo::exists(QDir(root.path()).filePath(input.runId)));
}

void MockResultPackageTest::
    givenAFileInsteadOfOutputRoot_whenWriteIsRequested_thenNoPartialPackageRemains()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString rootFile = QDir(temporary.path()).filePath(QStringLiteral("not-a-directory"));
    QFile file(rootFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("occupied");
    file.close();
    MockPackageInput input = validInput(rootFile);
    input.snapshot.parameters.outputRoot = rootFile;

    const PackageWriteResult result = MockResultPackage::write(input);

    QVERIFY(!result.ok);
    QVERIFY(QFileInfo(rootFile).isFile());
    QCOMPARE(QFileInfo(rootFile).size(), 8);
}

void MockResultPackageTest::
    givenAnEmptyRoot_whenHistoryIsLoaded_thenNoSampleRowsAreInvented()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const HistoryLoadResult history = MockResultPackage::loadHistory(root.path());

    QVERIFY2(history.ok, qPrintable(history.error));
    QVERIFY(history.records.isEmpty());
}

void MockResultPackageTest::
    givenActualManifests_whenHistoryIsLoaded_thenValidWarningAndErrorAreReported()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const PackageWriteResult written =
        MockResultPackage::write(validInput(root.path()));
    QVERIFY2(written.ok, qPrintable(written.error));

    QDir rootDir(root.path());
    QVERIFY(rootDir.mkdir(QStringLiteral("BROKEN-MANIFEST")));
    QFile broken(rootDir.filePath(QStringLiteral("BROKEN-MANIFEST/manifest.json")));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("{not-json");
    broken.close();

    HistoryLoadResult history = MockResultPackage::loadHistory(root.path());
    QVERIFY2(history.ok, qPrintable(history.error));
    QCOMPARE(history.records.size(), 2);
    bool foundValid = false;
    bool foundError = false;
    for (const MockHistoryRecord& record : history.records) {
        foundValid |= record.integrity == PackageIntegrity::Valid;
        foundError |= record.integrity == PackageIntegrity::Error;
    }
    QVERIFY(foundValid);
    QVERIFY(foundError);

    QFile resultImage(
        QDir(written.packageDirectory)
            .filePath(QStringLiteral("standard-mock-result.png")));
    QVERIFY(resultImage.open(QIODevice::Append));
    resultImage.write("changed");
    resultImage.close();

    history = MockResultPackage::loadHistory(root.path());
    bool foundIntegrityFailure = false;
    for (const MockHistoryRecord& record : history.records) {
        if (record.runId == QStringLiteral("RUN-MOCK-001")) {
            foundIntegrityFailure =
                record.integrity == PackageIntegrity::Warning
                || record.integrity == PackageIntegrity::Error;
            QVERIFY(record.previewImagePath.isEmpty());
        }
    }
    QVERIFY(foundIntegrityFailure);
}

void MockResultPackageTest::
    givenManifestIdentityTampered_whenVerified_thenPackageIsRejected()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const PackageWriteResult written =
        MockResultPackage::write(validInput(root.path()));
    QVERIFY2(written.ok, qPrintable(written.error));
    const QString manifestPath =
        QDir(written.packageDirectory).filePath(QStringLiteral("manifest.json"));
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QJsonObject original =
        QJsonDocument::fromJson(manifestFile.readAll()).object();
    manifestFile.close();

    const QList<QPair<QString, QString>> tamperedValues = {
        {QStringLiteral("runId"), QStringLiteral("RUN-MOCK-TAMPERED")},
        {QStringLiteral("snapshotId"), QStringLiteral("SNAP-MOCK-TAMPERED")},
        {QStringLiteral("sampleId"), QStringLiteral("SAMPLE-TAMPERED")},
        {QStringLiteral("templateId"), QStringLiteral("TEMPLATE-TAMPERED")},
        {QStringLiteral("softwareCommit"), QStringLiteral("TAMPERED-COMMIT")}
    };
    for (const auto& tampered : tamperedValues) {
        QJsonObject changed = original;
        changed.insert(tampered.first, tampered.second);
        QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        manifestFile.write(
            QJsonDocument(changed).toJson(QJsonDocument::Indented));
        manifestFile.close();

        const PackageVerification verification =
            MockResultPackage::verify(written.packageDirectory);
        QVERIFY2(
            verification.integrity == PackageIntegrity::Error,
            qPrintable(
                QStringLiteral("%1 篡改未被拒绝").arg(tampered.first)));
        QVERIFY(!verification.issues.isEmpty());
    }
}

QTEST_APPLESS_MAIN(MockResultPackageTest)
#include "test_mock_result_package.moc"
