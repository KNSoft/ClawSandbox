#define CLAWSANDBOX_USERDLL_EXPORTS
#include "UserDll.h"

#include <fltuser.h>
#include <strsafe.h>

static const WCHAR kServiceName[] = L"ClawSandbox";
static const WCHAR kServiceDescription[] = L"ClawSandbox file activity minifilter";
static const WCHAR kServiceGroup[] = L"FSFilter Activity Monitor";
static const WCHAR kDefaultInstance[] = L"ClawSandbox Instance";
static const WCHAR kDefaultAltitude[] = L"370131";
static const DWORD kSelfProtection = 0;
static const WCHAR kClawType[] = L"";
static HMODULE ModuleHandle;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    UNREFERENCED_PARAMETER(reserved);

    if (reason == DLL_PROCESS_ATTACH)
    {
        ModuleHandle = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

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

static void FormatWin32Message(DWORD error, WCHAR* buffer, size_t bufferCount)
{
    WCHAR* systemBuffer = NULL;
    DWORD length;

    ClearErrorMessage(buffer, bufferCount);
    if (buffer == NULL || bufferCount == 0)
    {
        return;
    }

    length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        0,
        (LPWSTR)&systemBuffer,
        0,
        NULL);

    if (length != 0 && systemBuffer != NULL)
    {
        while (length != 0 && (systemBuffer[length - 1] == L'\r' || systemBuffer[length - 1] == L'\n'))
        {
            systemBuffer[--length] = L'\0';
        }

        StringCchCopyW(buffer, bufferCount, systemBuffer);
        LocalFree(systemBuffer);
        return;
    }

    if (systemBuffer != NULL)
    {
        LocalFree(systemBuffer);
    }

    StringCchPrintfW(buffer, bufferCount, L"Win32 error 0x%08lX", error);
}

static void FormatHresultMessage(HRESULT hr, WCHAR* buffer, size_t bufferCount)
{
    DWORD messageId = (DWORD)hr;

    if (HRESULT_FACILITY(hr) == FACILITY_WIN32)
    {
        messageId = HRESULT_CODE(hr);
    }

    FormatWin32Message(messageId, buffer, bufferCount);
    if (buffer != NULL && bufferCount != 0 && wcsncmp(buffer, L"Win32 error", 11) == 0)
    {
        StringCchPrintfW(buffer, bufferCount, L"HRESULT 0x%08lX", (unsigned long)hr);
    }
}

static DWORD Win32FromHresult(HRESULT hr)
{
    if (HRESULT_FACILITY(hr) == FACILITY_WIN32)
    {
        return HRESULT_CODE(hr);
    }

    return (DWORD)hr;
}

static DWORD ResultFromBool(BOOL success)
{
    return success ? ERROR_SUCCESS : GetLastError();
}

static BOOL EnablePrivilege(PCWSTR privilegeName, WCHAR* errorMessage, size_t errorMessageCount)
{
    HANDLE token = NULL;
    TOKEN_PRIVILEGES tokenPrivileges;
    LUID privilegeLuid;
    DWORD lastError;

    ClearErrorMessage(errorMessage, errorMessageCount);

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return FALSE;
    }

    if (!LookupPrivilegeValueW(NULL, privilegeName, &privilegeLuid))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        CloseHandle(token);
        return FALSE;
    }

    ZeroMemory(&tokenPrivileges, sizeof(tokenPrivileges));
    tokenPrivileges.PrivilegeCount = 1;
    tokenPrivileges.Privileges[0].Luid = privilegeLuid;
    tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    SetLastError(ERROR_SUCCESS);
    if (!AdjustTokenPrivileges(token, FALSE, &tokenPrivileges, sizeof(tokenPrivileges), NULL, NULL))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        CloseHandle(token);
        return FALSE;
    }

    lastError = GetLastError();
    CloseHandle(token);

    if (lastError == ERROR_SUCCESS)
    {
        return TRUE;
    }

    if (lastError == ERROR_NOT_ALL_ASSIGNED)
    {
        SetErrorMessage(errorMessage, errorMessageCount, L"SeLoadDriverPrivilege is not available in the current token.");
        return FALSE;
    }

    FormatWin32Message(lastError, errorMessage, errorMessageCount);
    return FALSE;
}

