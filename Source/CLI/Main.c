#include "../UserDll/UserDll.h"

#include <intsafe.h>
#include <stdio.h>
#include <strsafe.h>

static void PrintUsage(void)
{
    wprintf(L"Usage:\n");
    wprintf(L"  ClawSandbox status\n");
    wprintf(L"  ClawSandbox install\n");
    wprintf(L"  ClawSandbox start\n");
    wprintf(L"  ClawSandbox stop\n");
    wprintf(L"  ClawSandbox uninstall\n");
    wprintf(L"  ClawSandbox self-protection on|off\n");
    wprintf(L"  ClawSandbox fs-whitelist [path ...]\n");
}

static BOOL EqualCommand(PCWSTR left, PCWSTR right)
{
    return _wcsicmp(left, right) == 0;
}

static DWORD PrintError(DWORD error, PCWSTR message)
{
    if (message != NULL && message[0] != L'\0')
    {
        fwprintf(stderr, L"%s\n", message);
    }
    else
    {
        fwprintf(stderr, L"Operation failed: 0x%08lX\n", error);
    }
    return error;
}

static DWORD InitializeForMutation(void)
{
    WCHAR errorMessage[CLAWSANDBOX_ERROR_CAPACITY];
    DWORD result;

    result = ClawSandboxInitialize(errorMessage, ARRAYSIZE(errorMessage));
    if (result != ERROR_SUCCESS)
    {
        return PrintError(result, errorMessage);
    }

    return ERROR_SUCCESS;
}

static DWORD RunSimpleCommand(DWORD (*command)(WCHAR*, size_t))
{
    WCHAR errorMessage[CLAWSANDBOX_ERROR_CAPACITY];
    DWORD result;

    result = InitializeForMutation();
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    result = command(errorMessage, ARRAYSIZE(errorMessage));
    if (result != ERROR_SUCCESS)
    {
        return PrintError(result, errorMessage);
    }

    return ERROR_SUCCESS;
}

static DWORD PrintServiceState(void)
{
    CLAWSANDBOX_SERVICE_STATE state;
    DWORD error;

    state = ClawSandboxQueryServiceState();
    if (!state.queryOk)
    {
        error = GetLastError();
        return PrintError(error, L"Unable to query the service state.");
    }

    if (!state.installed)
    {
        wprintf(L"not installed\n");
    }
    else if (state.deletePending)
    {
        wprintf(L"delete pending\n");
    }
    else if (state.running)
    {
        wprintf(L"running\n");
    }
    else if (state.startPending)
    {
        wprintf(L"start pending\n");
    }
    else if (state.stopPending)
    {
        wprintf(L"stop pending\n");
    }
    else
    {
        wprintf(L"stopped\n");
    }

    return ERROR_SUCCESS;
}

static DWORD SetSelfProtection(PCWSTR value)
{
    WCHAR errorMessage[CLAWSANDBOX_ERROR_CAPACITY];
    DWORD result;
    BOOL enabled;

    if (EqualCommand(value, L"on") || EqualCommand(value, L"enable") || EqualCommand(value, L"enabled"))
    {
        enabled = TRUE;
    }
    else if (EqualCommand(value, L"off") || EqualCommand(value, L"disable") || EqualCommand(value, L"disabled"))
    {
        enabled = FALSE;
    }
    else
    {
        PrintUsage();
        return ERROR_INVALID_PARAMETER;
    }

    result = InitializeForMutation();
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    result = ClawSandboxSetSelfProtection(enabled, errorMessage, ARRAYSIZE(errorMessage));
    if (result != ERROR_SUCCESS)
    {
        return PrintError(result, errorMessage);
    }

    return ERROR_SUCCESS;
}

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

static DWORD SetFsWhiteList(int count, wchar_t** values)
{
    WCHAR errorMessage[CLAWSANDBOX_ERROR_CAPACITY];
    PWSTR multiSz = NULL;
    DWORD result;

    result = InitializeForMutation();
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    result = BuildMultiSz(count, values, &multiSz);
    if (result != ERROR_SUCCESS)
    {
        return PrintError(result, NULL);
    }

    result = ClawSandboxSetFsWhiteList(multiSz, errorMessage, ARRAYSIZE(errorMessage));
    HeapFree(GetProcessHeap(), 0, multiSz);
    if (result != ERROR_SUCCESS)
    {
        return PrintError(result, errorMessage);
    }

    return ERROR_SUCCESS;
}

int wmain(int argc, wchar_t** argv)
{
    DWORD result;

    if (argc < 2)
    {
        PrintUsage();
        return ERROR_INVALID_PARAMETER;
    }

    if (EqualCommand(argv[1], L"status"))
    {
        result = PrintServiceState();
    }
    else if (EqualCommand(argv[1], L"install"))
    {
        result = RunSimpleCommand(ClawSandboxInstallService);
    }
    else if (EqualCommand(argv[1], L"start"))
    {
        result = RunSimpleCommand(ClawSandboxStartService);
    }
    else if (EqualCommand(argv[1], L"stop"))
    {
        result = RunSimpleCommand(ClawSandboxStopService);
    }
    else if (EqualCommand(argv[1], L"uninstall"))
    {
        result = RunSimpleCommand(ClawSandboxUninstallService);
    }
    else if (EqualCommand(argv[1], L"self-protection") && argc == 3)
    {
        result = SetSelfProtection(argv[2]);
    }
    else if (EqualCommand(argv[1], L"fs-whitelist"))
    {
        result = SetFsWhiteList(argc - 2, argv + 2);
    }
    else
    {
        PrintUsage();
        result = ERROR_INVALID_PARAMETER;
    }

    return result == ERROR_SUCCESS ? 0 : (int)result;
}
