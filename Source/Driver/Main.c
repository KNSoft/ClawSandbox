#include "Driver.h"

#pragma region Access Control

static CONST UNICODE_STRING EmptyImageName = RTL_CONSTANT_STRING(L"");

static
BOOLEAN
MainCreateRequestsWriteAccess(
    _In_ PFLT_CALLBACK_DATA Data)
{
    ACCESS_MASK DesiredAccess;
    ULONG CreateDisposition, CreateOptions;

    DesiredAccess = Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess;
    CreateDisposition = (Data->Iopb->Parameters.Create.Options >> 24) & 0xFF;
    CreateOptions = Data->Iopb->Parameters.Create.Options & 0x00FFFFFF;

    if ((DesiredAccess & (FILE_WRITE_DATA |
                          FILE_APPEND_DATA |
                          FILE_WRITE_EA |
                          FILE_WRITE_ATTRIBUTES |
                          DELETE |
                          WRITE_DAC |
                          WRITE_OWNER)) != 0)
    {
        return TRUE;
    } else if (CreateDisposition == FILE_SUPERSEDE ||
               CreateDisposition == FILE_CREATE ||
               CreateDisposition == FILE_OVERWRITE ||
               CreateDisposition == FILE_OVERWRITE_IF)
    {
        return TRUE;
    } else if (CreateOptions & FILE_DELETE_ON_CLOSE)
    {
        return TRUE;
    }

    return FALSE;
}

static
FLT_PREOP_CALLBACK_STATUS
MainFilterFilePreOp(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FILE_INFORMATION_CLASS FileInformationClass)
{
    PRULE_CLAW_TYPE ClawType;

    ClawType = RuleGetTrackedProcessClawType((HANDLE)(ULONG_PTR)FltGetRequestorProcessId(Data));
    if (ClawType == NULL ||
        RuleIsAllowWrite(ClawType, Data, FltObjects, FileInformationClass))
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    Data->IoStatus.Status = STATUS_LPAC_ACCESS_DENIED; // STATUS_ACCESS_DENIED;
    Data->IoStatus.Information = 0;
    return FLT_PREOP_COMPLETE;
}

static
FLT_PREOP_CALLBACK_STATUS
MainPreCreate(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
    UNREFERENCED_PARAMETER(CompletionContext);
    return MainCreateRequestsWriteAccess(Data) ?
        MainFilterFilePreOp(Data, FltObjects, 0) :
        FLT_PREOP_SUCCESS_NO_CALLBACK;
}

static
FLT_PREOP_CALLBACK_STATUS
MainPreWrite(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(CompletionContext);
    return MainFilterFilePreOp(Data, FltObjects, 0);
}

FLT_PREOP_CALLBACK_STATUS
FLTAPI
MainPreSetInformation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Outptr_result_maybenull_ PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(CompletionContext);
    FILE_INFORMATION_CLASS fic = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;

    if (fic != FileBasicInformation &&
        fic != FileDispositionInformation &&
        fic != FileDispositionInformationEx &&
        fic != FileEndOfFileInformation &&
        fic != FileAllocationInformation &&
        fic != FileRenameInformation &&
        fic != FileRenameInformationEx &&
        fic != FileLinkInformation &&
        fic != FileLinkInformationEx &&
        fic != FileValidDataLengthInformation)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    return MainFilterFilePreOp(Data, FltObjects, fic);
}

#pragma endregion

#pragma region Process track

VOID
MainProcessNotifyEx(
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    UNREFERENCED_PARAMETER(Process);

    HANDLE PPID;
    NTSTATUS Status;
    PRULE_CLAW_TYPE ClawType;
    PCUNICODE_STRING ImageName;

    /* Untrack when the process terminating */
    if (CreateInfo == NULL)
    {
        if (RuleUntrackProcess(ProcessId))
        {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_INFO_LEVEL,
                       LOG_PREFIX "Stop tracking process PID=%lu\n",
                       (ULONG)(ULONG_PTR)ProcessId);
        }
        return;
    }

    /* Bypass the process? */
    PPID = CreateInfo->ParentProcessId;
    ClawType = RuleGetTrackedProcessClawType(PPID);
    if (ClawType == NULL)
    {
        ClawType = RuleListMatchClawTypeCreate(CreateInfo);
    }
    if (ClawType == NULL)
    {
        return;
    }
    ImageName = CreateInfo->ImageFileName != NULL ?
        CreateInfo->ImageFileName :
        &EmptyImageName;

    /* Track the process */
    Status = RuleTrackProcess(ProcessId, ClawType);
    if (NT_SUCCESS(Status))
    {
        LOG(DPFLTR_INFO_LEVEL,
            "Start tracking process PID=%lu PPID=%lu Claw=%ws ImageName=%wZ\n",
            (ULONG)(ULONG_PTR)ProcessId,
            (ULONG)(ULONG_PTR)PPID,
            ClawType->Name,
            ImageName);
    } else
    {
        LOG(DPFLTR_INFO_LEVEL,
            "Failed (0x%08lX) to track process PID=%lu PPID=%lu Claw=%ws ImageName=%wZ\n",
            Status,
            (ULONG)(ULONG_PTR)ProcessId,
            (ULONG)(ULONG_PTR)PPID,
            ClawType->Name,
            ImageName);
    }
}

