#include "../UserDll/UserDll.h"

#include <intsafe.h>
#include <stdarg.h>
#include <stdio.h>
#include <strsafe.h>

static DWORD WriteUtf8(HANDLE output, PCWSTR text)
{
    char stackBuffer[1024];
    char* buffer;
    int byteCount;
    DWORD written;
    BOOL ok;

    if (text == NULL)
    {
        return ERROR_INVALID_PARAMETER;
    }

    byteCount = WideCharToMultiByte(CP_UTF8, 0, text, -1, stackBuffer, ARRAYSIZE(stackBuffer), NULL, NULL);
    if (byteCount != 0)
    {
        WriteFile(output, stackBuffer, (DWORD)byteCount - 1, &written, NULL);
        return ERROR_SUCCESS;
    }

    byteCount = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (byteCount == 0)
    {
        return GetLastError();
    }

    buffer = (char*)HeapAlloc(GetProcessHeap(), 0, byteCount);
    if (buffer == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    byteCount = WideCharToMultiByte(CP_UTF8, 0, text, -1, buffer, byteCount, NULL, NULL);
    ok = byteCount != 0 && WriteFile(output, buffer, (DWORD)byteCount - 1, &written, NULL);
    HeapFree(GetProcessHeap(), 0, buffer);
    return ok ? ERROR_SUCCESS : GetLastError();
}

static DWORD WriteFormatted(HANDLE output, PCWSTR format, ...)
{
    WCHAR stackBuffer[1024];
    WCHAR* buffer;
    va_list args;
    HRESULT hr;
    DWORD result;

    va_start(args, format);
    hr = StringCchVPrintfW(stackBuffer, ARRAYSIZE(stackBuffer), format, args);
    va_end(args);
    if (SUCCEEDED(hr))
    {
        return WriteUtf8(output, stackBuffer);
    }

    buffer = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, 32768 * sizeof(WCHAR));
    if (buffer == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    va_start(args, format);
    hr = StringCchVPrintfW(buffer, 32768, format, args);
    va_end(args);
    if (FAILED(hr))
    {
        HeapFree(GetProcessHeap(), 0, buffer);
        return HRESULT_CODE(hr);
    }

    result = WriteUtf8(output, buffer);
    HeapFree(GetProcessHeap(), 0, buffer);
    return result;
}

static void PrintUsage(void)
{
    WriteUtf8(
        GetStdHandle(STD_OUTPUT_HANDLE),
        L"Usage:\n"
        L"  ClawSandbox status\n"
        L"  ClawSandbox install\n"
        L"  ClawSandbox start\n"
        L"  ClawSandbox stop\n"
        L"  ClawSandbox uninstall\n"
        L"  ClawSandbox self-protection on|off\n"
        L"  ClawSandbox fs-whitelist [path ...]\n"
        L"  ClawSandbox claw-type [OpenClaw|LobsterAI|EasyClaw|AutoClaw]\n"
        L"  ClawSandbox tracked-pids\n"
        L"  ClawSandbox tracked\n");
}

static BOOL EqualCommand(PCWSTR left, PCWSTR right)
{
    return _wcsicmp(left, right) == 0;
}

static DWORD PrintError(DWORD error, PCWSTR message)
{
    if (message != NULL && message[0] != L'\0')
    {
        WriteFormatted(GetStdHandle(STD_ERROR_HANDLE), L"%s\n", message);
    }
    else
    {
        WriteFormatted(GetStdHandle(STD_ERROR_HANDLE), L"Operation failed: 0x%08lX\n", error);
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
        WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"not installed\n");
    }
    else if (state.deletePending)
    {
        WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"delete pending\n");
    }
    else if (state.running)
    {
        WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"running\n");
    }
    else if (state.startPending)
    {
        WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"start pending\n");
    }
    else if (state.stopPending)
    {
        WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"stop pending\n");
    }
    else
    {
        WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"stopped\n");
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

