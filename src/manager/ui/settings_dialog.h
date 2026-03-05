#pragma once

#include "manager/manager_service.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Modal settings dialog for configuring storage paths.
// Returns true if the user changed any settings.
bool ShowSettingsDialog(HWND parent, ManagerService& mgr);