static BOOL GetModuleDirectory(WCHAR* directory, size_t directoryCount)
{
    DWORD length;
    WCHAR* slash;

    ClearErrorMessage(directory, directoryCount);
    if (directory == NULL || directoryCount == 0)
    {
        return FALSE;
    }

    length = GetModuleFileNameW(ModuleHandle, directory, (DWORD)directoryCount);
    if (length == 0 || length >= directoryCount)
    {
        directory[0] = L'\0';
        return FALSE;
    }

    slash = wcsrchr(directory, L'\\');
    if (slash == NULL)
    {
        slash = wcsrchr(directory, L'/');
    }
    if (slash == NULL)
    {
        directory[0] = L'\0';
        return FALSE;
    }

    *slash = L'\0';
    return TRUE;
}

static BOOL BuildDriverPath(WCHAR* path, size_t pathCount)
{
    if (!GetModuleDirectory(path, pathCount))
    {
        return FALSE;
    }

    return SUCCEEDED(StringCchCatW(path, pathCount, L"\\ClawSandbox.sys"));
}

static BOOL FileExists(const WCHAR* path)
{
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static BOOL WriteStringValue(HKEY rootKey, const WCHAR* subKey, const WCHAR* valueName, const WCHAR* value)
{
    HKEY key = NULL;
    LONG openStatus;
    LONG writeStatus;
    DWORD byteCount;

    openStatus = RegCreateKeyExW(rootKey, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (openStatus != ERROR_SUCCESS)
    {
        SetLastError((DWORD)openStatus);
        return FALSE;
    }

    byteCount = (DWORD)((wcslen(value) + 1) * sizeof(WCHAR));
    writeStatus = RegSetValueExW(key, valueName, 0, REG_SZ, (const BYTE*)value, byteCount);
    RegCloseKey(key);

    if (writeStatus != ERROR_SUCCESS)
    {
        SetLastError((DWORD)writeStatus);
        return FALSE;
    }

    return TRUE;
}

static BOOL WriteDwordValue(HKEY rootKey, const WCHAR* subKey, const WCHAR* valueName, DWORD value)
{
    HKEY key = NULL;
    LONG openStatus;
    LONG writeStatus;

    openStatus = RegCreateKeyExW(rootKey, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (openStatus != ERROR_SUCCESS)
    {
        SetLastError((DWORD)openStatus);
        return FALSE;
    }

    writeStatus = RegSetValueExW(key, valueName, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
    RegCloseKey(key);

    if (writeStatus != ERROR_SUCCESS)
    {
        SetLastError((DWORD)writeStatus);
        return FALSE;
    }

    return TRUE;
}

static DWORD MultiSzByteCount(PCWSTR multiSz)
{
    PCWSTR cursor;

    if (multiSz == NULL)
    {
        return 0;
    }

    cursor = multiSz;
    while (*cursor != L'\0' || cursor[1] != L'\0')
    {
        cursor++;
    }

    return (DWORD)((cursor - multiSz + 2) * sizeof(WCHAR));
}

static BOOL WriteMultiSzValue(HKEY rootKey, const WCHAR* subKey, const WCHAR* valueName, PCWSTR value)
{
    HKEY key = NULL;
    LONG openStatus;
    LONG writeStatus;
    DWORD byteCount;
    static const WCHAR emptyMultiSz[] = L"\0";

    openStatus = RegCreateKeyExW(rootKey, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL);
    if (openStatus != ERROR_SUCCESS)
    {
        SetLastError((DWORD)openStatus);
        return FALSE;
    }

    if (value == NULL)
    {
        value = emptyMultiSz;
    }

    byteCount = MultiSzByteCount(value);
    writeStatus = RegSetValueExW(key, valueName, 0, REG_MULTI_SZ, (const BYTE*)value, byteCount);
    RegCloseKey(key);

    if (writeStatus != ERROR_SUCCESS)
    {
        SetLastError((DWORD)writeStatus);
        return FALSE;
    }

    return TRUE;
}

static BOOL ConfigureServiceRegistry(WCHAR* errorMessage, size_t errorMessageCount)
{
    WCHAR instancesKey[256];
    WCHAR defaultInstanceKey[256];
    const WCHAR serviceRoot[] = L"SYSTEM\\CurrentControlSet\\Services\\ClawSandbox";
    const WCHAR parametersKey[] = L"SYSTEM\\CurrentControlSet\\Services\\ClawSandbox\\Parameters\\ClawSandbox";

    StringCchPrintfW(instancesKey, ARRAYSIZE(instancesKey), L"%s\\Parameters\\Instances", serviceRoot);
    StringCchPrintfW(defaultInstanceKey, ARRAYSIZE(defaultInstanceKey), L"%s\\%s", instancesKey, kDefaultInstance);

    if (!WriteStringValue(HKEY_LOCAL_MACHINE, instancesKey, L"DefaultInstance", kDefaultInstance))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return FALSE;
    }

    if (!WriteStringValue(HKEY_LOCAL_MACHINE, defaultInstanceKey, L"Altitude", kDefaultAltitude))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return FALSE;
    }

    if (!WriteDwordValue(HKEY_LOCAL_MACHINE, defaultInstanceKey, L"Flags", 0))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return FALSE;
    }

    if (!WriteDwordValue(HKEY_LOCAL_MACHINE, parametersKey, L"SelfProtection", kSelfProtection))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return FALSE;
    }

    if (!WriteStringValue(HKEY_LOCAL_MACHINE, parametersKey, L"ClawType", kClawType))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return FALSE;
    }

    return TRUE;
}

