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
#include "stats.h"
#include "rxqueue.h"

#if DBG
#define _FILENUMBER     'QERM'
#endif

ULONG VendorDriverVersion = NIC_VENDOR_DRIVER_VERSION;

const NDIS_OID NICSupportedOids[] =
{
    //query
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_GEN_PHYSICAL_MEDIUM_EX,

    // stats
    OID_GEN_STATISTICS,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_MAX_COLLISIONS,
    OID_802_3_XMIT_UNDERRUN,

    // query + set
    OID_GEN_CURRENT_LOOKAHEAD,

    // set
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_802_3_MULTICAST_LIST,
    OID_TCP_OFFLOAD_PARAMETERS,
};

void
NICSetPacketFilter(
    IN PMP_ADAPTER Adapter,
    IN ULONG PacketFilter
)
/*++
Routine Description:

    This routine will set up the adapter so that it accepts packets
    that match the specified packet filter.  The only filter bits
    that can truly be toggled are for broadcast and promiscuous

Arguments:

    Adapter         Pointer to our adapter
    PacketFilter    The new packet filter

Return Value:

    Nothing.

--*/
{
    ULONG RcrReg;

    DBGPRINT(MP_TRACE, ("--> NICSetPacketFilter, PacketFilter=%08x\n", PacketFilter));

    NICSetMulticastList(Adapter);

    RcrReg = Adapter->CSRAddress->RCR;

    if (PacketFilter & (NDIS_PACKET_TYPE_ALL_MULTICAST |
                        NDIS_PACKET_TYPE_MULTICAST |
                        NDIS_PACKET_TYPE_PROMISCUOUS))
    {
        RcrReg |= (RCR_AM | RCR_AAP);
    }
    else
    {
        RcrReg &= ~RCR_AM;
    }

    if (PacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS)
    {
        RcrReg |= (RCR_AAP | RCR_AR | RCR_AER);
    }
    else
    {
        RcrReg &= ~(RCR_AAP | RCR_AR | RCR_AER);
    }

    if (PacketFilter &
            (NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_PROMISCUOUS))
    {
        RcrReg |= RCR_AB;
    }
    else
    {
        RcrReg &= ~RCR_AB;
    }

    if (PacketFilter &
            (NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_PROMISCUOUS))
    {
        RcrReg |= RCR_APM;
    }
    else
    {
        RcrReg &= ~RCR_APM;
    }

    //
    // write to RCR register
    //
    Adapter->CSRAddress->RCR = RcrReg;
}

VOID NICSetMulticastReg(
    IN PMP_ADAPTER Adapter,
    IN ULONG MultiCast_03,
    IN ULONG MultiCast_47
)
{
    PULONG pMultiCastReg;

    pMultiCastReg = (PULONG) (&Adapter->CSRAddress->MulticastReg0);

    *pMultiCastReg = MultiCast_03;
    pMultiCastReg = (PULONG) (&Adapter->CSRAddress->MulticastReg4);

    *pMultiCastReg = MultiCast_47;
}

