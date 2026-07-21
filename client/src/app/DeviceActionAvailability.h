#pragma once

#include "MriSdkTypes.h"

struct DeviceActionAvailability {
    bool canLoadSdk = false;
    bool canConnect = false;
    bool canRun = false;
    bool canAbort = false;
};

DeviceActionAvailability actionsForState(MriSdkSessionState state);
