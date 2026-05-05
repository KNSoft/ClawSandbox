#include "ServiceControl.h"

#include "../UserDll/UserDll.h"
#include <strsafe.h>

static void ClearErrorMessage(WCHAR* errorMessage, size_t errorMessageCount)
{
    if (errorMessage != NULL && errorMessageCount != 0)
    {
        errorMessage[0] = L'\0';
    }
}

static void SetErrorMessage(WCHAR* errorMessage, size_t errorMessageCount, const WCHAR* message)
{
    if (errorMessage == NULL || errorMessageCount == 0)
    {
        return;
    }

    StringCchCopyW(errorMessage, errorMessageCount, message != NULL ? message : L"Unknown error.");
}

static ServiceControlState ConvertServiceState(
    _In_ CLAWSANDBOX_SERVICE_STATE state)
{
    ServiceControlState result;

    result.queryOk = state.queryOk;
    result.installed = state.installed;
    result.running = state.running;
    result.startPending = state.startPending;
    result.stopPending = state.stopPending;
    result.deletePending = state.deletePending;
    return result;
}

DWORD ServiceControlInitialize(WCHAR* errorMessage, size_t errorMessageCount)
{
    return ClawSandboxInitialize(errorMessage, errorMessageCount);
}

DWORD ServiceControlInstall(WCHAR* errorMessage, size_t errorMessageCount)
{
    return ClawSandboxInstallService(errorMessage, errorMessageCount);
}

ServiceControlState ServiceControlQueryState(void)
{
    return ConvertServiceState(ClawSandboxQueryServiceState());
}

DWORD ServiceControlEnsureInstalledOnStartup(BOOL* installedByLauncher, WCHAR* errorMessage, size_t errorMessageCount)
{
    ServiceControlState state = ServiceControlQueryState();
    BOOL wasInstalled;
    DWORD result;

    ClearErrorMessage(errorMessage, errorMessageCount);
    if (installedByLauncher != NULL)
    {
        *installedByLauncher = FALSE;
    }

    if (!state.queryOk)
    {
        if (GetLastError() == ERROR_SUCCESS)
        {
            SetLastError(ERROR_GEN_FAILURE);
        }
        SetErrorMessage(errorMessage, errorMessageCount, L"Unable to query the service state.");
        return GetLastError();
    }

    wasInstalled = state.installed;
    result = ServiceControlInstall(errorMessage, errorMessageCount);
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    if (!wasInstalled && installedByLauncher != NULL)
    {
        *installedByLauncher = TRUE;
    }
    return ERROR_SUCCESS;
}

DWORD ServiceControlStart(WCHAR* errorMessage, size_t errorMessageCount)
{
    return ClawSandboxStartService(errorMessage, errorMessageCount);
}

DWORD ServiceControlStop(WCHAR* errorMessage, size_t errorMessageCount)
{
    return ClawSandboxStopService(errorMessage, errorMessageCount);
}

DWORD ServiceControlUninstall(WCHAR* errorMessage, size_t errorMessageCount)
{
    return ClawSandboxUninstallService(errorMessage, errorMessageCount);
}

DWORD ServiceControlSetSelfProtection(BOOL enabled, WCHAR* errorMessage, size_t errorMessageCount)
{
    return ClawSandboxSetSelfProtection(enabled, errorMessage, errorMessageCount);
}

DWORD ServiceControlSetFsWhiteList(const WCHAR* fsWhiteListMultiSz, WCHAR* errorMessage, size_t errorMessageCount)
{
    return ClawSandboxSetFsWhiteList(fsWhiteListMultiSz, errorMessage, errorMessageCount);
}

DWORD ServiceControlSetClawType(const WCHAR* clawType, WCHAR* errorMessage, size_t errorMessageCount)
{
    return ClawSandboxSetClawType(clawType, errorMessage, errorMessageCount);
}

DWORD ServiceControlCleanupOnExit(BOOL installedByLauncher, WCHAR* errorMessage, size_t errorMessageCount)
{
    DWORD result;

    if (!installedByLauncher)
    {
        return ERROR_SUCCESS;
    }

    result = ServiceControlStop(errorMessage, errorMessageCount);
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    return ClawSandboxUninstallService(errorMessage, errorMessageCount);
}
