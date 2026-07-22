#include "app/ImageQualityEvaluator.h"

#include <QImage>
#include <QtTest>

#include <cmath>
#include <vector>

namespace {
QImage createKnownPhantom()
{
    QImage image(64, 64, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const bool signal = x >= 22 && x <= 42 && y >= 16 && y <= 48;
            const int value = signal ? 100 : (((x + y) % 2 == 0) ? 8 : 12);
            image.setPixel(x, y, qRgb(value, value, value));
        }
    }
    return image;
}

double borderNoise(const QImage& image)
{
    const int borderWidth = qMax(2, qMin(image.width(), image.height()) / 10);
    std::vector<double> values;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (x < borderWidth || x >= image.width() - borderWidth ||
                y < borderWidth || y >= image.height() - borderWidth) {
                values.push_back(qGray(image.pixel(x, y)));
            }
        }
    }
    double average = 0.0;
    for (const double value : values) {
        average += value;
    }
    average /= values.size();
    double sum = 0.0;
    for (const double value : values) {
        const double delta = value - average;
        sum += delta * delta;
    }
    return std::sqrt(sum / (values.size() - 1));
}
}

class ImageQualityEvaluatorTest : public QObject {
    Q_OBJECT

private slots:
    void imageLevelSnrUsesSignalMeanOverBackgroundNoise();
    void evaluatesA256PixelImageUsingTheLargestSignalObject();
    void rejectsAFlatImageWithoutMeasurableNoise();
};

void ImageQualityEvaluatorTest::imageLevelSnrUsesSignalMeanOverBackgroundNoise()
{
    const QImage image = createKnownPhantom();
    const ImageQualityResult result = ImageQualityEvaluator::evaluate(image);

    QVERIFY2(result.ok, qPrintable(result.error));
    const double expectedDb = 20.0 * std::log10(100.0 / borderNoise(image));
    QVERIFY2(qAbs(result.snrDb - expectedDb) < 0.01,
             qPrintable(QStringLiteral("expected %1 dB, got %2 dB")
                            .arg(expectedDb, 0, 'f', 4)
                            .arg(result.snrDb, 0, 'f', 4)));
}

void ImageQualityEvaluatorTest::evaluatesA256PixelImageUsingTheLargestSignalObject()
{
    QImage image(256, 256, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const bool largeObject = x >= 100 && x <= 150 && y >= 80 && y <= 180;
            const bool distractor = x >= 50 && x <= 54 && y >= 50 && y <= 54;
            const int value = largeObject ? 100
                                          : (distractor ? 220 : (((x + y) % 2 == 0) ? 8 : 12));
            image.setPixel(x, y, qRgb(value, value, value));
        }
    }

    const ImageQualityResult result = ImageQualityEvaluator::evaluate(image);

    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.objectSizePixels, QSize(51, 101));
    QCOMPARE(result.uniformityPercent, 100.0);
}

void ImageQualityEvaluatorTest::rejectsAFlatImageWithoutMeasurableNoise()
{
    QImage image(256, 256, QImage::Format_RGB32);
    image.fill(qRgb(42, 42, 42));

    const ImageQualityResult result = ImageQualityEvaluator::evaluate(image);

    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

QTEST_MAIN(ImageQualityEvaluatorTest)
#include "test_image_quality_evaluator.moc"