static BOOL InstallDriver(WCHAR* errorMessage, size_t errorMessageCount)
{
    WCHAR driverPath[MAX_PATH];
    SC_HANDLE scm = NULL;
    SC_HANDLE service = NULL;
    BOOL createdService = FALSE;
    const WCHAR dependencies[] = L"FltMgr\0";
    SERVICE_DESCRIPTIONW description;

    ClearErrorMessage(errorMessage, errorMessageCount);

    if (!BuildDriverPath(driverPath, ARRAYSIZE(driverPath)))
    {
        SetLastError(ERROR_PATH_NOT_FOUND);
        SetErrorMessage(errorMessage, errorMessageCount, L"Unable to resolve the driver path.");
        return FALSE;
    }

    if (!FileExists(driverPath))
    {
        SetLastError(ERROR_FILE_NOT_FOUND);
        StringCchPrintfW(errorMessage, errorMessageCount, L"Driver binary not found:\n%s", driverPath);
        return FALSE;
    }

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL)
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return FALSE;
    }

    service = OpenServiceW(scm, kServiceName, SERVICE_QUERY_STATUS | DELETE | SERVICE_CHANGE_CONFIG);
    if (service == NULL)
    {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_MARKED_FOR_DELETE)
        {
            SetLastError(error);
            SetErrorMessage(errorMessage, errorMessageCount, L"The service is marked for deletion. Close any remaining handles and try again.");
            CloseServiceHandle(scm);
            return FALSE;
        }

        if (error != ERROR_SERVICE_DOES_NOT_EXIST)
        {
            FormatWin32Message(error, errorMessage, errorMessageCount);
            CloseServiceHandle(scm);
            return FALSE;
        }
    }

    if (service == NULL)
    {
        service = CreateServiceW(
            scm,
            kServiceName,
            kServiceName,
            SERVICE_QUERY_STATUS | DELETE | SERVICE_CHANGE_CONFIG,
            SERVICE_FILE_SYSTEM_DRIVER,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL,
            driverPath,
            kServiceGroup,
            NULL,
            dependencies,
            NULL,
            NULL);
        if (service == NULL)
        {
            FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
            CloseServiceHandle(scm);
            return FALSE;
        }
        createdService = TRUE;
    }

    if (!ChangeServiceConfigW(
            service,
            SERVICE_FILE_SYSTEM_DRIVER,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL,
            driverPath,
            kServiceGroup,
            NULL,
            dependencies,
            NULL,
            NULL,
            kServiceName))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        if (createdService)
        {
            DeleteService(service);
        }
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return FALSE;
    }

    ZeroMemory(&description, sizeof(description));
    description.lpDescription = (LPWSTR)kServiceDescription;
    if (!ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &description))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        if (createdService)
        {
            DeleteService(service);
        }
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return FALSE;
    }

    if (!ConfigureServiceRegistry(errorMessage, errorMessageCount))
    {
        if (createdService)
        {
            DeleteService(service);
        }
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return FALSE;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

static BOOL UninstallDriver(WCHAR* errorMessage, size_t errorMessageCount)
{
    SC_HANDLE scm = NULL;
    SC_HANDLE service = NULL;
    SERVICE_STATUS_PROCESS status;
    DWORD bytesNeeded = 0;

    ClearErrorMessage(errorMessage, errorMessageCount);

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm == NULL)
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return FALSE;
    }

    service = OpenServiceW(scm, kServiceName, SERVICE_QUERY_STATUS | DELETE);
    if (service == NULL)
    {
        DWORD error = GetLastError();
        CloseServiceHandle(scm);

        if (error == ERROR_SERVICE_DOES_NOT_EXIST || error == ERROR_SERVICE_MARKED_FOR_DELETE)
        {
            return TRUE;
        }

        FormatWin32Message(error, errorMessage, errorMessageCount);
        return FALSE;
    }

    ZeroMemory(&status, sizeof(status));
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(status), &bytesNeeded))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return FALSE;
    }

    if (status.dwCurrentState != SERVICE_STOPPED)
    {
        SetLastError(ERROR_BUSY);
        SetErrorMessage(errorMessage, errorMessageCount, L"The driver is still running. Stop it before uninstalling.");
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return FALSE;
    }

    if (!DeleteService(service))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return FALSE;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

