#pragma once

#include <stddef.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SERVICE_CONTROL_ERROR_CAPACITY 1024

typedef struct ServiceControlState
{
    BOOL queryOk;
    BOOL installed;
    BOOL running;
    BOOL startPending;
    BOOL stopPending;
    BOOL deletePending;
} ServiceControlState;

DWORD ServiceControlInitialize(wchar_t* errorMessage, size_t errorMessageCount);
DWORD ServiceControlInstall(wchar_t* errorMessage, size_t errorMessageCount);
ServiceControlState ServiceControlQueryState(void);
DWORD ServiceControlEnsureInstalledOnStartup(BOOL* installedByLauncher, wchar_t* errorMessage, size_t errorMessageCount);
DWORD ServiceControlStart(wchar_t* errorMessage, size_t errorMessageCount);
DWORD ServiceControlStop(wchar_t* errorMessage, size_t errorMessageCount);
DWORD ServiceControlUninstall(wchar_t* errorMessage, size_t errorMessageCount);
DWORD ServiceControlSetSelfProtection(BOOL enabled, wchar_t* errorMessage, size_t errorMessageCount);
DWORD ServiceControlSetFsWhiteList(const wchar_t* fsWhiteListMultiSz, wchar_t* errorMessage, size_t errorMessageCount);
DWORD ServiceControlSetClawType(const wchar_t* clawType, wchar_t* errorMessage, size_t errorMessageCount);
DWORD ServiceControlCleanupOnExit(BOOL installedByLauncher, wchar_t* errorMessage, size_t errorMessageCount);

#ifdef __cplusplus
}
#endif
