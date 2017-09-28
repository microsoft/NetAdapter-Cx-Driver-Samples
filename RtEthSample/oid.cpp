/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "trace.h"
#include "adapter.h"
#include "device.h"
#include "oid.h"
#include "statistics.h"
#include "rxqueue.h"
#include "link.h"

_Requires_lock_held_(adapter->Lock)
void
RtAdapterSetOffloadParameters(
    _In_  RT_ADAPTER *adapter,
    _In_  NDIS_OFFLOAD_PARAMETERS *offloadParameters,
    _Out_ NDIS_OFFLOAD *offloadConfiguration
)
{
    switch (offloadParameters->IPv4Checksum)
    {
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
        adapter->IPChksumOffv4 = RtChecksumOffloadDisabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
        adapter->IPChksumOffv4 = RtChecksumOffloadTxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
        adapter->IPChksumOffv4 = RtChecksumOffloadRxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
        adapter->IPChksumOffv4 = RtChecksumOffloadTxRxEnabled;
        break;
    }

    switch (offloadParameters->TCPIPv4Checksum)
    {
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
        adapter->TCPChksumOffv4 = RtChecksumOffloadDisabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
        adapter->TCPChksumOffv4 = RtChecksumOffloadTxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
        adapter->TCPChksumOffv4 = RtChecksumOffloadRxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
        adapter->TCPChksumOffv4 = RtChecksumOffloadTxRxEnabled;
        break;
    }

    switch (offloadParameters->UDPIPv4Checksum)
    {
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
        adapter->UDPChksumOffv4 = RtChecksumOffloadDisabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
        adapter->UDPChksumOffv4 = RtChecksumOffloadTxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
        adapter->UDPChksumOffv4 = RtChecksumOffloadRxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
        adapter->UDPChksumOffv4 = RtChecksumOffloadTxRxEnabled;
        break;
    }

    switch (offloadParameters->TCPIPv6Checksum)
    {
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
        adapter->TCPChksumOffv6 = RtChecksumOffloadDisabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
        adapter->TCPChksumOffv6 = RtChecksumOffloadTxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
        adapter->TCPChksumOffv6 = RtChecksumOffloadRxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
        adapter->TCPChksumOffv6 = RtChecksumOffloadTxRxEnabled;
        break;
    }

    switch (offloadParameters->UDPIPv6Checksum)
    {
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
        adapter->UDPChksumOffv6 = RtChecksumOffloadDisabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
        adapter->UDPChksumOffv6 = RtChecksumOffloadTxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
        adapter->UDPChksumOffv6 = RtChecksumOffloadRxEnabled;
        break;
    case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
        adapter->UDPChksumOffv6 = RtChecksumOffloadTxRxEnabled;
        break;
    }

    RtAdapterUpdateEnabledChecksumOffloads(adapter);
    RtAdapterQueryOffloadConfiguration(adapter, offloadConfiguration);
}

#define RTK_NIC_GBE_PCIE_ADAPTER_NAME "Realtek PCIe GBE Family Controller"

// OID_GEN_VENDOR_DESCRIPTION
void
EvtNetRequestQueryVendorDescription(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST Request,
    _Out_writes_bytes_(OutputBufferLength)
    PVOID OutputBuffer,
    UINT OutputBufferLength)
{
    __analysis_assume(OutputBufferLength >= sizeof(RTK_NIC_GBE_PCIE_ADAPTER_NAME));

    UNREFERENCED_PARAMETER((RequestQueue, OutputBufferLength));

    TraceEntry();

    RtlCopyMemory(OutputBuffer, RTK_NIC_GBE_PCIE_ADAPTER_NAME, sizeof(RTK_NIC_GBE_PCIE_ADAPTER_NAME));

    NetRequestQueryDataComplete(Request, STATUS_SUCCESS, sizeof(RTK_NIC_GBE_PCIE_ADAPTER_NAME));

    TraceExit();
}

// OID_OFFLOAD_ENCAPSULATION
void
EvtNetRequestQueryOffloadEncapsulation(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST Request,
    _Out_writes_bytes_(OutputBufferLength)
    PVOID OutputBuffer,
    UINT OutputBufferLength)
{
    __analysis_assume(OutputBufferLength >= sizeof(NDIS_OFFLOAD_ENCAPSULATION));

    UNREFERENCED_PARAMETER((OutputBufferLength));

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter);

    RtlCopyMemory(OutputBuffer, &adapter->OffloadEncapsulation, sizeof(NDIS_OFFLOAD_ENCAPSULATION));

    NetRequestQueryDataComplete(Request, STATUS_SUCCESS, sizeof(NDIS_OFFLOAD_ENCAPSULATION));

    TraceExit();
}