void NICSetMulticastList(
    IN PMP_ADAPTER Adapter
)
/*++
Routine Description:

This routine will set up the adapter for a specified multicast address list

Arguments:

Adapter     Pointer to our adapter

Return Value:

NDIS_STATUS_SUCCESS
NDIS_STATUS_NOT_ACCEPTED

--*/
{
    UCHAR Byte;
    UCHAR  Bit;
    ULONG MultiCast_03;
    ULONG MultiCast_47;
    ULONG  AddressCount = Adapter->MCAddressCount;
    ULONG  MultiCastR1;
    ULONG  MultiCastR2;

    UCHAR  NicMulticastRegs[MAX_NIC_MULTICAST_REG] = {0};

    // Now turn on the bit for each address in the multicast list.
    for (UINT i = 0; (i < AddressCount) && (i < MAX_MULTICAST_ADDRESSES); i++)
    {
        DBGPRINT(MP_INFO, (" %d-th multicast address :%02x:%02x:%02x:%02x:%02x:%02x ", i, Adapter->MCList[i][0], Adapter->MCList[i][1], Adapter->MCList[i][2], Adapter->MCList[i][3], Adapter->MCList[i][4], Adapter->MCList[i][5]));
        GetMulticastBit(Adapter->MCList[i], &Byte, &Bit);
        DBGPRINT(MP_INFO, (" % byte:%d /bit:%d\n ", Byte, Bit));
        NicMulticastRegs[Byte] |= Bit;
    }

    //Write Multicast bits to register
    MultiCast_03 = 0;

    MultiCast_47 = 0;

    for (UINT i = 0; i < 4; i++)
    {
        MultiCast_03 = MultiCast_03 + (NicMulticastRegs[i] << (8 * i));
    }

    for (UINT i = 4; i < 8; i++)
    {
        MultiCast_47 = MultiCast_47 + (NicMulticastRegs[i] << (8 * (i - 4)));
    }


    DBGPRINT(MP_INFO, ("Multicast %x - %x \n", MultiCast_03, MultiCast_47));

    MultiCastR1 = 0;
    MultiCastR2 = 0;

    MultiCastR1 |= (MultiCast_47 & 0x000000ff) << 24;
    MultiCastR1 |= (MultiCast_47 & 0x0000ff00) << 8;
    MultiCastR1 |= (MultiCast_47 & 0x00ff0000) >> 8;
    MultiCastR1 |= (MultiCast_47 & 0xff000000) >> 24;

    MultiCastR2 |= (MultiCast_03 & 0x000000ff) << 24;
    MultiCastR2 |= (MultiCast_03 & 0x0000ff00) << 8;
    MultiCastR2 |= (MultiCast_03 & 0x00ff0000) >> 8;
    MultiCastR2 |= (MultiCast_03 & 0xff000000) >> 24;

    MultiCast_03 = MultiCastR1;
    MultiCast_47 = MultiCastR2;
    DBGPRINT(MP_INFO, ("8168 Revise Multicast %x - %x \n", MultiCast_03, MultiCast_47));

    NICSetMulticastReg(Adapter, MultiCast_03, MultiCast_47);

    DBGPRINT(MP_INFO, ("Set multicast registers to :%08x %08x\n", MultiCast_03, MultiCast_47));
}

VOID
MPSetPowerD0(
    _In_ PMP_ADAPTER Adapter
)
/*++
Routine Description:

This routine is called when the device receives a 
EvtDeviceD0Entry.

Arguments:

Adapter                 Pointer to the adapter structure

Return Value:

--*/
{
    //
    // MPSetPowerD0Private initializes the adapter, issues a reset
    //
    MPSetPowerD0PrivateRealtek(Adapter);
    
    // Set up the multicast list address
    // return to D0, WOL no more require to RX all multicast packets
    NICSetMulticastList(Adapter);

    // Update link state
    NET_ADAPTER_LINK_STATE linkState;

    RtUpdateMediaState(Adapter);
    RtGetLinkState(Adapter, &linkState);

    NetAdapterSetCurrentLinkState(Adapter->Adapter, &linkState);
}


VOID
MPSetPowerLow(
    _In_ PMP_ADAPTER Adapter
    )
/*++
Routine Description:

This routine is called when the device receives a 
EvtDeviceD0Exit callback

Arguments:

Adapter                 Pointer to the adapter structure
PowerState              NewPowerState

Return Value:

--*/
{
    NET_ADAPTER_LINK_STATE linkState;

    WdfSpinLockAcquire(Adapter->Lock); {

        Adapter->MediaState = MediaConnectStateUnknown;
        Adapter->MediaDuplexState = MediaDuplexStateUnknown;
        Adapter->LinkSpeed = NDIS_LINK_SPEED_UNKNOWN;

        NET_ADAPTER_LINK_STATE_INIT(
            &linkState, Adapter->LinkSpeed, Adapter->MediaState,
            Adapter->MediaDuplexState, NetAdapterPauseFunctionsUnknown,
            NET_ADAPTER_AUTO_NEGOTIATION_NO_FLAGS);

    } WdfSpinLockRelease(Adapter->Lock);

    NetAdapterSetCurrentLinkState(Adapter->Adapter, &linkState);

    // acknowledge interrupt
    USHORT isr = Adapter->CSRAddress->ISR;
    Adapter->CSRAddress->ISR = isr;
}

