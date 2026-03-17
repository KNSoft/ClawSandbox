#include "Driver.h"

/*
 * Return TRUE to track.
 * The process will be tracked if any callback returns TRUE.
 */
_Function_class_(FN_PROCESS_TRACK_CALLBACK)
typedef
BOOLEAN
FN_PROCESS_TRACK_CALLBACK(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo);

/*
 * Return TRUE to allow write.
 * The file write operation will be allowed if any callback returns TRUE.
 */
_Function_class_(FN_FILE_WRITE_CALLBACK)
typedef
BOOLEAN
FN_FILE_WRITE_CALLBACK(
    _In_ PCUNICODE_STRING Path,
    _In_ PCFLT_RELATED_OBJECTS FltObjects);

/* OpenClaw */

static CONST UNICODE_STRING OpenClawProcessName = RTL_CONSTANT_STRING(L"node.exe");
static CONST UNICODE_STRING OpenClawCmdPart1A = RTL_CONSTANT_STRING(L"\\openclaw\\openclaw.mjs");
static CONST UNICODE_STRING OpenClawCmdPart1B = RTL_CONSTANT_STRING(L"\\openclaw\\dist\\index.js");
static CONST UNICODE_STRING OpenClawCmdPart2 = RTL_CONSTANT_STRING(L" gateway");

static CONST UNICODE_STRING OpenClawPart = RTL_CONSTANT_STRING(L"openclaw");

static
_Function_class_(FN_PROCESS_TRACK_CALLBACK)
BOOLEAN
IsOpenClawProcess(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    if (!PathEndsWithComponentInsensitive(CreateInfo->ImageFileName, &OpenClawProcessName))
    {
        return FALSE;
    }
    if ((!StringContainsInSensetive(CreateInfo->CommandLine, &OpenClawCmdPart1A) &&
         !StringContainsInSensetive(CreateInfo->CommandLine, &OpenClawCmdPart1B)) ||
        !StringContainsInSensetive(CreateInfo->CommandLine, &OpenClawCmdPart2))
    {
        return FALSE;
    }
    return TRUE;
}

static
_Function_class_(FN_FILE_WRITE_CALLBACK)
BOOLEAN
IsOpenClawAllowWritePath(
    _In_ PCUNICODE_STRING Path,
    _In_ PCFLT_RELATED_OBJECTS FltObjects)
{
    UNREFERENCED_PARAMETER(FltObjects);
    return StringContainsInSensetive(Path, &OpenClawPart);
}

/* LobsterAI */

static CONST UNICODE_STRING LobsterAIProcessName = RTL_CONSTANT_STRING(L"LobsterAI.exe");

static CONST UNICODE_STRING LobsterAIPart = RTL_CONSTANT_STRING(L"lobsterai");

static
_Function_class_(FN_PROCESS_TRACK_CALLBACK)
BOOLEAN
IsLobsterAIProcess(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    return PathEndsWithComponentInsensitive(CreateInfo->ImageFileName, &LobsterAIProcessName);
}

static
_Function_class_(FN_FILE_WRITE_CALLBACK)
BOOLEAN
IsLobsterAIAllowWritePath(
    _In_ PCUNICODE_STRING Path,
    _In_ PCFLT_RELATED_OBJECTS FltObjects)
{
    UNREFERENCED_PARAMETER(FltObjects);
    return StringContainsInSensetive(Path, &LobsterAIPart);
}

/* EasyClaw */

static CONST UNICODE_STRING EasyClawProcessName = RTL_CONSTANT_STRING(L"easyclaw.exe");

static CONST UNICODE_STRING EasyClawPart = RTL_CONSTANT_STRING(L"easyclaw");

static
_Function_class_(FN_PROCESS_TRACK_CALLBACK)
BOOLEAN
IsEasyClawProcess(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    return PathEndsWithComponentInsensitive(CreateInfo->ImageFileName, &EasyClawProcessName);
}

static
_Function_class_(FN_FILE_WRITE_CALLBACK)
BOOLEAN
IsEasyClawAllowWritePath(
    _In_ PCUNICODE_STRING Path,
    _In_ PCFLT_RELATED_OBJECTS FltObjects)
{
    UNREFERENCED_PARAMETER(FltObjects);
    return StringContainsInSensetive(Path, &EasyClawPart);
}

/* The whole list */

static FN_PROCESS_TRACK_CALLBACK* g_apfnProcessTrackCallbacks[] = {
    &IsOpenClawProcess,
    &IsLobsterAIProcess,
    &IsEasyClawProcess,
};

static FN_FILE_WRITE_CALLBACK* g_apfnFileWriteCallbacks[] = {
    &IsOpenClawAllowWritePath,
    &IsLobsterAIAllowWritePath,
    &IsEasyClawAllowWritePath,
};

BOOLEAN
RuleListShouldTrackCreate(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    for (ULONG i = 0; i < ARRAYSIZE(g_apfnProcessTrackCallbacks); i++)
    {
        if (g_apfnProcessTrackCallbacks[i](CreateInfo))
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOLEAN
RuleListIsAllowWritePath(
    _In_ PCUNICODE_STRING Path,
    _In_ PCFLT_RELATED_OBJECTS FltObjects)
{
    for (ULONG i = 0; i < ARRAYSIZE(g_apfnFileWriteCallbacks); i++)
    {
        if (g_apfnFileWriteCallbacks[i](Path, FltObjects))
        {
            return TRUE;
        }
    }
    return FALSE;
}