void
EvtNetRequestQuerySuccess(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST Request,
    _Out_writes_bytes_(OutputBufferLength)
    PVOID OutputBuffer,
    UINT OutputBufferLength)
{
    UNREFERENCED_PARAMETER((RequestQueue, OutputBuffer, OutputBufferLength));

    TraceEntry();

    NetRequestCompleteWithoutInformation(Request, STATUS_SUCCESS);

    TraceExit();
}

void
EvtNetRequestQueryUlong(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST Request,
    _Out_writes_bytes_(OutputBufferLength)
    PVOID OutputBuffer,
    UINT OutputBufferLength)
{
    __analysis_assume(OutputBufferLength >= sizeof(ULONG));

    UNREFERENCED_PARAMETER((OutputBufferLength));

    NDIS_OID oid = NetRequestGetId(Request);

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter, TraceLoggingUInt32(oid));

    ULONG result = 0;

    switch (oid)
    {
    case OID_GEN_HARDWARE_STATUS:
        result = NdisHardwareStatusReady;
        break;

    case OID_GEN_VENDOR_ID:
        result = *(PULONG)(adapter->PermanentAddress.Address);
        break;

    case OID_GEN_MEDIA_SUPPORTED:
    case OID_GEN_MEDIA_IN_USE:
        result = NdisMedium802_3;
        break;

    case OID_GEN_PHYSICAL_MEDIUM_EX:
        result = NdisPhysicalMedium802_3;
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:
        // "Current Lookahead" is the number of bytes following the Ethernet header
        // that the NIC should indicate in the first NET_PACKET_FRAGMENT. Essentially
        // a Current Lookahead of 8 would mean that each indicated NET_PACKET's first
        // NET_PACKET_FRAGMENT would point to a buffer of at *least* size
        // ETH_LENGTH_OF_HEADER + 8.
        //
        // Since the RTL8168D *always* indicates all traffic in a single, contiguous buffer,
        // its driver just reports the maximum ethernet payload size as the current lookahead.
        __fallthrough;
    case OID_GEN_MAXIMUM_FRAME_SIZE:
        result = RT_MAX_PACKET_SIZE - ETH_LENGTH_OF_HEADER;
        break;

    case OID_GEN_MAXIMUM_TOTAL_SIZE:
    case OID_GEN_TRANSMIT_BLOCK_SIZE:
    case OID_GEN_RECEIVE_BLOCK_SIZE:
        result = (ULONG)RT_MAX_PACKET_SIZE;
        break;

    case OID_GEN_MAC_OPTIONS:
        result = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
            NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
            NDIS_MAC_OPTION_NO_LOOPBACK;
        break;

    case OID_GEN_TRANSMIT_BUFFER_SPACE:
        result = RT_MAX_PACKET_SIZE * adapter->NumTcb;
        break;

    case OID_GEN_RECEIVE_BUFFER_SPACE:
        if (adapter->RxQueue)
        {
            result = RT_MAX_PACKET_SIZE * NetRxQueueGetRingBuffer(adapter->RxQueue)->NumberOfElements;
        }
        else
        {
            result = 0;
        }
        break;

    case OID_GEN_VENDOR_DRIVER_VERSION:
        result = RT_VENDOR_DRIVER_VERSION;
        break;

    case OID_802_3_MAXIMUM_LIST_SIZE:
        result = RT_MAX_MCAST_LIST;
        break;

    default:
        NT_ASSERTMSG("Unexpected OID", false);
        break;
    }

    *(PULONG UNALIGNED)OutputBuffer = result;

    NetRequestQueryDataComplete(Request, STATUS_SUCCESS, sizeof(ULONG));

    TraceExit();
}