DWORD ClawSandboxInitialize(WCHAR* errorMessage, size_t errorMessageCount)
{
    if (!EnablePrivilege(SE_LOAD_DRIVER_NAME, errorMessage, errorMessageCount))
    {
        return GetLastError();
    }

    return ERROR_SUCCESS;
}

DWORD ClawSandboxInstallService(WCHAR* errorMessage, size_t errorMessageCount)
{
    CLAWSANDBOX_SERVICE_STATE state = ClawSandboxQueryServiceState();

    ClearErrorMessage(errorMessage, errorMessageCount);
    if (!state.queryOk)
    {
        return GetLastError();
    }

    return ResultFromBool(InstallDriver(errorMessage, errorMessageCount));
}

DWORD ClawSandboxSetOptions(const CLAWSANDBOX_OPTIONS* options, WCHAR* errorMessage, size_t errorMessageCount)
{
    CLAWSANDBOX_SERVICE_STATE state;
    const WCHAR parametersKey[] = L"SYSTEM\\CurrentControlSet\\Services\\ClawSandbox\\Parameters\\ClawSandbox";
    const DWORD supportedFlags = CLAWSANDBOX_OPTION_SELF_PROTECTION |
                                 CLAWSANDBOX_OPTION_FS_WHITE_LIST |
                                 CLAWSANDBOX_OPTION_CLAW_TYPE;

    ClearErrorMessage(errorMessage, errorMessageCount);
    if (options == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        SetErrorMessage(errorMessage, errorMessageCount, L"Options must not be NULL.");
        return ERROR_INVALID_PARAMETER;
    }
    if (options->flags == 0 || (options->flags & ~supportedFlags) != 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        SetErrorMessage(errorMessage, errorMessageCount, L"Options contain invalid flags.");
        return ERROR_INVALID_PARAMETER;
    }

    state = ClawSandboxQueryServiceState();
    if (!state.queryOk)
    {
        SetErrorMessage(errorMessage, errorMessageCount, L"Unable to query the service state.");
        return GetLastError();
    }
    if (!state.installed)
    {
        SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
        SetErrorMessage(errorMessage, errorMessageCount, L"The driver is not installed.");
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    if ((options->flags & CLAWSANDBOX_OPTION_SELF_PROTECTION) != 0 &&
        !WriteDwordValue(HKEY_LOCAL_MACHINE, parametersKey, L"SelfProtection", options->selfProtection ? 1 : 0))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return GetLastError();
    }

    if ((options->flags & CLAWSANDBOX_OPTION_FS_WHITE_LIST) != 0 &&
        !WriteMultiSzValue(HKEY_LOCAL_MACHINE, parametersKey, L"FSWhiteList", options->fsWhiteListMultiSz))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return GetLastError();
    }

    if ((options->flags & CLAWSANDBOX_OPTION_CLAW_TYPE) != 0 &&
        !WriteStringValue(HKEY_LOCAL_MACHINE, parametersKey, L"ClawType", options->clawType != NULL ? options->clawType : L""))
    {
        FormatWin32Message(GetLastError(), errorMessage, errorMessageCount);
        return GetLastError();
    }

    return ERROR_SUCCESS;
}