_Requires_lock_held_(adapter->Lock)
void
RtAdapterSetOffloadParameters(
    _In_  MP_ADAPTER *adapter,
    _In_  NDIS_OFFLOAD_PARAMETERS *offloadParameters,
    _Out_ NDIS_OFFLOAD *offloadConfiguration
)
{
    if (offloadParameters->Header.Revision == NDIS_OFFLOAD_REVISION_1
        || offloadParameters->Header.Revision == NDIS_OFFLOAD_REVISION_2
        || offloadParameters->Header.Revision == NDIS_OFFLOAD_REVISION_3
        )
    {
        switch (offloadParameters->IPv4Checksum)
        {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            adapter->IPChksumOffv4 = CHKSUM_OFFLOAD_DISABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            adapter->IPChksumOffv4 = CHKSUM_OFFLOAD_TX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            adapter->IPChksumOffv4 = CHKSUM_OFFLOAD_RX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            adapter->IPChksumOffv4 = CHKSUM_OFFLOAD_TX_RX_ENABLED;
            break;
        }

        switch (offloadParameters->TCPIPv4Checksum)
        {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            adapter->TCPChksumOffv4 = CHKSUM_OFFLOAD_DISABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            adapter->TCPChksumOffv4 = CHKSUM_OFFLOAD_TX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            adapter->TCPChksumOffv4 = CHKSUM_OFFLOAD_RX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            adapter->TCPChksumOffv4 = CHKSUM_OFFLOAD_TX_RX_ENABLED;
            break;
        }

        switch (offloadParameters->UDPIPv4Checksum)
        {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            adapter->UDPChksumOffv4 = CHKSUM_OFFLOAD_DISABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            adapter->UDPChksumOffv4 = CHKSUM_OFFLOAD_TX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            adapter->UDPChksumOffv4 = CHKSUM_OFFLOAD_RX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            adapter->UDPChksumOffv4 = CHKSUM_OFFLOAD_TX_RX_ENABLED;
            break;
        }

        switch (offloadParameters->TCPIPv6Checksum)
        {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            adapter->TCPChksumOffv6 = CHKSUM_OFFLOAD_DISABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            adapter->TCPChksumOffv6 = CHKSUM_OFFLOAD_TX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            adapter->TCPChksumOffv6 = CHKSUM_OFFLOAD_RX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            adapter->TCPChksumOffv6 = CHKSUM_OFFLOAD_TX_RX_ENABLED;
            break;
        }

        switch (offloadParameters->UDPIPv6Checksum)
        {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            adapter->UDPChksumOffv6 = CHKSUM_OFFLOAD_DISABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            adapter->UDPChksumOffv6 = CHKSUM_OFFLOAD_TX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            adapter->UDPChksumOffv6 = CHKSUM_OFFLOAD_RX_ENABLED;
            break;
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            adapter->UDPChksumOffv6 = CHKSUM_OFFLOAD_TX_RX_ENABLED;
            break;
        }
    }

    RtAdapterUpdateEnabledChecksumOffloads(adapter);
    RtAdapterQueryOffloadConfiguration(adapter, offloadConfiguration);
}