void
EvtNetRequestSetMulticastList(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST      Request,
    _In_reads_bytes_(InputBufferLength)
    PVOID                InputBuffer,
    UINT                 InputBufferLength)
{
    NDIS_OID oid = NetRequestGetId(Request);

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter, TraceLoggingUInt32(oid));

    if (InputBufferLength % ETH_LENGTH_OF_ADDRESS != 0)
    {
        NetRequestCompleteWithoutInformation(Request, NDIS_STATUS_INVALID_LENGTH);
        goto Exit;
    }

    ULONG mcAddressCount = InputBufferLength / ETH_LENGTH_OF_ADDRESS;

    NT_ASSERT(mcAddressCount <= RT_MAX_MCAST_LIST);

    if (mcAddressCount > RT_MAX_MCAST_LIST)
    {
        NetRequestCompleteWithoutInformation(Request, NDIS_STATUS_INVALID_DATA);
        goto Exit;
    }

    WdfSpinLockAcquire(adapter->Lock); {

        adapter->MCAddressCount = mcAddressCount;

        //
        // Save the MC list
        //
        RtlCopyMemory(
            adapter->MCList,
            InputBuffer,
            mcAddressCount * ETH_LENGTH_OF_ADDRESS);

        RtAdapterPushMulticastList(adapter);

    } WdfSpinLockRelease(adapter->Lock);

    NetRequestSetDataComplete(Request, STATUS_SUCCESS, mcAddressCount * ETH_LENGTH_OF_ADDRESS);

Exit:

    TraceExit();
}

void
EvtNetRequestSetPacketFilter(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST      Request,
    _In_reads_bytes_(InputBufferLength)
    PVOID                InputBuffer,
    UINT                 InputBufferLength)
{
    UNREFERENCED_PARAMETER((InputBufferLength));

    NDIS_OID oid = NetRequestGetId(Request);

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter, TraceLoggingUInt32(oid));

    NET_PACKET_FILTER_TYPES_FLAGS packetFilter =
        *(NET_PACKET_FILTER_TYPES_FLAGS UNALIGNED*)InputBuffer;

    if (packetFilter & ~RT_SUPPORTED_FILTERS)
    {
        NetRequestCompleteWithoutInformation(Request, STATUS_NOT_SUPPORTED);
        goto Exit;
    }

    WdfSpinLockAcquire(adapter->Lock); {

        adapter->PacketFilter = packetFilter;
        RtAdapterUpdateRcr(adapter);

        // Changing the packet filter might require clearing the active MCList
        RtAdapterPushMulticastList(adapter);

    } WdfSpinLockRelease(adapter->Lock);

    NetRequestSetDataComplete(Request, STATUS_SUCCESS, sizeof(ULONG));

Exit:

    TraceExit();
}

void
EvtNetRequestSetCurrentLookahead(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST      Request,
    _In_reads_bytes_(InputBufferLength)
    PVOID                InputBuffer,
    UINT                 InputBufferLength)
{
    UNREFERENCED_PARAMETER((InputBufferLength));

    NDIS_OID oid = NetRequestGetId(Request);

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter, TraceLoggingUInt32(oid));

    if (*(UNALIGNED PULONG) InputBuffer > (RT_MAX_PACKET_SIZE - ETH_LENGTH_OF_HEADER))
    {
        NetRequestCompleteWithoutInformation(Request, NDIS_STATUS_INVALID_DATA);
        goto Exit;
    }

    // the set "Current Lookahead" value is not used as the "Current Lookahead" for the
    // RTL8168D is *always* the maximum payload size.

    NetRequestSetDataComplete(Request, STATUS_SUCCESS, sizeof(ULONG));

Exit:

    TraceExit();
}

void
EvtNetRequestSetTcpOffloadParameters(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST      Request,
    _In_reads_bytes_(InputBufferLength)
    PVOID                InputBuffer,
    UINT                 InputBufferLength)
{
    UNREFERENCED_PARAMETER(InputBufferLength);

    NDIS_OID oid = NetRequestGetId(Request);

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter, TraceLoggingUInt32(oid));

    NDIS_OFFLOAD offloadConfiguration;
    WdfSpinLockAcquire(adapter->Lock); {

        RtAdapterSetOffloadParameters(adapter, (NDIS_OFFLOAD_PARAMETERS*)InputBuffer, &offloadConfiguration);

    } WdfSpinLockRelease(adapter->Lock);

    {
        NDIS_STATUS_INDICATION statusIndication;
        RtlZeroMemory(&statusIndication, sizeof(NDIS_STATUS_INDICATION));

        statusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
        statusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
        statusIndication.Header.Size = sizeof(NDIS_STATUS_INDICATION);

        statusIndication.SourceHandle = adapter->NdisLegacyAdapterHandle;
        statusIndication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
        statusIndication.StatusBuffer = &offloadConfiguration;
        statusIndication.StatusBufferSize = sizeof(offloadConfiguration);

        NdisMIndicateStatusEx(adapter->NdisLegacyAdapterHandle, &statusIndication);
    }

    NetRequestSetDataComplete(Request, STATUS_SUCCESS, sizeof(NDIS_OFFLOAD_PARAMETERS));

    TraceExit();
}

