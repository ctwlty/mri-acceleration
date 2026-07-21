#include "DeviceActionAvailability.h"

DeviceActionAvailability actionsForState(MriSdkSessionState state)
{
    switch (state) {
    case MriSdkSessionState::Unloaded:
    case MriSdkSessionState::Fault:
    case MriSdkSessionState::Closed:
        return {true, false, false, false};
    case MriSdkSessionState::Loaded:
        return {true, true, false, false};
    case MriSdkSessionState::Ready:
        return {true, false, true, false};
    case MriSdkSessionState::Scanning:
        return {false, false, false, true};
    case MriSdkSessionState::Initializing:
    case MriSdkSessionState::Stopping:
        return {false, false, false, false};
    }
    return {};
}
