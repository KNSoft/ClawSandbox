#include "Driver.h"

static PFLT_FILTER CommunicationFilter;
static PFLT_PORT ServerPort;
static PFLT_PORT ClientPort;
static EX_PUSH_LOCK CommunicationLock;

static
NTSTATUS
FLTAPI
CommunicationConnectNotify(
    _In_ PFLT_PORT ClientPortHandle,
    _In_opt_ PVOID ServerPortCookie,
    _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Outptr_result_maybenull_ PVOID *ConnectionCookie)
{
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);

    FltAcquirePushLockExclusive(&CommunicationLock);
    ClientPort = ClientPortHandle;
    FltReleasePushLock(&CommunicationLock);

    *ConnectionCookie = NULL;
    return STATUS_SUCCESS;
}

static
VOID
FLTAPI
CommunicationDisconnectNotify(
    _In_opt_ PVOID ConnectionCookie)
{
    UNREFERENCED_PARAMETER(ConnectionCookie);

    FltAcquirePushLockExclusive(&CommunicationLock);
    if (ClientPort != NULL)
    {
        FltCloseClientPort(CommunicationFilter, &ClientPort);
    }
    FltReleasePushLock(&CommunicationLock);
}

static
NTSTATUS
CommunicationQueryTrackedProcessIds(
    _Out_writes_bytes_to_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength)
{
    PCLAWSANDBOX_TRACKED_PIDS_REPLY Reply;
    ULONG Capacity;
    ULONG Count;

    if (OutputBuffer == NULL ||
        ReturnOutputBufferLength == NULL ||
        OutputBufferLength < (ULONG)FIELD_OFFSET(CLAWSANDBOX_TRACKED_PIDS_REPLY, ProcessIds))
    {
        return STATUS_INVALID_PARAMETER;
    }

    Reply = (PCLAWSANDBOX_TRACKED_PIDS_REPLY)OutputBuffer;
    Capacity = (OutputBufferLength - (ULONG)FIELD_OFFSET(CLAWSANDBOX_TRACKED_PIDS_REPLY, ProcessIds)) / sizeof(ULONG_PTR);

    RtlZeroMemory(Reply, OutputBufferLength);
    RuleCopyTrackedProcessIds(Reply->ProcessIds, Capacity, &Count);
    Reply->Count = Count;
    Reply->Capacity = Capacity;

    *ReturnOutputBufferLength = (ULONG)FIELD_OFFSET(CLAWSANDBOX_TRACKED_PIDS_REPLY, ProcessIds) +
                                min(Count, Capacity) * sizeof(ULONG_PTR);
    return STATUS_SUCCESS;
}

static
NTSTATUS
FLTAPI
CommunicationMessageNotify(
    _In_opt_ PVOID PortCookie,
    _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength)
{
    PCLAWSANDBOX_MESSAGE_HEADER Header;

    UNREFERENCED_PARAMETER(PortCookie);

    if (ReturnOutputBufferLength != NULL)
    {
        *ReturnOutputBufferLength = 0;
    }
    if (InputBuffer == NULL ||
        InputBufferLength < sizeof(CLAWSANDBOX_MESSAGE_HEADER) ||
        OutputBuffer == NULL ||
        ReturnOutputBufferLength == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Header = (PCLAWSANDBOX_MESSAGE_HEADER)InputBuffer;
    if (Header->Version != CLAWSANDBOX_PROTOCOL_VERSION)
    {
        return STATUS_REVISION_MISMATCH;
    }

    switch (Header->Message)
    {
        case CLAWSANDBOX_MESSAGE_QUERY_TRACKED_PIDS:
            return CommunicationQueryTrackedProcessIds(OutputBuffer, OutputBufferLength, ReturnOutputBufferLength);
        default:
            return STATUS_INVALID_DEVICE_REQUEST;
    }
}

NTSTATUS
MainCreateCommunicationPort(
    _In_ PFLT_FILTER Filter)
{
    NTSTATUS Status;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    UNICODE_STRING PortName = RTL_CONSTANT_STRING(CLAWSANDBOX_PORT_NAME);
    OBJECT_ATTRIBUTES ObjectAttributes;

    if (Filter == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    FltInitializePushLock(&CommunicationLock);

    CommunicationFilter = Filter;
    SecurityDescriptor = NULL;
    Status = FltBuildDefaultSecurityDescriptor(&SecurityDescriptor, FLT_PORT_ALL_ACCESS);
    if (!NT_SUCCESS(Status))
    {
        CommunicationFilter = NULL;
        FltDeletePushLock(&CommunicationLock);
        return Status;
    }

    InitializeObjectAttributes(&ObjectAttributes,
                               &PortName,
                               OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL,
                               SecurityDescriptor);

    Status = FltCreateCommunicationPort(CommunicationFilter,
                                        &ServerPort,
                                        &ObjectAttributes,
                                        NULL,
                                        CommunicationConnectNotify,
                                        CommunicationDisconnectNotify,
                                        CommunicationMessageNotify,
                                        1);
    FltFreeSecurityDescriptor(SecurityDescriptor);
    if (!NT_SUCCESS(Status))
    {
        CommunicationFilter = NULL;
        FltDeletePushLock(&CommunicationLock);
    }
    return Status;
}

VOID
MainCloseCommunicationPort(VOID)
{
    FltAcquirePushLockExclusive(&CommunicationLock);
    if (ServerPort != NULL)
    {
        FltCloseCommunicationPort(ServerPort);
        ServerPort = NULL;
    }
    if (ClientPort != NULL)
    {
        FltCloseClientPort(CommunicationFilter, &ClientPort);
    }
    CommunicationFilter = NULL;
    FltReleasePushLock(&CommunicationLock);
    FltDeletePushLock(&CommunicationLock);
}