void
EvtNetRequestSetOffloadEncapsulation(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST      Request,
    _In_reads_bytes_(InputBufferLength)
    PVOID                InputBuffer,
    UINT                 InputBufferLength)
{
    UNREFERENCED_PARAMETER(InputBufferLength);

    NDIS_OID oid = NetRequestGetId(Request);

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter, TraceLoggingUInt32(oid));

    NDIS_OFFLOAD_ENCAPSULATION *setEncapsulation = (NDIS_OFFLOAD_ENCAPSULATION*)InputBuffer;

    adapter->OffloadEncapsulation.IPv4.Enabled           = setEncapsulation->IPv4.Enabled;
    adapter->OffloadEncapsulation.IPv4.EncapsulationType = setEncapsulation->IPv4.EncapsulationType;
    adapter->OffloadEncapsulation.IPv4.HeaderSize        = setEncapsulation->IPv4.HeaderSize;

    adapter->OffloadEncapsulation.IPv6.Enabled           = setEncapsulation->IPv6.Enabled;
    adapter->OffloadEncapsulation.IPv6.EncapsulationType = setEncapsulation->IPv6.EncapsulationType;
    adapter->OffloadEncapsulation.IPv6.HeaderSize        = setEncapsulation->IPv6.HeaderSize;

    NetRequestSetDataComplete(Request, STATUS_SUCCESS, sizeof(NDIS_OFFLOAD_ENCAPSULATION));

    TraceExit();
}

void
EvtNetRequestQueryInterruptModeration(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST Request,
    _Out_writes_bytes_(OutputBufferLength)
    PVOID OutputBuffer,
    UINT OutputBufferLength)
{
    UNREFERENCED_PARAMETER((OutputBufferLength));

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);
    NDIS_INTERRUPT_MODERATION_PARAMETERS *imParameters = (NDIS_INTERRUPT_MODERATION_PARAMETERS*)OutputBuffer;

    TraceEntryRtAdapter(adapter);

    RtlZeroMemory(imParameters, NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1);

    imParameters->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    imParameters->Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
    imParameters->Header.Size = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;

    if (adapter->InterruptModerationMode == RtInterruptModerationDisabled)
    {
        imParameters->InterruptModeration = NdisInterruptModerationNotSupported;
    }
    else if (adapter->InterruptModerationDisabled)
    {
        imParameters->InterruptModeration = NdisInterruptModerationDisabled;
    }
    else
    {
        imParameters->InterruptModeration = NdisInterruptModerationEnabled;
    }

    NetRequestQueryDataComplete(Request, STATUS_SUCCESS, NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1);

    TraceExit();
}

void
EvtNetRequestSetInterruptModeration(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST      Request,
    _In_reads_bytes_(InputBufferLength)
    PVOID                InputBuffer,
    UINT                 InputBufferLength)
{
    UNREFERENCED_PARAMETER((InputBufferLength));

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);
    NDIS_INTERRUPT_MODERATION_PARAMETERS *imParameters = (NDIS_INTERRUPT_MODERATION_PARAMETERS*)InputBuffer;

    TraceEntryRtAdapter(adapter);

    if (adapter->InterruptModerationMode == RtInterruptModerationDisabled)
    {
        NetRequestSetDataComplete(Request, NDIS_STATUS_INVALID_DATA, NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1);
    }
    else
    {
        switch (imParameters->InterruptModeration)
        {
        case NdisInterruptModerationEnabled:
            adapter->InterruptModerationDisabled = false;
            break;

        case NdisInterruptModerationDisabled:
            adapter->InterruptModerationDisabled = true;
            break;
        }

        RtAdapterUpdateInterruptModeration(adapter);

        NetRequestSetDataComplete(Request, STATUS_SUCCESS, NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1);
    }

    TraceExit();
}

typedef struct _RT_OID_QUERY {
    NDIS_OID Oid;
    PFN_NET_REQUEST_QUERY_DATA EvtQueryData;
    UINT MinimumSize;
} RT_OID_QUERY, *PRT_OID_QUERY;

