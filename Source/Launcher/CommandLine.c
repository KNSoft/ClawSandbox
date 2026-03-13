#include <Windows.h>
#include <shellapi.h>

#include "AppShared.h"
#include "ServiceControl.h"

typedef DWORD (*ServiceCommandFn)(WCHAR* errorMessage, size_t errorMessageCount);

static int RunStatusCommand(void)
{
    ServiceControlState state = ServiceControlQueryState();
    DWORD error = GetLastError();

    if (!state.queryOk)
    {
        if (error == ERROR_SUCCESS)
        {
            error = ERROR_GEN_FAILURE;
        }

        return (int)HRESULT_FROM_WIN32(error);
    }

    return (state.installed ? 1 : 0) | (state.running ? 2 : 0);
}

static int RunServiceCommand(ServiceCommandFn command)
{
    WCHAR errorMessage[SERVICE_CONTROL_ERROR_CAPACITY] = { 0 };
    DWORD error = ServiceControlInitialize(errorMessage, ARRAYSIZE(errorMessage));

    if (error != ERROR_SUCCESS)
    {
        return (int)error;
    }

    return (int)command(errorMessage, ARRAYSIZE(errorMessage));
}

static int RunCommand(const WCHAR* command)
{
    if (_wcsicmp(command, L"install") == 0)
    {
        return RunServiceCommand(ServiceControlInstall);
    }

    if (_wcsicmp(command, L"start") == 0)
    {
        return RunServiceCommand(ServiceControlStart);
    }

    if (_wcsicmp(command, L"stop") == 0)
    {
        return RunServiceCommand(ServiceControlStop);
    }

    if (_wcsicmp(command, L"uninstall") == 0)
    {
        return RunServiceCommand(ServiceControlUninstall);
    }

    if (_wcsicmp(command, L"status") == 0)
    {
        return RunStatusCommand();
    }

    SetLastError(ERROR_INVALID_PARAMETER);
    return ERROR_INVALID_PARAMETER;
}

BOOL AppTryHandleCommandLine(int* exitCode)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    BOOL shouldHandle;
    int localExitCode = ERROR_SUCCESS;

    if (argv == NULL)
    {
        if (exitCode != NULL)
        {
            *exitCode = (int)GetLastError();
        }

        return TRUE;
    }

    shouldHandle = argc > 1;
    if (shouldHandle)
    {
        if (argc == 2)
        {
            localExitCode = RunCommand(argv[1]);
        }
        else
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            localExitCode = (int)GetLastError();
        }
    }

    LocalFree(argv);

    if (shouldHandle && exitCode != NULL)
    {
        *exitCode = localExitCode;
    }

    return shouldHandle;
}