DWORD ClawSandboxSetSelfProtection(BOOL enabled, WCHAR* errorMessage, size_t errorMessageCount)
{
    CLAWSANDBOX_OPTIONS options;

    ZeroMemory(&options, sizeof(options));
    options.flags = CLAWSANDBOX_OPTION_SELF_PROTECTION;
    options.selfProtection = enabled;
    return ClawSandboxSetOptions(&options, errorMessage, errorMessageCount);
}

DWORD ClawSandboxSetFsWhiteList(PCWSTR fsWhiteListMultiSz, WCHAR* errorMessage, size_t errorMessageCount)
{
    CLAWSANDBOX_OPTIONS options;

    ZeroMemory(&options, sizeof(options));
    options.flags = CLAWSANDBOX_OPTION_FS_WHITE_LIST;
    options.fsWhiteListMultiSz = fsWhiteListMultiSz;
    return ClawSandboxSetOptions(&options, errorMessage, errorMessageCount);
}

DWORD ClawSandboxSetClawType(PCWSTR clawType, WCHAR* errorMessage, size_t errorMessageCount)
{
    CLAWSANDBOX_OPTIONS options;

    ZeroMemory(&options, sizeof(options));
    options.flags = CLAWSANDBOX_OPTION_CLAW_TYPE;
    options.clawType = clawType;
    return ClawSandboxSetOptions(&options, errorMessage, errorMessageCount);
}

CLAWSANDBOX_SERVICE_STATE ClawSandboxQueryServiceState(void)
{
    CLAWSANDBOX_SERVICE_STATE state;
    SC_HANDLE scm = NULL;
    SC_HANDLE service = NULL;
    SERVICE_STATUS_PROCESS status;
    DWORD bytesNeeded = 0;

    ZeroMemory(&state, sizeof(state));
    state.queryOk = TRUE;

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm == NULL)
    {
        state.queryOk = FALSE;
        SetLastError(GetLastError());
        return state;
    }

    service = OpenServiceW(scm, kServiceName, SERVICE_QUERY_STATUS);
    if (service == NULL)
    {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_MARKED_FOR_DELETE)
        {
            state.installed = TRUE;
            state.deletePending = TRUE;
        }
        else if (error != ERROR_SERVICE_DOES_NOT_EXIST)
        {
            state.queryOk = FALSE;
            SetLastError(error);
        }
        else
        {
            SetLastError(ERROR_SUCCESS);
        }

        CloseServiceHandle(scm);
        return state;
    }

    state.installed = TRUE;
    ZeroMemory(&status, sizeof(status));
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(status), &bytesNeeded))
    {
        state.queryOk = FALSE;
        SetLastError(GetLastError());
    }
    else
    {
        state.running = status.dwCurrentState == SERVICE_RUNNING;
        state.startPending = status.dwCurrentState == SERVICE_START_PENDING;
        state.stopPending = status.dwCurrentState == SERVICE_STOP_PENDING;
        SetLastError(ERROR_SUCCESS);
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return state;
}

DWORD ClawSandboxStartService(WCHAR* errorMessage, size_t errorMessageCount)
{
    CLAWSANDBOX_SERVICE_STATE state = ClawSandboxQueryServiceState();
    HRESULT hr;

    ClearErrorMessage(errorMessage, errorMessageCount);

    if (!state.queryOk)
    {
        if (GetLastError() == ERROR_SUCCESS)
        {
            SetLastError(ERROR_GEN_FAILURE);
        }
        SetErrorMessage(errorMessage, errorMessageCount, L"Unable to query the service state.");
        return GetLastError();
    }

    if (!state.installed)
    {
        SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
        SetErrorMessage(errorMessage, errorMessageCount, L"The driver is not installed.");
        return GetLastError();
    }

    if (state.running || state.startPending)
    {
        return ERROR_SUCCESS;
    }

    hr = FilterLoad(kServiceName);
    if (SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_SERVICE_ALREADY_RUNNING) || hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))
    {
        return ERROR_SUCCESS;
    }

    SetLastError(HRESULT_FACILITY(hr) == FACILITY_WIN32 ? HRESULT_CODE(hr) : (DWORD)hr);
    FormatHresultMessage(hr, errorMessage, errorMessageCount);
    return GetLastError();
}

