#include "Driver.h"

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

/* AutoClaw */

static CONST UNICODE_STRING AutoClawProcessName = RTL_CONSTANT_STRING(L"AutoClaw.exe");

static CONST UNICODE_STRING AutoClawPart = RTL_CONSTANT_STRING(L"AutoClaw");

static
_Function_class_(FN_PROCESS_TRACK_CALLBACK)
BOOLEAN
IsAutoClawProcess(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    return PathEndsWithComponentInsensitive(CreateInfo->ImageFileName, &AutoClawProcessName);
}

static
_Function_class_(FN_FILE_WRITE_CALLBACK)
BOOLEAN
IsAutoClawAllowWritePath(
    _In_ PCUNICODE_STRING Path,
    _In_ PCFLT_RELATED_OBJECTS FltObjects)
{
    UNREFERENCED_PARAMETER(FltObjects);
    return StringContainsInSensetive(Path, &AutoClawPart);
}

/* The whole list */

static RULE_CLAW_TYPE g_aClawTypes[] = {
    { L"OpenClaw", NULL, &IsOpenClawProcess, &IsOpenClawAllowWritePath },
    { L"LobsterAI", &LobsterAIProcessName, &IsLobsterAIProcess, &IsLobsterAIAllowWritePath },
    { L"EasyClaw", &EasyClawProcessName, &IsEasyClawProcess, &IsEasyClawAllowWritePath },
    { L"AutoClaw", &AutoClawProcessName, &IsAutoClawProcess, &IsAutoClawAllowWritePath },
};

_Ret_maybenull_
PRULE_CLAW_TYPE
RuleListMatchClawTypeCreate(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    if (CreateInfo == NULL ||
        CreateInfo->ImageFileName == NULL ||
        CreateInfo->CommandLine == NULL)
    {
        return NULL;
    }

    for (ULONG i = 0; i < ARRAYSIZE(g_aClawTypes); i++)
    {
        if (g_aClawTypes[i].ProcessTrackCallback(CreateInfo))
        {
            return &g_aClawTypes[i];
        }
    }
    return NULL;
}

_Ret_maybenull_
PRULE_CLAW_TYPE
RuleListMatchClawTypeImageName(
    _In_ PCUNICODE_STRING ImageName)
{
    if (ImageName == NULL)
    {
        return NULL;
    }

    for (ULONG i = 0; i < ARRAYSIZE(g_aClawTypes); i++)
    {
        if (g_aClawTypes[i].SpecificProcessName != NULL &&
            PathEndsWithComponentInsensitive(ImageName, g_aClawTypes[i].SpecificProcessName))
        {
            return &g_aClawTypes[i];
        }
    }
    return NULL;
}
