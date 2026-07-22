#pragma once

#include <QImage>
#include <QSize>
#include <QString>

struct ImageQualityResult {
    bool ok = false;
    QString error;
    double snrDb = 0.0;
    double uniformityPercent = 0.0;
    QSize objectSizePixels;
};

class ImageQualityEvaluator {
public:
    static ImageQualityResult evaluate(const QImage& source);
};
