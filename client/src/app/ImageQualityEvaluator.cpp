#include "ImageQualityEvaluator.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <vector>

namespace {
double percentile(std::vector<double> values, double percent)
{
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double position = (values.size() - 1) * percent / 100.0;
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    if (lower == upper) {
        return values[lower];
    }
    const double fraction = position - lower;
    return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

double mean(const std::vector<double>& values)
{
    return values.empty()
        ? 0.0
        : std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double sampleDeviation(const std::vector<double>& values, double average)
{
    if (values.size() < 2) {
        return 0.0;
    }
    double sum = 0.0;
    for (const double value : values) {
        const double delta = value - average;
        sum += delta * delta;
    }
    return std::sqrt(sum / (values.size() - 1));
}
}

ImageQualityResult ImageQualityEvaluator::evaluate(const QImage& source)
{
    ImageQualityResult result;
    if (source.isNull() || source.width() < 16 || source.height() < 16) {
        result.error = QStringLiteral("图像为空或尺寸过小");
        return result;
    }

    const QImage image = source.convertToFormat(QImage::Format_Grayscale8);
    const int width = image.width();
    const int height = image.height();
    const int pixelCount = width * height;
    std::vector<double> pixels(static_cast<std::size_t>(pixelCount));
    for (int y = 0; y < height; ++y) {
        const uchar* line = image.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            pixels[static_cast<std::size_t>(y * width + x)] = line[x];
        }
    }

    const int borderWidth = qMax(2, qMin(width, height) / 10);
    std::vector<double> background;
    background.reserve(static_cast<std::size_t>(pixelCount / 3));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (x < borderWidth || x >= width - borderWidth ||
                y < borderWidth || y >= height - borderWidth) {
                background.push_back(pixels[static_cast<std::size_t>(y * width + x)]);
            }
        }
    }

    const double backgroundMedian = percentile(background, 50.0);
    std::vector<double> deviations;
    deviations.reserve(background.size());
    for (const double value : background) {
        deviations.push_back(std::abs(value - backgroundMedian));
    }
    const double robustNoise = 1.4826 * percentile(deviations, 50.0);
    const double highSignal = percentile(pixels, 99.0);
    const double threshold = backgroundMedian +
        std::max({5.0, 3.0 * robustNoise, 0.15 * (highSignal - backgroundMedian)});

    std::vector<unsigned char> foreground(static_cast<std::size_t>(pixelCount), 0);
    for (int i = 0; i < pixelCount; ++i) {
        foreground[static_cast<std::size_t>(i)] = pixels[static_cast<std::size_t>(i)] > threshold;
    }

    std::vector<unsigned char> visited(static_cast<std::size_t>(pixelCount), 0);
    std::vector<int> largestComponent;
    const int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int start = 0; start < pixelCount; ++start) {
        if (!foreground[static_cast<std::size_t>(start)] || visited[static_cast<std::size_t>(start)]) {
            continue;
        }
        std::queue<int> pending;
        std::vector<int> component;
        pending.push(start);
        visited[static_cast<std::size_t>(start)] = 1;
        while (!pending.empty()) {
            const int current = pending.front();
            pending.pop();
            component.push_back(current);
            const int x = current % width;
            const int y = current / width;
            for (const auto& offset : offsets) {
                const int nextX = x + offset[0];
                const int nextY = y + offset[1];
                if (nextX < 0 || nextX >= width || nextY < 0 || nextY >= height) {
                    continue;
                }
                const int next = nextY * width + nextX;
                if (foreground[static_cast<std::size_t>(next)] &&
                    !visited[static_cast<std::size_t>(next)]) {
                    visited[static_cast<std::size_t>(next)] = 1;
                    pending.push(next);
                }
            }
        }
        if (component.size() > largestComponent.size()) {
            largestComponent = std::move(component);
        }
    }

    const std::size_t minimumObjectPixels =
        static_cast<std::size_t>(qMax(16, pixelCount / 1000));
    if (largestComponent.size() < minimumObjectPixels) {
        result.error = QStringLiteral("未找到可用的成像对象");
        return result;
    }

    std::vector<double> signal;
    signal.reserve(largestComponent.size());
    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;
    for (const int index : largestComponent) {
        const int x = index % width;
        const int y = index / width;
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x);
        maxY = qMax(maxY, y);
        signal.push_back(pixels[static_cast<std::size_t>(index)]);
    }

    const double backgroundAverage = mean(background);
    const double noise = sampleDeviation(background, backgroundAverage);
    const double signalAverage = mean(signal);
    if (noise <= std::numeric_limits<double>::epsilon() || signalAverage <= backgroundAverage) {
        result.error = QStringLiteral("背景噪声或信号不足，无法评估");
        return result;
    }

    const double lowSignal = percentile(signal, 5.0);
    const double upperSignal = percentile(signal, 95.0);
    const double snrRatio = signalAverage / noise;
    result.snrDb = 20.0 * std::log10(snrRatio);
    result.uniformityPercent = upperSignal + lowSignal <= 0.0
        ? 0.0
        : 100.0 * (1.0 - (upperSignal - lowSignal) / (upperSignal + lowSignal));
    result.uniformityPercent = std::clamp(result.uniformityPercent, 0.0, 100.0);
    result.objectSizePixels = QSize(maxX - minX + 1, maxY - minY + 1);
    result.ok = std::isfinite(result.snrDb) && std::isfinite(result.uniformityPercent);
    if (!result.ok) {
        result.error = QStringLiteral("质控计算返回了非有限值");
    }
    return result;
}