const RT_OID_QUERY ComplexQueries[] = {
    { OID_GEN_STATISTICS,           EvtNetRequestQueryAllStatistics,        sizeof(NDIS_STATISTICS_INFO) },
    { OID_GEN_VENDOR_DESCRIPTION,   EvtNetRequestQueryVendorDescription,    sizeof(RTK_NIC_GBE_PCIE_ADAPTER_NAME) },
    { OID_OFFLOAD_ENCAPSULATION,    EvtNetRequestQueryOffloadEncapsulation, sizeof(NDIS_OFFLOAD_ENCAPSULATION) },
    { OID_PNP_QUERY_POWER,          EvtNetRequestQuerySuccess,              0 },
    { OID_GEN_INTERRUPT_MODERATION, EvtNetRequestQueryInterruptModeration,  NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1 },
};

typedef struct _RT_OID_SET {
    NDIS_OID Oid;
    PFN_NET_REQUEST_SET_DATA EvtSetData;
    UINT MinimumSize;
} RT_OID_SET, *PRT_OID_SET;

const RT_OID_SET ComplexSets[] = {
    { OID_802_3_MULTICAST_LIST,      EvtNetRequestSetMulticastList,        0 },
    { OID_GEN_CURRENT_PACKET_FILTER, EvtNetRequestSetPacketFilter,         sizeof(ULONG) },
    { OID_GEN_CURRENT_LOOKAHEAD,     EvtNetRequestSetCurrentLookahead,     sizeof(ULONG) },
    { OID_TCP_OFFLOAD_PARAMETERS,    EvtNetRequestSetTcpOffloadParameters, sizeof(NDIS_OFFLOAD_PARAMETERS) },
    { OID_OFFLOAD_ENCAPSULATION,     EvtNetRequestSetOffloadEncapsulation, sizeof(NDIS_OFFLOAD_ENCAPSULATION) },
    { OID_GEN_INTERRUPT_MODERATION,  EvtNetRequestSetInterruptModeration,  NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1 },
};

const NDIS_OID UlongQueries[] = {
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_VENDOR_ID,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_PHYSICAL_MEDIUM_EX,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_802_3_MAXIMUM_LIST_SIZE,
};

const NDIS_OID StatisticQueries[] = {
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_MAX_COLLISIONS,
    OID_802_3_XMIT_UNDERRUN,
};

NTSTATUS
RtInitializeAdapterRequestQueue(
    _In_ RT_ADAPTER *adapter)
{
    NTSTATUS status;
    NET_REQUEST_QUEUE_CONFIG queueConfig;

    TraceEntryRtAdapter(adapter);

    NET_REQUEST_QUEUE_CONFIG_INIT_DEFAULT_SEQUENTIAL(
        &queueConfig,
        adapter->NetAdapter);

    // registers those OIDs that can be completed as ULONGs
    for (ULONG i = 0; i < ARRAYSIZE(UlongQueries); i++)
    {
        NET_REQUEST_QUEUE_CONFIG_ADD_QUERY_DATA_HANDLER(
            &queueConfig,
            UlongQueries[i],
            EvtNetRequestQueryUlong,
            sizeof(ULONG));
    }

    // registers individual statistic OIDs
    for (ULONG i = 0; i < ARRAYSIZE(StatisticQueries); i++)
    {
        NET_REQUEST_QUEUE_CONFIG_ADD_QUERY_DATA_HANDLER(
            &queueConfig,
            StatisticQueries[i],
            EvtNetRequestQueryIndividualStatistics,
            sizeof(ULONG));
    }

    // registers query OIDs with complex behaviors
    for (ULONG i = 0; i < ARRAYSIZE(ComplexQueries); i++)
    {
        NET_REQUEST_QUEUE_CONFIG_ADD_QUERY_DATA_HANDLER(
            &queueConfig,
            ComplexQueries[i].Oid,
            ComplexQueries[i].EvtQueryData,
            ComplexQueries[i].MinimumSize);
    }

    // registers set OIDs with complex behaviors
    for (ULONG i = 0; i < ARRAYSIZE(ComplexSets); i++)
    {
        NET_REQUEST_QUEUE_CONFIG_ADD_SET_DATA_HANDLER(
            &queueConfig,
            ComplexSets[i].Oid,
            ComplexSets[i].EvtSetData,
            ComplexSets[i].MinimumSize);
    }

    //
    // Create the default NETREQUESTQUEUE.
    //
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetRequestQueueCreate(&queueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL));

Exit:
    TraceExitResult(status);
    return status;
}