void
EvtNetRequestQuerySupportedOids(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST Request,
    _Out_writes_bytes_(OutputBufferLength)
    PVOID OutputBuffer,
    UINT OutputBufferLength)
{
    __analysis_assume(OutputBufferLength >= sizeof(NICSupportedOids));

    UNREFERENCED_PARAMETER((RequestQueue, OutputBufferLength));

    TraceEntry();

    RtlCopyMemory(OutputBuffer, NICSupportedOids, sizeof(NICSupportedOids));

    NetRequestQueryDataComplete(Request, STATUS_SUCCESS, sizeof(NICSupportedOids));

    TraceExit();
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
EvtNetRequestQueryUshort(
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
    PMP_ADAPTER rtAdapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(rtAdapter, TraceLoggingUInt32(oid));

    USHORT result = 0;

    switch (oid)
    {
    case OID_GEN_DRIVER_VERSION:
        result = NIC_DRIVER_VERSION;
        break;

    default:
        NT_ASSERTMSG("Unexpected OID", false);
        break;
    }

    *(PUSHORT UNALIGNED)OutputBuffer = result;

    NetRequestQueryDataComplete(Request, STATUS_SUCCESS, sizeof(USHORT));

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
    PMP_ADAPTER rtAdapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(rtAdapter, TraceLoggingUInt32(oid));

    ULONG result = 0;

    switch (oid)
    {
    case OID_GEN_HARDWARE_STATUS:
        result = NdisHardwareStatusReady;
        break;

    case OID_GEN_VENDOR_ID:
        result = *(PULONG)rtAdapter->PermanentAddress;
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
        // NIC_HEADER_SIZE + 8.
        //
        // Since the RTL8168D *always* indicates all traffic in a single, contiguous buffer,
        // its driver just reports the maximum ethernet payload size as the current lookahead.
        __fallthrough;
    case OID_GEN_MAXIMUM_FRAME_SIZE:
        result = NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE;
        break;

    case OID_GEN_MAXIMUM_TOTAL_SIZE:
    case OID_GEN_TRANSMIT_BLOCK_SIZE:
    case OID_GEN_RECEIVE_BLOCK_SIZE:
        result = (ULONG)NIC_MAX_PACKET_SIZE;
        break;

    case OID_GEN_MAC_OPTIONS:
        result = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
            NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
            NDIS_MAC_OPTION_NO_LOOPBACK;
        break;

    case OID_GEN_TRANSMIT_BUFFER_SPACE:
        result = NIC_MAX_PACKET_SIZE * rtAdapter->NumTcb;
        break;

    case OID_GEN_RECEIVE_BUFFER_SPACE:
        if (rtAdapter->RxQueue)
        {
            result = NIC_MAX_PACKET_SIZE * NetRxQueueGetRingBuffer(rtAdapter->RxQueue)->NumberOfElements;
        }
        else
        {
            result = 0;
        }
        break;

    case OID_GEN_VENDOR_DRIVER_VERSION:
        result = VendorDriverVersion;
        break;

    case OID_802_3_MAXIMUM_LIST_SIZE:
        result = NIC_MAX_MCAST_LIST;
        break;

    case OID_GEN_MAXIMUM_SEND_PACKETS:
        result = NIC_MAX_SEND_PACKETS;
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
EvtNetRequestQueryIndividualStatistics(
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
    PMP_ADAPTER adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter, TraceLoggingUInt32(oid, "Oid"));

    switch (oid)
    {
    case OID_GEN_XMIT_OK:
        // Purely software counter
        NOTHING;
        break;
    default:
        // Dependent on counters from hardware
        RtAdapterDumpStatsCounters(adapter);
        break;
    }

    ULONG64 result = 0;
    switch (oid)
    {
    case OID_GEN_XMIT_OK:
        result = adapter->OutUCastPkts + adapter->OutMulticastPkts + adapter->OutBroadcastPkts;
        break;

    case OID_GEN_RCV_OK:
        result = adapter->HwTotalRxMatchPhy + adapter->HwTotalRxMulticast + adapter->HwTotalRxBroadcast;
        break;

    case OID_802_3_XMIT_ONE_COLLISION:
        result = adapter->TxOneRetry;
        break;

    case OID_802_3_XMIT_MORE_COLLISIONS:
        result = adapter->TxMoreThanOneRetry;
        break;

    case OID_802_3_XMIT_MAX_COLLISIONS:
        result = adapter->TxAbortExcessCollisions;
        break;

    case OID_802_3_XMIT_UNDERRUN:
        result = adapter->TxDmaUnderrun;
        break;

    default:
        NT_ASSERTMSG("Unexpected OID", false);
        break;
    }


    // The NETREQUESTQUEUE ensures that OutputBufferLength is at least sizeof(ULONG)
    NT_ASSERT(OutputBufferLength >= sizeof(ULONG));

    if (OutputBufferLength < sizeof(ULONG64))
    {
        *(PULONG UNALIGNED)OutputBuffer = (ULONG)result;
        NetRequestQueryDataComplete(Request, STATUS_SUCCESS, sizeof(ULONG));
    }
    else
    {
        *(PULONG64 UNALIGNED)OutputBuffer = result;
        NetRequestQueryDataComplete(Request, STATUS_SUCCESS, sizeof(ULONG64));
    }


    TraceExit();
}

void
EvtNetRequestQueryAllStatistics(
    _In_ NETREQUESTQUEUE RequestQueue,
    _In_ NETREQUEST Request,
    _Out_writes_bytes_(OutputBufferLength)
    PVOID OutputBuffer,
    UINT OutputBufferLength)
{
    __analysis_assume(OutputBufferLength >= sizeof(NDIS_STATISTICS_INFO));

    UNREFERENCED_PARAMETER((OutputBufferLength));

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    PMP_ADAPTER adapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(adapter);

    RtAdapterDumpStatsCounters(adapter);

    NDIS_STATISTICS_INFO *statisticsInfo = (NDIS_STATISTICS_INFO *)OutputBuffer;

    statisticsInfo->Header.Revision = NDIS_OBJECT_REVISION_1;
    statisticsInfo->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    statisticsInfo->Header.Size = sizeof(NDIS_STATISTICS_INFO);

    statisticsInfo->SupportedStatistics
        =
            NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR             |
            NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS          |

            NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV   |
            NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV  |
            NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV  |

            NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV    |
            NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV   |
            NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV   |
            NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV             |

            NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR            |
            NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS         |

            NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT  |
            NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT |
            NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT |

            NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT   |
            NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT  |
            NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT  |
            NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT;

    // Rx statistics
    statisticsInfo->ifInErrors = adapter->TotalRxErr;
    statisticsInfo->ifInDiscards = adapter->TotalRxErr + adapter->RcvResourceErrors;

    statisticsInfo->ifHCInUcastPkts = adapter->HwTotalRxMatchPhy;
    statisticsInfo->ifHCInMulticastPkts = adapter->HwTotalRxMulticast;
    statisticsInfo->ifHCInBroadcastPkts = adapter->HwTotalRxBroadcast;

    statisticsInfo->ifHCInUcastOctets = adapter->InUcastOctets;
    statisticsInfo->ifHCInMulticastOctets = adapter->InMulticastOctets;
    statisticsInfo->ifHCInBroadcastOctets = adapter->InBroadcastOctets;
    statisticsInfo->ifHCInOctets = adapter->InBroadcastOctets + adapter->InMulticastOctets + adapter->InUcastOctets;

    // Tx statistics
    statisticsInfo->ifOutErrors = adapter->TotalTxErr;
    statisticsInfo->ifOutDiscards = adapter->TxAbortExcessCollisions;

    statisticsInfo->ifHCOutUcastPkts = adapter->OutUCastPkts;
    statisticsInfo->ifHCOutMulticastPkts = adapter->OutMulticastPkts;
    statisticsInfo->ifHCOutBroadcastPkts = adapter->OutBroadcastPkts;

    statisticsInfo->ifHCOutUcastOctets = adapter->OutUCastOctets;
    statisticsInfo->ifHCOutMulticastOctets = adapter->OutMulticastOctets;
    statisticsInfo->ifHCOutBroadcastOctets = adapter->OutBroadcastOctets;
    statisticsInfo->ifHCOutOctets = adapter->OutMulticastOctets + adapter->OutBroadcastOctets + adapter->OutUCastOctets;

    NetRequestQueryDataComplete(Request, STATUS_SUCCESS, sizeof(NDIS_STATISTICS_INFO));

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
    PMP_ADAPTER rtAdapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(rtAdapter, TraceLoggingUInt32(oid));

    if (InputBufferLength % ETH_LENGTH_OF_ADDRESS != 0)
    {
        NetRequestCompleteWithoutInformation(Request, NDIS_STATUS_INVALID_LENGTH);
        goto Exit;
    }

    ULONG mcAddressCount = InputBufferLength / ETH_LENGTH_OF_ADDRESS;

    NT_ASSERT(mcAddressCount <= NIC_MAX_MCAST_LIST);

    if (mcAddressCount > NIC_MAX_MCAST_LIST)
    {
        NetRequestCompleteWithoutInformation(Request, NDIS_STATUS_INVALID_DATA);
        goto Exit;
    }

    WdfSpinLockAcquire(rtAdapter->Lock); {

        rtAdapter->MCAddressCount = mcAddressCount;

        //
        // Save the MC list
        //
        NdisMoveMemory(
            rtAdapter->MCList,
            InputBuffer,
            mcAddressCount * ETH_LENGTH_OF_ADDRESS);

    } WdfSpinLockRelease(rtAdapter->Lock);

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
    PMP_ADAPTER rtAdapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(rtAdapter, TraceLoggingUInt32(oid));

    ULONG PacketFilter = *(PULONG UNALIGNED)InputBuffer;

    if (PacketFilter & ~NIC_SUPPORTED_FILTERS)
    {
        NetRequestCompleteWithoutInformation(Request, STATUS_NOT_SUPPORTED);
        goto Exit;
    }

    WdfSpinLockAcquire(rtAdapter->Lock); {

        rtAdapter->PacketFilter = PacketFilter;
        RtAdapterUpdateRcr(rtAdapter);

    } WdfSpinLockRelease(rtAdapter->Lock);

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
    PMP_ADAPTER rtAdapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(rtAdapter, TraceLoggingUInt32(oid));

    if (*(UNALIGNED PULONG) InputBuffer > (NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE))
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
    NDIS_OID oid = NetRequestGetId(Request);

    NETADAPTER netAdapter = NetRequestQueueGetAdapter(RequestQueue);
    PMP_ADAPTER rtAdapter = RtGetAdapterContext(netAdapter);

    TraceEntryRtAdapter(rtAdapter, TraceLoggingUInt32(oid));

    NDIS_OFFLOAD offloadConfiguration;
    WdfSpinLockAcquire(rtAdapter->Lock); {

        RtAdapterSetOffloadParameters(rtAdapter, (PNDIS_OFFLOAD_PARAMETERS)InputBuffer, &offloadConfiguration);

    } WdfSpinLockRelease(rtAdapter->Lock);

    {
        NDIS_STATUS_INDICATION statusIndication;
        RtlZeroMemory(&statusIndication, sizeof(NDIS_STATUS_INDICATION));

        statusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
        statusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
        statusIndication.Header.Size = sizeof(NDIS_STATUS_INDICATION);

        statusIndication.SourceHandle = rtAdapter->NdisLegacyAdapterHandle;
        statusIndication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
        statusIndication.StatusBuffer = &offloadConfiguration;
        statusIndication.StatusBufferSize = sizeof(offloadConfiguration);

        NdisMIndicateStatusEx(rtAdapter->NdisLegacyAdapterHandle, &statusIndication);
    }

    NetRequestSetDataComplete(Request, STATUS_SUCCESS, InputBufferLength);

    TraceExit();
}

typedef struct _RT_OID_QUERY {
    NDIS_OID Oid;
    PFN_NET_REQUEST_QUERY_DATA EvtQueryData;
    UINT MinimumSize;
} RT_OID_QUERY, *PRT_OID_QUERY;

const RT_OID_QUERY ComplexQueries[] = {
    { OID_GEN_SUPPORTED_LIST,       EvtNetRequestQuerySupportedOids,     sizeof(NICSupportedOids) },
    { OID_GEN_STATISTICS,           EvtNetRequestQueryAllStatistics,     sizeof(NDIS_STATISTICS_INFO) },
    { OID_GEN_VENDOR_DESCRIPTION,   EvtNetRequestQueryVendorDescription, sizeof(RTK_NIC_GBE_PCIE_ADAPTER_NAME) },
    { OID_PNP_QUERY_POWER,          EvtNetRequestQuerySuccess,           0 },
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
    { OID_TCP_OFFLOAD_PARAMETERS,    EvtNetRequestSetTcpOffloadParameters, NDIS_SIZEOF_OFFLOAD_PARAMETERS_REVISION_3 }
};

const NDIS_OID UshortQueries[] = {
    OID_GEN_DRIVER_VERSION,
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
    OID_GEN_MAXIMUM_SEND_PACKETS,
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
    _In_ MP_ADAPTER *rtAdapter)
{
    NTSTATUS status;
    NET_REQUEST_QUEUE_CONFIG queueConfig;

    TraceEntryRtAdapter(rtAdapter);

    NET_REQUEST_QUEUE_CONFIG_INIT_DEFAULT_SEQUENTIAL(
        &queueConfig,
        rtAdapter->Adapter);

    // registers those OIDs that can be completed as USHORTs
    for (ULONG i = 0; i < ARRAYSIZE(UshortQueries); i++)
    {
        NET_REQUEST_QUEUE_CONFIG_ADD_QUERY_DATA_HANDLER(
            &queueConfig,
            UshortQueries[i],
            EvtNetRequestQueryUshort,
            sizeof(USHORT));
    }

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