static DWORD SetClawType(PCWSTR value)
{
    WCHAR errorMessage[CLAWSANDBOX_ERROR_CAPACITY];
    DWORD result;

    result = InitializeForMutation();
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    result = ClawSandboxSetClawType(value, errorMessage, ARRAYSIZE(errorMessage));
    if (result != ERROR_SUCCESS)
    {
        return PrintError(result, errorMessage);
    }

    return ERROR_SUCCESS;
}

static DWORD QueryTrackedProcessIds(void)
{
    WCHAR errorMessage[CLAWSANDBOX_ERROR_CAPACITY];
    PDWORD processIds;
    DWORD capacity;
    DWORD count;
    DWORD result;
    DWORD i;

    capacity = 32;
    processIds = NULL;
    for (;;)
    {
        processIds = (PDWORD)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, capacity * sizeof(DWORD));
        if (processIds == NULL)
        {
            return PrintError(ERROR_NOT_ENOUGH_MEMORY, NULL);
        }

        result = ClawSandboxQueryTrackedProcessIds(processIds, capacity, &count, errorMessage, ARRAYSIZE(errorMessage));
        if (result != ERROR_MORE_DATA)
        {
            break;
        }

        HeapFree(GetProcessHeap(), 0, processIds);
        processIds = NULL;
        capacity = count;
    }

    if (result != ERROR_SUCCESS)
    {
        HeapFree(GetProcessHeap(), 0, processIds);
        return PrintError(result, errorMessage);
    }

    for (i = 0; i < count; i++)
    {
        WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"%lu\n", processIds[i]);
    }

    HeapFree(GetProcessHeap(), 0, processIds);
    return ERROR_SUCCESS;
}

static DWORD QueryTrackedProcesses(void)
{
    WCHAR errorMessage[CLAWSANDBOX_ERROR_CAPACITY];
    CLAWSANDBOX_TRACKED_PROCESS* processes;
    DWORD capacity;
    DWORD count;
    DWORD result;
    DWORD i;

    capacity = 32;
    processes = NULL;
    for (;;)
    {
        processes = (CLAWSANDBOX_TRACKED_PROCESS*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, capacity * sizeof(CLAWSANDBOX_TRACKED_PROCESS));
        if (processes == NULL)
        {
            return PrintError(ERROR_NOT_ENOUGH_MEMORY, NULL);
        }

        result = ClawSandboxQueryTrackedProcesses(processes, capacity, &count, errorMessage, ARRAYSIZE(errorMessage));
        if (result != ERROR_MORE_DATA)
        {
            break;
        }

        HeapFree(GetProcessHeap(), 0, processes);
        processes = NULL;
        capacity = count;
    }

    if (result != ERROR_SUCCESS)
    {
        HeapFree(GetProcessHeap(), 0, processes);
        return PrintError(result, errorMessage);
    }

    for (i = 0; i < count; i++)
    {
        if (processes[i].imagePathAvailable)
        {
            WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"%lu\t%s\n", processes[i].processId, processes[i].imagePath);
        }
        else
        {
            WriteFormatted(GetStdHandle(STD_OUTPUT_HANDLE), L"%lu\t%s\n", processes[i].processId, L"<path unavailable>");
        }
    }

    HeapFree(GetProcessHeap(), 0, processes);
    return ERROR_SUCCESS;
}

int wmain(int argc, wchar_t** argv)
{
    DWORD result;

    SetConsoleOutputCP(CP_UTF8);

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
    else if (EqualCommand(argv[1], L"claw-type") && argc <= 3)
    {
        result = SetClawType(argc == 3 ? argv[2] : L"");
    }
    else if (EqualCommand(argv[1], L"tracked-pids"))
    {
        result = QueryTrackedProcessIds();
    }
    else if (EqualCommand(argv[1], L"tracked"))
    {
        result = QueryTrackedProcesses();
    }
    else
    {
        PrintUsage();
        result = ERROR_INVALID_PARAMETER;
    }

    return result == ERROR_SUCCESS ? 0 : (int)result;
}
