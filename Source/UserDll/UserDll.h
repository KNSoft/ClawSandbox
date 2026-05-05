#pragma once

#include <Windows.h>
#include <stddef.h>
#include "../Common/ClawSandboxProtocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLAWSANDBOX_ERROR_CAPACITY 1024
#define CLAWSANDBOX_PROCESS_PATH_CAPACITY 1024
#define CLAWSANDBOX_OPTION_SELF_PROTECTION 0x00000001
#define CLAWSANDBOX_OPTION_FS_WHITE_LIST 0x00000002
#define CLAWSANDBOX_OPTION_CLAW_TYPE 0x00000004

#ifdef CLAWSANDBOX_USERDLL_EXPORTS
#define CLAWSANDBOX_API __declspec(dllexport)
#else
#define CLAWSANDBOX_API __declspec(dllimport)
#endif

typedef struct CLAWSANDBOX_SERVICE_STATE
{
    BOOL queryOk;
    BOOL installed;
    BOOL running;
    BOOL startPending;
    BOOL stopPending;
    BOOL deletePending;
} CLAWSANDBOX_SERVICE_STATE;

typedef struct CLAWSANDBOX_OPTIONS
{
    DWORD flags;
    BOOL selfProtection;
    PCWSTR fsWhiteListMultiSz;
    PCWSTR clawType;
} CLAWSANDBOX_OPTIONS;

typedef struct CLAWSANDBOX_TRACKED_PROCESS
{
    DWORD processId;
    BOOL imagePathAvailable;
    WCHAR imagePath[CLAWSANDBOX_PROCESS_PATH_CAPACITY];
} CLAWSANDBOX_TRACKED_PROCESS;

CLAWSANDBOX_API DWORD ClawSandboxInitialize(wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxInstallService(wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxSetOptions(const CLAWSANDBOX_OPTIONS* options, wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxSetSelfProtection(BOOL enabled, wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxSetFsWhiteList(PCWSTR fsWhiteListMultiSz, wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxSetClawType(PCWSTR clawType, wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxStartService(wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxStopService(wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxUninstallService(wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API CLAWSANDBOX_SERVICE_STATE ClawSandboxQueryServiceState(void);
CLAWSANDBOX_API DWORD ClawSandboxQueryTrackedProcessIds(DWORD* processIds, DWORD capacity, DWORD* count, wchar_t* errorMessage, size_t errorMessageCount);
CLAWSANDBOX_API DWORD ClawSandboxQueryTrackedProcesses(CLAWSANDBOX_TRACKED_PROCESS* processes, DWORD capacity, DWORD* count, wchar_t* errorMessage, size_t errorMessageCount);

#ifdef __cplusplus
}
#endif
