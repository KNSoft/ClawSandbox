#pragma once

#include <fltKernel.h>
#include <ntstrsafe.h>

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

/* Rule.c */

NTSTATUS
RuleInitialize(
    _In_ PFLT_FILTER Filter,
    _In_ PCUNICODE_STRING RegistryPath);

VOID
RuleUninitialize(VOID);

BOOLEAN
RuleShouldTrackCreate(
    _In_ PPS_CREATE_NOTIFY_INFO CreateInfo);

BOOLEAN
RuleIsTrackedProcess(
    _In_ HANDLE ProcessId);

NTSTATUS
RuleTrackProcess(
    _In_ HANDLE ProcessId);

BOOLEAN
RuleUntrackProcess(
    _In_ HANDLE ProcessId);

BOOLEAN
RuleIsAllowWrite(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FILE_INFORMATION_CLASS FileInformationClass);

