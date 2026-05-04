#include "Driver.h"

#pragma region Process track

typedef struct _TRACKED_PROCESS_ENTRY
{
    LIST_ENTRY Link;
    HANDLE ProcessId;
    PRULE_CLAW_TYPE ClawType;
} TRACKED_PROCESS_ENTRY, *PTRACKED_PROCESS_ENTRY;

static EX_PUSH_LOCK TrackedProcessLock;
static LIST_ENTRY TrackedProcessList;

static
PTRACKED_PROCESS_ENTRY
RuleFindTrackedProcessEntryLocked(
    _In_ HANDLE ProcessId)
{
    PLIST_ENTRY Link;

    for (Link = TrackedProcessList.Flink; Link != &TrackedProcessList; Link = Link->Flink)
    {
        PTRACKED_PROCESS_ENTRY Entry;

        Entry = CONTAINING_RECORD(Link, TRACKED_PROCESS_ENTRY, Link);
        if (Entry->ProcessId == ProcessId)
        {
            return Entry;
        }
    }

    return NULL;
}

_Ret_maybenull_
PRULE_CLAW_TYPE
RuleGetTrackedProcessClawType(
    _In_ HANDLE ProcessId)
{
    PTRACKED_PROCESS_ENTRY Entry;
    PRULE_CLAW_TYPE ClawType;

    ClawType = NULL;

    FltAcquirePushLockShared(&TrackedProcessLock);
    Entry = RuleFindTrackedProcessEntryLocked(ProcessId);
    if (Entry != NULL)
    {
        ClawType = Entry->ClawType;
    }
    FltReleasePushLock(&TrackedProcessLock);

    return ClawType;
}

BOOLEAN
RuleHasTrackedProcess(VOID)
{
    BOOLEAN HasTrackedProcess;

    FltAcquirePushLockShared(&TrackedProcessLock);
    HasTrackedProcess = !IsListEmpty(&TrackedProcessList);
    FltReleasePushLock(&TrackedProcessLock);

    return HasTrackedProcess;
}

NTSTATUS
RuleTrackProcess(
    _In_ HANDLE ProcessId,
    _In_ PRULE_CLAW_TYPE ClawType)
{
    PTRACKED_PROCESS_ENTRY Entry;

    if (ClawType == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Entry = (PTRACKED_PROCESS_ENTRY)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(TRACKED_PROCESS_ENTRY), CLAWSANDBOX_TAG);
    if (Entry == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Entry, sizeof(*Entry));
    Entry->ProcessId = ProcessId;
    Entry->ClawType = ClawType;

    FltAcquirePushLockExclusive(&TrackedProcessLock);
    {
        PTRACKED_PROCESS_ENTRY ExistingEntry;

        ExistingEntry = RuleFindTrackedProcessEntryLocked(ProcessId);
        if (ExistingEntry != NULL)
        {
            ExistingEntry->ClawType = ClawType;
            FltReleasePushLock(&TrackedProcessLock);
            ExFreePoolWithTag(Entry, CLAWSANDBOX_TAG);
            return STATUS_SUCCESS;
        }

        InsertTailList(&TrackedProcessList, &Entry->Link);
    }
    FltReleasePushLock(&TrackedProcessLock);

    return STATUS_SUCCESS;
}

BOOLEAN
RuleUntrackProcess(
    _In_ HANDLE ProcessId)
{
    PTRACKED_PROCESS_ENTRY Entry;

    FltAcquirePushLockExclusive(&TrackedProcessLock);
    {
        Entry = RuleFindTrackedProcessEntryLocked(ProcessId);
        if (Entry != NULL)
        {
            RemoveEntryList(&Entry->Link);
            FltReleasePushLock(&TrackedProcessLock);
            ExFreePoolWithTag(Entry, CLAWSANDBOX_TAG);
            return TRUE;
        }
    }
    FltReleasePushLock(&TrackedProcessLock);

    return FALSE;
}

#pragma endregion