#pragma endregion

#pragma region Startup and shutdown

static PFLT_FILTER FilterHandle;

static
VOID
MainTrackExistingProcesses(VOID)
{
    HANDLE ProcessHandle;
    HANDLE NextProcessHandle;
    PEPROCESS Process;
    PUNICODE_STRING ImageName;
    PRULE_CLAW_TYPE ClawType;
    NTSTATUS Status;
    HANDLE ProcessId;

    ProcessHandle = NULL;
    while (TRUE)
    {
        NextProcessHandle = NULL;
        Status = ZwGetNextProcess(ProcessHandle,
                                  PROCESS_QUERY_LIMITED_INFORMATION,
                                  OBJ_KERNEL_HANDLE,
                                  0,
                                  &NextProcessHandle);
        if (ProcessHandle != NULL)
        {
            ZwClose(ProcessHandle);
        }

        ProcessHandle = NextProcessHandle;
        if (!NT_SUCCESS(Status))
        {
            break;
        }

        Process = NULL;
        Status = ObReferenceObjectByHandle(ProcessHandle,
                                           PROCESS_QUERY_LIMITED_INFORMATION,
                                           *PsProcessType,
                                           KernelMode,
                                           (PVOID*)&Process,
                                           NULL);
        if (!NT_SUCCESS(Status))
        {
            continue;
        }

        ImageName = NULL;
        Status = SeLocateProcessImageName(Process, &ImageName);
        if (!NT_SUCCESS(Status) || ImageName == NULL)
        {
            ObDereferenceObject(Process);
            continue;
        }

    ClawType = RuleListMatchClawTypeImageName(ImageName);
    if (ClawType != NULL)
    {
        ProcessId = PsGetProcessId(Process);
        if (PsGetProcessExitStatus(Process) != STATUS_PENDING)
        {
            Status = STATUS_PROCESS_IS_TERMINATING;
        } else
        {
            Status = RuleTrackProcess(ProcessId, ClawType);
            if (NT_SUCCESS(Status) && PsGetProcessExitStatus(Process) != STATUS_PENDING)
            {
                RuleUntrackProcess(ProcessId);
                Status = STATUS_PROCESS_IS_TERMINATING;
            }
            if (NT_SUCCESS(Status))
            {
                LOG(DPFLTR_INFO_LEVEL,
                    "Start tracking existing process PID=%lu Claw=%ws ImageName=%wZ\n",
                    (ULONG)(ULONG_PTR)ProcessId,
                    ClawType->Name,
                    ImageName);
            }
        }
        if (!NT_SUCCESS(Status))
        {
            LOG(DPFLTR_INFO_LEVEL,
                "Failed (0x%08lX) to track existing process PID=%lu Claw=%ws ImageName=%wZ\n",
                Status,
                (ULONG)(ULONG_PTR)ProcessId,
                ClawType->Name,
                ImageName);
        }
    }

        ExFreePool(ImageName);
        ObDereferenceObject(Process);
    }
}

static
NTSTATUS
FLTAPI
MainFilterUnload(
    FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);
    NTSTATUS Status;

    if (RuleIsSelfProtectionEnabled() && RuleHasTrackedProcess())
    {
        return STATUS_FLT_DO_NOT_DETACH;
    }

    Status = PsSetCreateProcessNotifyRoutineEx(MainProcessNotifyEx, TRUE);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }
    MainCloseCommunicationPort();
    FltUnregisterFilter(FilterHandle);
    RuleUninitialize();

    return STATUS_SUCCESS;
}

static
NTSTATUS
FLTAPI
MainInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);

    return STATUS_SUCCESS;
}

static CONST FLT_OPERATION_REGISTRATION OperationCallbacks[] = {
    { IRP_MJ_CREATE, 0, MainPreCreate, NULL },
    { IRP_MJ_WRITE, 0, MainPreWrite, NULL },
    { IRP_MJ_SET_INFORMATION, 0, MainPreSetInformation, NULL },
    { IRP_MJ_OPERATION_END }
};

static CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    FLTFL_REGISTRATION_DO_NOT_SUPPORT_SERVICE_STOP,
    NULL,
    OperationCallbacks,
    MainFilterUnload,
    MainInstanceSetup,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

DRIVER_INITIALIZE DriverEntry;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;

    Status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
    if (NT_SUCCESS(Status))
    {
        Status = RuleInitialize(FilterHandle, RegistryPath);
        if (NT_SUCCESS(Status))
        {
            Status = MainCreateCommunicationPort(FilterHandle);
            if (NT_SUCCESS(Status))
            {
                Status = PsSetCreateProcessNotifyRoutineEx(MainProcessNotifyEx, FALSE);
                if (NT_SUCCESS(Status))
                {
                    MainTrackExistingProcesses();
                    Status = FltStartFiltering(FilterHandle);
                    if (NT_SUCCESS(Status))
                    {
                        return STATUS_SUCCESS;
                    }
                    PsSetCreateProcessNotifyRoutineEx(MainProcessNotifyEx, TRUE);
                }
                MainCloseCommunicationPort();
            }
            RuleUninitialize();
        }
        FltUnregisterFilter(FilterHandle);
    }

    return Status;
}

#pragma endregion