DWORD ClawSandboxStopService(WCHAR* errorMessage, size_t errorMessageCount)
{
    CLAWSANDBOX_SERVICE_STATE state = ClawSandboxQueryServiceState();
    HRESULT hr;

    ClearErrorMessage(errorMessage, errorMessageCount);

    if (!state.queryOk)
    {
        if (GetLastError() == ERROR_SUCCESS)
        {
            SetLastError(ERROR_GEN_FAILURE);
        }
        SetErrorMessage(errorMessage, errorMessageCount, L"Unable to query the service state.");
        return GetLastError();
    }

    if (!state.installed || (!state.running && !state.startPending))
    {
        return ERROR_SUCCESS;
    }

    hr = FilterUnload(kServiceName);
    if (SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_ACTIVE))
    {
        return ERROR_SUCCESS;
    }

    SetLastError(HRESULT_FACILITY(hr) == FACILITY_WIN32 ? HRESULT_CODE(hr) : (DWORD)hr);
    FormatHresultMessage(hr, errorMessage, errorMessageCount);
    return GetLastError();
}

DWORD ClawSandboxUninstallService(WCHAR* errorMessage, size_t errorMessageCount)
{
    CLAWSANDBOX_SERVICE_STATE state = ClawSandboxQueryServiceState();

    ClearErrorMessage(errorMessage, errorMessageCount);
    if (!state.queryOk)
    {
        return GetLastError();
    }

    if (!state.installed || state.deletePending)
    {
        return ERROR_SUCCESS;
    }

    return ResultFromBool(UninstallDriver(errorMessage, errorMessageCount));
}

static DWORD QueryTrackedProcessIdsInternal(PDWORD* processIds, PDWORD count, WCHAR* errorMessage, size_t errorMessageCount)
{
    HANDLE port = INVALID_HANDLE_VALUE;
    CLAWSANDBOX_MESSAGE_HEADER request;
    PCLAWSANDBOX_TRACKED_PIDS_REPLY reply = NULL;
    DWORD capacity = 32;
    DWORD result = ERROR_SUCCESS;
    HRESULT hr;

    if (processIds == NULL || count == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        SetErrorMessage(errorMessage, errorMessageCount, L"Output parameter must not be NULL.");
        return ERROR_INVALID_PARAMETER;
    }

    *processIds = NULL;
    *count = 0;
    ClearErrorMessage(errorMessage, errorMessageCount);

    hr = FilterConnectCommunicationPort(CLAWSANDBOX_PORT_NAME, 0, NULL, 0, NULL, &port);
    if (FAILED(hr))
    {
        result = Win32FromHresult(hr);
        FormatHresultMessage(hr, errorMessage, errorMessageCount);
        return result;
    }

    ZeroMemory(&request, sizeof(request));
    request.Version = CLAWSANDBOX_PROTOCOL_VERSION;
    request.Message = CLAWSANDBOX_MESSAGE_QUERY_TRACKED_PIDS;

    for (;;)
    {
        DWORD replySize;
        DWORD bytesReturned = 0;
        DWORD copyCount;
        PDWORD ids;

        replySize = FIELD_OFFSET(CLAWSANDBOX_TRACKED_PIDS_REPLY, ProcessIds) +
                    capacity * sizeof(ULONG_PTR);
        reply = (PCLAWSANDBOX_TRACKED_PIDS_REPLY)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, replySize);
        if (reply == NULL)
        {
            result = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        hr = FilterSendMessage(port, &request, sizeof(request), reply, replySize, &bytesReturned);
        if (FAILED(hr) && reply->Count <= reply->Capacity)
        {
            result = Win32FromHresult(hr);
            FormatHresultMessage(hr, errorMessage, errorMessageCount);
            break;
        }

        if (reply->Count > reply->Capacity)
        {
            capacity = reply->Count;
            HeapFree(GetProcessHeap(), 0, reply);
            reply = NULL;
            continue;
        }

        copyCount = reply->Count;
        ids = NULL;
        if (copyCount != 0)
        {
            DWORD i;

            ids = (PDWORD)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, copyCount * sizeof(DWORD));
            if (ids == NULL)
            {
                result = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            for (i = 0; i < copyCount; i++)
            {
                ids[i] = (DWORD)reply->ProcessIds[i];
            }
        }

        *processIds = ids;
        *count = copyCount;
        result = ERROR_SUCCESS;
        break;
    }

    if (reply != NULL)
    {
        HeapFree(GetProcessHeap(), 0, reply);
    }
    if (port != INVALID_HANDLE_VALUE)
    {
        CloseHandle(port);
    }

    if (result != ERROR_SUCCESS && errorMessage != NULL && errorMessageCount != 0 && errorMessage[0] == L'\0')
    {
        FormatWin32Message(result, errorMessage, errorMessageCount);
    }
    return result;
}

