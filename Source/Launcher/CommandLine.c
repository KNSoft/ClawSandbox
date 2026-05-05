#include <Windows.h>
#include <intsafe.h>
#include <shellapi.h>

#include "AppShared.h"
#include "ServiceControl.h"

typedef DWORD (*ServiceCommandFn)(WCHAR* errorMessage, size_t errorMessageCount);

static DWORD BuildMultiSz(int count, wchar_t** values, PWSTR* multiSz)
{
    size_t charCount = 1;
    size_t index;
    int i;
    PWSTR buffer;
    PWSTR cursor;

    *multiSz = NULL;
    for (i = 0; i < count; i++)
    {
        if (FAILED(SizeTAdd(charCount, wcslen(values[i]) + 1, &charCount)))
        {
            return ERROR_ARITHMETIC_OVERFLOW;
        }
    }

    buffer = (PWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, charCount * sizeof(WCHAR));
    if (buffer == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    cursor = buffer;
    for (i = 0; i < count; i++)
    {
        index = wcslen(values[i]);
        CopyMemory(cursor, values[i], index * sizeof(WCHAR));
        cursor += index + 1;
    }

    *multiSz = buffer;
    return ERROR_SUCCESS;
}

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

static int RunClawTypeCommand(const WCHAR* clawType)
{
    WCHAR errorMessage[SERVICE_CONTROL_ERROR_CAPACITY] = { 0 };
    DWORD error = ServiceControlInitialize(errorMessage, ARRAYSIZE(errorMessage));

    if (error != ERROR_SUCCESS)
    {
        return (int)error;
    }

    return (int)ServiceControlSetClawType(clawType, errorMessage, ARRAYSIZE(errorMessage));
}

static int RunSelfProtectionCommand(const WCHAR* value)
{
    WCHAR errorMessage[SERVICE_CONTROL_ERROR_CAPACITY] = { 0 };
    DWORD error;
    BOOL enabled;

    if (_wcsicmp(value, L"on") == 0 || _wcsicmp(value, L"enable") == 0 || _wcsicmp(value, L"enabled") == 0)
    {
        enabled = TRUE;
    }
    else if (_wcsicmp(value, L"off") == 0 || _wcsicmp(value, L"disable") == 0 || _wcsicmp(value, L"disabled") == 0)
    {
        enabled = FALSE;
    }
    else
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return ERROR_INVALID_PARAMETER;
    }

    error = ServiceControlInitialize(errorMessage, ARRAYSIZE(errorMessage));
    if (error != ERROR_SUCCESS)
    {
        return (int)error;
    }

    return (int)ServiceControlSetSelfProtection(enabled, errorMessage, ARRAYSIZE(errorMessage));
}

static int RunFsWhiteListCommand(int count, wchar_t** values)
{
    WCHAR errorMessage[SERVICE_CONTROL_ERROR_CAPACITY] = { 0 };
    PWSTR multiSz = NULL;
    DWORD error;

    error = ServiceControlInitialize(errorMessage, ARRAYSIZE(errorMessage));
    if (error != ERROR_SUCCESS)
    {
        return (int)error;
    }

    error = BuildMultiSz(count, values, &multiSz);
    if (error != ERROR_SUCCESS)
    {
        return (int)error;
    }

    error = ServiceControlSetFsWhiteList(multiSz, errorMessage, ARRAYSIZE(errorMessage));
    HeapFree(GetProcessHeap(), 0, multiSz);
    return (int)error;
}

static int RunCommand(int argc, LPWSTR* argv)
{
    const WCHAR* command = argv[1];

    if (_wcsicmp(command, L"install") == 0)
    {
        if (argc != 2)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return ERROR_INVALID_PARAMETER;
        }
        return RunServiceCommand(ServiceControlInstall);
    }

    if (_wcsicmp(command, L"start") == 0)
    {
        if (argc != 2)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return ERROR_INVALID_PARAMETER;
        }
        return RunServiceCommand(ServiceControlStart);
    }

    if (_wcsicmp(command, L"stop") == 0)
    {
        if (argc != 2)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return ERROR_INVALID_PARAMETER;
        }
        return RunServiceCommand(ServiceControlStop);
    }

    if (_wcsicmp(command, L"uninstall") == 0)
    {
        if (argc != 2)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return ERROR_INVALID_PARAMETER;
        }
        return RunServiceCommand(ServiceControlUninstall);
    }

    if (_wcsicmp(command, L"status") == 0)
    {
        if (argc != 2)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return ERROR_INVALID_PARAMETER;
        }
        return RunStatusCommand();
    }

    if (_wcsicmp(command, L"claw-type") == 0 && argc <= 3)
    {
        return RunClawTypeCommand(argc == 3 ? argv[2] : L"");
    }

    if (_wcsicmp(command, L"self-protection") == 0 && argc == 3)
    {
        return RunSelfProtectionCommand(argv[2]);
    }

    if (_wcsicmp(command, L"fs-whitelist") == 0)
    {
        return RunFsWhiteListCommand(argc - 2, argv + 2);
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
        localExitCode = RunCommand(argc, argv);
    }

    LocalFree(argv);

    if (shouldHandle && exitCode != NULL)
    {
        *exitCode = localExitCode;
    }

    return shouldHandle;
}
