#pragma once

#include "MriSdkTypes.h"

#include <QString>

struct SceneTemplate {
    QString name;
    QString target;
    QString sequence;
    QString stepA;
    QString stepB;
    QString processing;
    QString analysis;
    QString snr;
    QString uniformity;
    QString peak;
    QString area;
    QString note;
    QString primaryScene;
    QString outputType;
    QString acquisitionProtocol;
    QString preparation;
    QString positioning;
    QString reconstruction;
    QString qcOutput;
    QString presetVersion;
    QString adaptationStatus;
    QString runGate;
    ExecutionGate executionGate = ExecutionGate::Hold;
    QString parameterStatus;
    QString parameterDetails;
    QString sdkMappingStatus;
    QString physicsCheckStatus;
    QString riskTags;
    QString handoffTarget;
};