DWORD ClawSandboxQueryTrackedProcessIds(DWORD* processIds, DWORD capacity, DWORD* count, WCHAR* errorMessage, size_t errorMessageCount)
{
    PDWORD localProcessIds = NULL;
    DWORD localCount = 0;
    DWORD result;

    if (count == NULL || (capacity != 0 && processIds == NULL))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        SetErrorMessage(errorMessage, errorMessageCount, L"Output parameter must not be NULL.");
        return ERROR_INVALID_PARAMETER;
    }

    result = QueryTrackedProcessIdsInternal(&localProcessIds, &localCount, errorMessage, errorMessageCount);
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    *count = localCount;
    if (localCount > capacity)
    {
        HeapFree(GetProcessHeap(), 0, localProcessIds);
        SetLastError(ERROR_MORE_DATA);
        SetErrorMessage(errorMessage, errorMessageCount, L"The output buffer is too small.");
        return ERROR_MORE_DATA;
    }

    if (localCount != 0)
    {
        CopyMemory(processIds, localProcessIds, localCount * sizeof(DWORD));
    }

    HeapFree(GetProcessHeap(), 0, localProcessIds);
    return ERROR_SUCCESS;
}

DWORD ClawSandboxQueryTrackedProcesses(CLAWSANDBOX_TRACKED_PROCESS* processes, DWORD capacity, DWORD* count, WCHAR* errorMessage, size_t errorMessageCount)
{
    PDWORD processIds = NULL;
    DWORD localCount = 0;
    DWORD result;
    DWORD i;

    if (count == NULL || (capacity != 0 && processes == NULL))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        SetErrorMessage(errorMessage, errorMessageCount, L"Output parameter must not be NULL.");
        return ERROR_INVALID_PARAMETER;
    }

    result = QueryTrackedProcessIdsInternal(&processIds, &localCount, errorMessage, errorMessageCount);
    if (result != ERROR_SUCCESS)
    {
        return result;
    }

    *count = localCount;
    if (localCount > capacity)
    {
        HeapFree(GetProcessHeap(), 0, processIds);
        SetLastError(ERROR_MORE_DATA);
        SetErrorMessage(errorMessage, errorMessageCount, L"The output buffer is too small.");
        return ERROR_MORE_DATA;
    }

    for (i = 0; i < localCount; i++)
    {
        HANDLE processHandle;
        DWORD pathCapacity = CLAWSANDBOX_PROCESS_PATH_CAPACITY;

        ZeroMemory(&processes[i], sizeof(processes[i]));
        processes[i].processId = processIds[i];

        processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processIds[i]);
        if (processHandle == NULL)
        {
            continue;
        }

        if (QueryFullProcessImageNameW(processHandle, 0, processes[i].imagePath, &pathCapacity))
        {
            processes[i].imagePathAvailable = TRUE;
        }
        CloseHandle(processHandle);
    }

    HeapFree(GetProcessHeap(), 0, processIds);
    return ERROR_SUCCESS;
}