static UNICODE_STRING SystemRoot = RTL_CONSTANT_STRING(L"\\SystemRoot");
static OBJECT_ATTRIBUTES Attributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(&SystemRoot, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE);
static CONST UNICODE_STRING ParametersSubKey = RTL_CONSTANT_STRING(L"\\Parameters");
static CONST UNICODE_STRING FsWhiteListValueName = RTL_CONSTANT_STRING(L"FSWhiteList");
static PWCHAR FsWhiteListBuffer = NULL;
static ULONG FsWhiteListBufferLength;

static
VOID
RuleFreeFsWhiteList(VOID)
{
    if (FsWhiteListBuffer != NULL)
    {
        ExFreePoolWithTag(FsWhiteListBuffer, CLAWSANDBOX_TAG);
        FsWhiteListBuffer = NULL;
        FsWhiteListBufferLength = 0;
    }
}

static
NTSTATUS
RuleLoadFsWhiteList(
    _In_ PCUNICODE_STRING RegistryPath,
    _Outptr_result_maybenull_ PWCHAR* Buffer,
    _Out_ PULONG BufferLength)
{
    NTSTATUS Status;
    ULONG ParametersPathMaximumLength;
    UNICODE_STRING ParametersPath;
    PWCHAR ParametersPathBuffer;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE KeyHandle;
    ULONG ResultLength;
    PKEY_VALUE_PARTIAL_INFORMATION ValueInformation;
    PWCHAR ValueBuffer;

    if (RegistryPath == NULL || RegistryPath->Buffer == NULL || Buffer == NULL || BufferLength == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *Buffer = NULL;
    *BufferLength = 0;

    if ((ULONG)RegistryPath->Length + (ULONG)ParametersSubKey.Length + (ULONG)sizeof(WCHAR) > MAXUSHORT)
    {
        return STATUS_NAME_TOO_LONG;
    }

    ParametersPathMaximumLength = (ULONG)RegistryPath->Length + (ULONG)ParametersSubKey.Length + (ULONG)sizeof(WCHAR);
    ParametersPathBuffer = (PWCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED,
                                                   ParametersPathMaximumLength,
                                                   CLAWSANDBOX_TAG);
    if (ParametersPathBuffer == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ParametersPath.Buffer = ParametersPathBuffer;
    ParametersPath.Length = 0;
    ParametersPath.MaximumLength = (USHORT)ParametersPathMaximumLength;

    Status = RtlAppendUnicodeStringToString(&ParametersPath, RegistryPath);
    if (NT_SUCCESS(Status))
    {
        Status = RtlAppendUnicodeStringToString(&ParametersPath, &ParametersSubKey);
    }
    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(ParametersPathBuffer, CLAWSANDBOX_TAG);
        return Status;
    }

    InitializeObjectAttributes(&ObjectAttributes,
                               &ParametersPath,
                               OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    KeyHandle = NULL;
    Status = ZwOpenKey(&KeyHandle, KEY_QUERY_VALUE, &ObjectAttributes);
    ExFreePoolWithTag(ParametersPathBuffer, CLAWSANDBOX_TAG);
    if (Status == STATUS_OBJECT_NAME_NOT_FOUND || Status == STATUS_OBJECT_PATH_NOT_FOUND)
    {
        return STATUS_NOT_FOUND;
    }
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    ResultLength = 0;
    Status = ZwQueryValueKey(KeyHandle,
                             (PUNICODE_STRING)&FsWhiteListValueName,
                             KeyValuePartialInformation,
                             NULL,
                             0,
                             &ResultLength);
    if (Status != STATUS_BUFFER_OVERFLOW && Status != STATUS_BUFFER_TOO_SMALL)
    {
        ZwClose(KeyHandle);
        if (NT_SUCCESS(Status))
        {
            Status = STATUS_UNEXPECTED_IO_ERROR;
        }
        return Status;
    }

    ValueInformation = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_NON_PAGED,
                                                                       ResultLength,
                                                                       CLAWSANDBOX_TAG);
    if (ValueInformation == NULL)
    {
        ZwClose(KeyHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = ZwQueryValueKey(KeyHandle,
                             (PUNICODE_STRING)&FsWhiteListValueName,
                             KeyValuePartialInformation,
                             ValueInformation,
                             ResultLength,
                             &ResultLength);
    ZwClose(KeyHandle);
    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(ValueInformation, CLAWSANDBOX_TAG);
        return Status;
    }

    if (ValueInformation->Type != REG_MULTI_SZ || (ValueInformation->DataLength % sizeof(WCHAR)) != 0)
    {
        ExFreePoolWithTag(ValueInformation, CLAWSANDBOX_TAG);
        return STATUS_INVALID_PARAMETER;
    }

    ValueBuffer = (PWCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED,
                                          ValueInformation->DataLength + sizeof(WCHAR),
                                          CLAWSANDBOX_TAG);
    if (ValueBuffer == NULL)
    {
        ExFreePoolWithTag(ValueInformation, CLAWSANDBOX_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(ValueBuffer, ValueInformation->Data, ValueInformation->DataLength);
    ValueBuffer[ValueInformation->DataLength / sizeof(WCHAR)] = UNICODE_NULL;
    *Buffer = ValueBuffer;
    *BufferLength = ValueInformation->DataLength + sizeof(WCHAR);

    ExFreePoolWithTag(ValueInformation, CLAWSANDBOX_TAG);
    return STATUS_SUCCESS;
}

static
BOOLEAN
RuleIsAllowByFsWhiteList(
    _In_ PCUNICODE_STRING Path)
{
    PWCHAR EntryBuffer;
    ULONG RemainingCch;

    if (Path == NULL || FsWhiteListBuffer == NULL || FsWhiteListBufferLength < sizeof(WCHAR))
    {
        return FALSE;
    }

    EntryBuffer = FsWhiteListBuffer;
    RemainingCch = FsWhiteListBufferLength / sizeof(WCHAR);
    while (RemainingCch > 1 && *EntryBuffer != UNICODE_NULL)
    {
        UNICODE_STRING Entry;
        USHORT EntryCch;

        EntryCch = 0;
        while ((ULONG)EntryCch < RemainingCch && EntryBuffer[EntryCch] != UNICODE_NULL)
        {
            EntryCch++;
        }

        if (EntryCch != 0)
        {
            Entry.Buffer = EntryBuffer;
            Entry.Length = EntryCch * sizeof(WCHAR);
            Entry.MaximumLength = Entry.Length;
            if (PathContainsComponentInsensitive(Path, &Entry))
            {
                return TRUE;
            }
        }

        if ((ULONG)EntryCch >= RemainingCch)
        {
            break;
        }

        EntryBuffer += EntryCch + 1;
        RemainingCch -= EntryCch + 1;
    }

    return FALSE;
}

static
NTSTATUS
RuleInitSystemVolume(
    _In_ PFLT_FILTER Filter,
    _Out_ PFLT_VOLUME* SystemVolume)
{
    NTSTATUS Status;
    HANDLE FileHandle;
    IO_STATUS_BLOCK iosb;
    PFILE_OBJECT FileObject;

    Status = ZwCreateFile(&FileHandle,
                          FILE_READ_ATTRIBUTES,
                          &Attributes,
                          &iosb,
                          NULL,
                          FILE_ATTRIBUTE_NORMAL,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          FILE_OPEN,
                          FILE_DIRECTORY_FILE,
                          NULL,
                          0);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }
    Status = ObReferenceObjectByHandle(FileHandle,
                                       FILE_READ_ATTRIBUTES,
                                       *IoFileObjectType,
                                       KernelMode,
                                       (PVOID*)&FileObject,
                                       NULL);
    ZwClose(FileHandle);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = FltGetVolumeFromFileObject(Filter, FileObject, SystemVolume);
    ObDereferenceObject(FileObject);
    return Status;
}

static PFLT_VOLUME FltSystemVolume;

NTSTATUS
RuleInitialize(
    _In_ PFLT_FILTER Filter,
    _In_ PCUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;

    FltInitializePushLock(&TrackedProcessLock);
    InitializeListHead(&TrackedProcessList);

    Status = RuleInitSystemVolume(Filter, &FltSystemVolume);
    if (!NT_SUCCESS(Status))
    {
        FltDeletePushLock(&TrackedProcessLock);
        return Status;
    }

    Status = RuleLoadFsWhiteList(RegistryPath, &FsWhiteListBuffer, &FsWhiteListBufferLength);
    UNREFERENCED_PARAMETER(Status);

    return STATUS_SUCCESS;
}

VOID
RuleUninitialize(VOID)
{
    LIST_ENTRY LocalTrackedProcessList;
    PLIST_ENTRY Link;
    PTRACKED_PROCESS_ENTRY Entry;

    InitializeListHead(&LocalTrackedProcessList);

    FltAcquirePushLockExclusive(&TrackedProcessLock);
    if (!IsListEmpty(&TrackedProcessList))
    {
        LocalTrackedProcessList = TrackedProcessList;
        LocalTrackedProcessList.Flink->Blink = &LocalTrackedProcessList;
        LocalTrackedProcessList.Blink->Flink = &LocalTrackedProcessList;
        InitializeListHead(&TrackedProcessList);
    }
    FltReleasePushLock(&TrackedProcessLock);

    while (!IsListEmpty(&LocalTrackedProcessList))
    {
        Link = RemoveHeadList(&LocalTrackedProcessList);
        Entry = CONTAINING_RECORD(Link, TRACKED_PROCESS_ENTRY, Link);
        ExFreePoolWithTag(Entry, CLAWSANDBOX_TAG);
    }

    if (FltSystemVolume != NULL)
    {
        FltObjectDereference(FltSystemVolume);
        FltSystemVolume = NULL;
    }

    RuleFreeFsWhiteList();
    FltDeletePushLock(&TrackedProcessLock);
}

static
PCSTR
GetOperationName(
    _In_ UCHAR MajorFunction)
{
    if (MajorFunction == IRP_MJ_CREATE)
    {
        return "IRP_MJ_CREATE";
    } else if (MajorFunction == IRP_MJ_WRITE)
    {
        return "IRP_MJ_WRITE";
    } else if (MajorFunction == IRP_MJ_SET_INFORMATION)
    {
        return "IRP_MJ_SET_INFORMATION";
    } else
    {
        return "Unknown";
    }
}

static CONST UNICODE_STRING TempDirPart = RTL_CONSTANT_STRING(L"Temp");
static CONST UNICODE_STRING TmpDirPart = RTL_CONSTANT_STRING(L"tmp");
static CONST UNICODE_STRING WindowsCachesPathPart = RTL_CONSTANT_STRING(L"\\AppData\\Local\\Microsoft\\Windows\\Caches\\");

static
BOOLEAN
RuleIsAllowWritePath(
    _In_ PRULE_CLAW_TYPE ClawType,
    _In_ PCUNICODE_STRING Path,
    _In_ PCFLT_RELATED_OBJECTS FltObjects)
{
    if (ClawType->FileWriteCallback(Path, FltObjects))
    {
        return TRUE;
    }
    if (FltObjects->Volume == FltSystemVolume &&
        (PathContainsComponentInsensitive(Path, &TempDirPart) ||
         PathContainsComponentInsensitive(Path, &TmpDirPart) ||
         StringContainsInSensetive(Path, &WindowsCachesPathPart)))
    {
        return TRUE;
    }
    if (RuleIsAllowByFsWhiteList(Path))
    {
        return TRUE;
    }
    return FALSE;
}

static
NTSTATUS
RuleGetDestinationNameInformation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FILE_INFORMATION_CLASS FileInformationClass,
    _Outptr_ PFLT_FILE_NAME_INFORMATION* DestinationNameInfo)
{
    HANDLE RootDirectory;
    PWSTR FileName;
    ULONG FileNameLength;

    if (Data->Iopb->Parameters.SetFileInformation.InfoBuffer == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (FileInformationClass == FileRenameInformation ||
        FileInformationClass == FileRenameInformationEx)
    {
        PFILE_RENAME_INFORMATION RenameInfo;

        RenameInfo = (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
        RootDirectory = RenameInfo->RootDirectory;
        FileName = RenameInfo->FileName;
        FileNameLength = RenameInfo->FileNameLength;
    } else if (FileInformationClass == FileLinkInformation ||
               FileInformationClass == FileLinkInformationEx)
    {
        PFILE_LINK_INFORMATION LinkInfo;

        LinkInfo = (PFILE_LINK_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
        RootDirectory = LinkInfo->RootDirectory;
        FileName = LinkInfo->FileName;
        FileNameLength = LinkInfo->FileNameLength;
    } else
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (FileNameLength != 0 && FileName == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    return FltGetDestinationFileNameInformation(FltObjects->Instance,
                                                FltObjects->FileObject,
                                                RootDirectory,
                                                FileName,
                                                FileNameLength,
                                                FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
                                                DestinationNameInfo);
}

BOOLEAN
RuleIsAllowWrite(
    _In_ PRULE_CLAW_TYPE ClawType,
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FILE_INFORMATION_CLASS FileInformationClass)
{
    BOOLEAN CheckDestinationName;
    NTSTATUS Status;
    PFLT_FILE_NAME_INFORMATION NameInfo;
    PFLT_FILE_NAME_INFORMATION DestinationNameInfo;
    PCWSTR ClawName;
    BOOLEAN IsAllow;

    CheckDestinationName = (FileInformationClass == FileRenameInformation ||
                            FileInformationClass == FileRenameInformationEx ||
                            FileInformationClass == FileLinkInformation ||
                            FileInformationClass == FileLinkInformationEx);
    NameInfo = NULL;
    DestinationNameInfo = NULL;
    ClawName = ClawType->Name;

    Status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &NameInfo);
    if (!NT_SUCCESS(Status))
    {
        return !CheckDestinationName;
    }

    IsAllow = RuleIsAllowWritePath(ClawType, &NameInfo->Name, FltObjects);
    if (!IsAllow)
    {
        LOG(DPFLTR_TRACE_LEVEL,
            "[BLOCK] Claw=%ws %hs PID=%lu path=%wZ\n",
            ClawName,
            GetOperationName(Data->Iopb->MajorFunction),
            FltGetRequestorProcessId(Data),
            &NameInfo->Name);
        goto _Exit;
    }

    if (CheckDestinationName)
    {
        Status = RuleGetDestinationNameInformation(Data, FltObjects, FileInformationClass, &DestinationNameInfo);
        if (!NT_SUCCESS(Status))
        {
            LOG(DPFLTR_TRACE_LEVEL,
                "[BLOCK] Claw=%ws %hs PID=%lu source=%wZ destination-name-lookup=0x%08lX\n",
                ClawName,
                GetOperationName(Data->Iopb->MajorFunction),
                FltGetRequestorProcessId(Data),
                &NameInfo->Name,
                Status);
            IsAllow = FALSE;
            goto _Exit;
        }

        IsAllow = RuleIsAllowWritePath(ClawType, &DestinationNameInfo->Name, FltObjects);
        if (!IsAllow)
        {
            LOG(DPFLTR_TRACE_LEVEL,
                "[BLOCK] Claw=%ws %hs PID=%lu source=%wZ destination=%wZ\n",
                ClawName,
                GetOperationName(Data->Iopb->MajorFunction),
                FltGetRequestorProcessId(Data),
                &NameInfo->Name,
                &DestinationNameInfo->Name);
        }
    }

_Exit:
    if (DestinationNameInfo != NULL)
    {
        FltReleaseFileNameInformation(DestinationNameInfo);
    }
    FltReleaseFileNameInformation(NameInfo);
    return IsAllow;
}
