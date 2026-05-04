#pragma once

#include <fltKernel.h>
#include <ntstrsafe.h>

/* WDK/SDK */

NTSYSAPI
NTSTATUS
NTAPI
ZwGetNextProcess(
    _In_opt_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ ULONG HandleAttributes,
    _In_ ULONG Flags,
    _Out_ PHANDLE NewProcessHandle);

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

#define LOG_PREFIX "[ClawSandbox] "
#define LOG(Level, Format, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, Level, LOG_PREFIX Format, ##__VA_ARGS__)
#define CLAWSANDBOX_TAG 'bSlC'

/* Utils.c */

BOOLEAN
StringContainsInSensetive(
    _In_ PCUNICODE_STRING String,
    _In_ PCUNICODE_STRING Token);

BOOLEAN
PathEndsWithComponentInsensitive(
    _In_ PCUNICODE_STRING Path,
    _In_ PCUNICODE_STRING FileName);

BOOLEAN
PathContainsComponentInsensitive(
    _In_ PCUNICODE_STRING Path,
    _In_ PCUNICODE_STRING Component);

/* Rule.List.c */

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
 */
_Function_class_(FN_FILE_WRITE_CALLBACK)
typedef
BOOLEAN
FN_FILE_WRITE_CALLBACK(
    _In_ PCUNICODE_STRING Path,
    _In_ PCFLT_RELATED_OBJECTS FltObjects);

typedef struct _RULE_CLAW_TYPE
{
    _Notnull_ PCWSTR Name;
    _Maybenull_ PCUNICODE_STRING SpecificProcessName;
    _Notnull_ FN_PROCESS_TRACK_CALLBACK* ProcessTrackCallback;
    _Notnull_ FN_FILE_WRITE_CALLBACK* FileWriteCallback;
} RULE_CLAW_TYPE, *PRULE_CLAW_TYPE;

_Ret_maybenull_
PRULE_CLAW_TYPE
RuleListMatchClawTypeCreate(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo);

_Ret_maybenull_
PRULE_CLAW_TYPE
RuleListMatchClawTypeImageName(
    _In_ PCUNICODE_STRING ImageName);

/* Rule.c */

NTSTATUS
RuleInitialize(
    _In_ PFLT_FILTER Filter,
    _In_ PCUNICODE_STRING RegistryPath);

VOID
RuleUninitialize(VOID);

_Ret_maybenull_
PRULE_CLAW_TYPE
RuleGetTrackedProcessClawType(
    _In_ HANDLE ProcessId);

BOOLEAN
RuleHasTrackedProcess(VOID);

BOOLEAN
RuleIsSelfProtectionEnabled(VOID);

NTSTATUS
RuleTrackProcess(
    _In_ HANDLE ProcessId,
    _In_ PRULE_CLAW_TYPE ClawType);

BOOLEAN
RuleUntrackProcess(
    _In_ HANDLE ProcessId);

BOOLEAN
RuleIsAllowWrite(
    _In_ PRULE_CLAW_TYPE ClawType,
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FILE_INFORMATION_CLASS FileInformationClass);
