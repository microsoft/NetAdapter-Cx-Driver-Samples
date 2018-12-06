/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "device.h"
#include "rxqueue.h"
#include "trace.h"
#include "adapter.h"
#include "interrupt.h"
#include "gigamac.h"

void
RtUpdateRecvStats(
    _In_ RT_RXQUEUE *rx,
    _In_ RT_RX_DESC const *rxd,
         ULONG length
    )
{
    // Unlike for Tx, the hardware has counters for Broadcast, Multicast,
    // and Unicast inbound packets. So, we defer to the hardware counter for
    // # of Rx transmissions.

    if (rxd->RxDescDataIpv6Rss.status & RXS_BAR)
    {
        rx->Adapter->InBroadcastOctets += length;
    }
    else if (rxd->RxDescDataIpv6Rss.status & RXS_MAR)
    {
        rx->Adapter->InMulticastOctets += length;
    }
    else
    {
        rx->Adapter->InUcastOctets += length;
    }
}

static
void
RxFillRtl8111DChecksumInfo(
    _In_    RT_RXQUEUE const *rx,
    _In_    RT_RX_DESC const *rxd,
    _Inout_ NET_PACKET *packet
    )
{
    RT_ADAPTER* adapter = rx->Adapter;
    NET_PACKET_CHECKSUM* checksumInfo =
        NetPacketGetPacketChecksum(
            packet,
            rx->ChecksumExtensionOffSet);

    packet->Layout.Layer2Type = NET_PACKET_LAYER2_TYPE_ETHERNET;
    checksumInfo->Layer2 =
        (rxd->RxDescDataIpv6Rss.status & RXS_CRC)
        ? NET_PACKET_RX_CHECKSUM_INVALID
        : NET_PACKET_RX_CHECKSUM_VALID;

    USHORT const isIpv4 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV4;
    USHORT const isIpv6 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV6;

    NT_ASSERT(!(isIpv4 && isIpv6));

    if (isIpv4)
    {
        packet->Layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS;

        if (adapter->IpHwChkSum)
        {
            checksumInfo->Layer3 =
                (rxd->RxDescDataIpv6Rss.status & RXS_IPF)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
    else if (isIpv6)
    {
        packet->Layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS;
    }
    else
    {
        return;
    }

    USHORT const isTcp = rxd->RxDescDataIpv6Rss.status & RXS_TCPIP_PACKET;
    USHORT const isUdp = rxd->RxDescDataIpv6Rss.status & RXS_UDPIP_PACKET;

    NT_ASSERT(!(isTcp && isUdp));

    if (isTcp)
    {
        packet->Layout.Layer4Type = NET_PACKET_LAYER4_TYPE_TCP;

        if (adapter->TcpHwChkSum)
        {
            checksumInfo->Layer4 =
                (rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_TCPF)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
    else if (isUdp)
    {
        packet->Layout.Layer4Type = NET_PACKET_LAYER4_TYPE_UDP;

        if (adapter->UdpHwChkSum)
        {
            checksumInfo->Layer4 =
                (rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_UDPF)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
}

static
void
RxFillRtl8111EChecksumInfo(
    _In_    RT_RXQUEUE const *rx,
    _In_    RT_RX_DESC const *rxd,
    _Inout_ NET_PACKET *packet
    )
{
    RT_ADAPTER* adapter = rx->Adapter;
    NET_PACKET_CHECKSUM* checksumInfo =
        NetPacketGetPacketChecksum(
            packet,
            rx->ChecksumExtensionOffSet);

    packet->Layout.Layer2Type = NET_PACKET_LAYER2_TYPE_ETHERNET;
    checksumInfo->Layer2 =
        (rxd->RxDescDataIpv6Rss.status & RXS_CRC)
        ? NET_PACKET_RX_CHECKSUM_INVALID
        : NET_PACKET_RX_CHECKSUM_VALID;

    USHORT const isIpv4 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV4;
    USHORT const isIpv6 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV6;

    NT_ASSERT(!(isIpv4 && isIpv6));

    if (isIpv4)
    {
        packet->Layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS;

        if (adapter->IpHwChkSum)
        {
            checksumInfo->Layer3 =
                (rxd->RxDescDataIpv6Rss.status & RXS_IPF)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
    else if (isIpv6)
    {
        packet->Layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS;
    }
    else
    {
        return;
    }

    USHORT const isTcp = rxd->RxDescDataIpv6Rss.status & RXS_TCPIP_PACKET;
    USHORT const isUdp = rxd->RxDescDataIpv6Rss.status & RXS_UDPIP_PACKET;

    NT_ASSERT(!(isTcp && isUdp));

    if (isTcp)
    {
        packet->Layout.Layer4Type = NET_PACKET_LAYER4_TYPE_TCP;

        if (adapter->TcpHwChkSum)
        {
            checksumInfo->Layer4 =
                (rxd->RxDescDataIpv6Rss.TcpUdpFailure & TXS_TCPCS)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
    else if (isUdp)
    {
        packet->Layout.Layer4Type = NET_PACKET_LAYER4_TYPE_UDP;

        if (adapter->UdpHwChkSum)
        {
            checksumInfo->Layer4 =
                (rxd->RxDescDataIpv6Rss.TcpUdpFailure & TXS_UDPCS)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
}

static
void
RtFillRxChecksumInfo(
    _In_    RT_RXQUEUE const *rx,
    _In_    RT_RX_DESC const *rxd,
    _Inout_ NET_PACKET *packet
    )
{
    switch (rx->Adapter->ChipType)
    {
    case RTL8168D:
        RxFillRtl8111DChecksumInfo(rx, rxd, packet);
        break;
    case RTL8168D_REV_C_REV_D:
    case RTL8168E:
        RxFillRtl8111EChecksumInfo(rx, rxd, packet);
        break;
    }
}

void
RxIndicateReceives(
    _In_ RT_RXQUEUE *rx
    )
{
    PCNET_DATAPATH_DESCRIPTOR descriptor = rx->DatapathDescriptor;
    NET_RING_BUFFER *rb = NET_DATAPATH_DESCRIPTOR_GET_PACKET_RING_BUFFER(descriptor);

    UINT32 i;

    for (i = rb->BeginIndex; i != rb->NextIndex; i = NetRingBufferIncrementIndex(rb, i))
    {
        RT_RX_DESC const *rxd = &rx->RxdBase[i];
        NET_PACKET *packet = NetRingBufferGetPacketAtIndex(descriptor, i);

        if (0 != (rxd->RxDescDataIpv6Rss.status & RXS_OWN))
            break;

        auto fragment = NET_PACKET_GET_FRAGMENT(packet, descriptor, 0);
        fragment->ValidLength = rxd->RxDescDataIpv6Rss.length - FRAME_CRC_SIZE;
        fragment->Offset = 0;

        NT_FRE_ASSERT(packet->FragmentCount == 1);

        if (rx->ChecksumExtensionOffSet != NET_PACKET_EXTENSION_INVALID_OFFSET)
        {
            // fill packetTcpChecksum
            RtFillRxChecksumInfo(rx, rxd, packet);
        }

        RtUpdateRecvStats(rx, rxd, fragment->ValidLength);
    }

    rb->BeginIndex = i;
}

void
RxPostBuffers(
    _In_ RT_RXQUEUE *rx
    )
{
    PCNET_DATAPATH_DESCRIPTOR descriptor = rx->DatapathDescriptor;
    NET_RING_BUFFER *rb = NET_DATAPATH_DESCRIPTOR_GET_PACKET_RING_BUFFER(descriptor);

    UINT32 initialIndex = rb->NextIndex;

    while (true)
    {
        UINT32 index = rb->NextIndex;
        RT_RX_DESC *rxd = &rx->RxdBase[index];

        NET_PACKET *packet = NetRingBufferAdvanceNextPacket(descriptor);
        if (!packet)
            break;

        auto fragment = NET_PACKET_GET_FRAGMENT(packet, descriptor, 0);
        rxd->BufferAddress = fragment->Mapping.DmaLogicalAddress;
        rxd->RxDescDataIpv6Rss.TcpUdpFailure = 0;
        rxd->RxDescDataIpv6Rss.length = fragment->Capacity;
        rxd->RxDescDataIpv6Rss.VLAN_TAG.Value = 0;

        MemoryBarrier();

        rxd->RxDescDataIpv6Rss.status = RXS_OWN | ((rb->NextIndex == 0) ? RXS_EOR : 0);
    }

    if (initialIndex != rb->NextIndex)
    {
        // jtippet: here's where to ring the doorbell to inform HW that more RXDs were posted.
        // But I can't find any doorbell in the old code.
    }
}

NTSTATUS
RtRxQueueInitialize(
    _In_ NETPACKETQUEUE rxQueue,
    _In_ RT_ADAPTER *adapter
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);

    rx->Adapter = adapter;
    rx->Interrupt = adapter->Interrupt;
    rx->DatapathDescriptor = NetRxQueueGetDatapathDescriptor(rxQueue);

    // allocate descriptors
    auto descriptor = NET_DATAPATH_DESCRIPTOR_GET_PACKET_RING_BUFFER(rx->DatapathDescriptor);
    auto const rxdSize = descriptor->NumberOfElements * sizeof(RT_RX_DESC);
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfCommonBufferCreate(
            rx->Adapter->DmaEnabler,
            rxdSize,
            WDF_NO_OBJECT_ATTRIBUTES,
            &rx->RxdArray));

    rx->RxdBase = static_cast<RT_RX_DESC*>(WdfCommonBufferGetAlignedVirtualAddress(rx->RxdArray));
    rx->RxdSize = rxdSize;

Exit:
    return status;
}

ULONG
RtConvertPacketFilterToRcr(
    _In_ NET_PACKET_FILTER_TYPES_FLAGS packetFilter
    )
{
    if (packetFilter & NET_PACKET_FILTER_TYPE_PROMISCUOUS)
    {
        return (RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_AR | RCR_AER);
    }

    return
        ((packetFilter & NET_PACKET_FILTER_TYPE_ALL_MULTICAST) ? RCR_AM  : 0) |
        ((packetFilter & NET_PACKET_FILTER_TYPE_MULTICAST)     ? RCR_AM  : 0) |
        ((packetFilter & NET_PACKET_FILTER_TYPE_BROADCAST)     ? RCR_AB  : 0) |
        ((packetFilter & NET_PACKET_FILTER_TYPE_DIRECTED)      ? RCR_APM : 0);
}

_Use_decl_annotations_
void
RtAdapterUpdateRcr(
    _In_ RT_ADAPTER *adapter
    )
{
    adapter->CSRAddress->RCR =
        (TCR_RCR_MXDMA_UNLIMITED << RCR_MXDMA_OFFSET) |
            RtConvertPacketFilterToRcr(adapter->PacketFilter);
}

void
RtRxQueueSetInterrupt(
    _In_ RT_RXQUEUE *rx,
    _In_ BOOLEAN notificationEnabled
    )
{
    InterlockedExchange(&rx->Interrupt->RxNotifyArmed[rx->QueueId], notificationEnabled);
    RtUpdateImr(rx->Interrupt, rx->QueueId);

    if (!notificationEnabled)
        // block this thread until we're sure any outstanding DPCs are complete.
        // This is to guarantee we don't return from this function call until
        // any oustanding rx notification is complete.
        KeFlushQueuedDpcs();
}

_Use_decl_annotations_
void
EvtRxQueueStart(
    NETPACKETQUEUE rxQueue
    )
{
    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);
    RT_ADAPTER *adapter = rx->Adapter;

    RtlZeroMemory(rx->RxdBase, rx->RxdSize);

    PHYSICAL_ADDRESS pa = WdfCommonBufferGetAlignedLogicalAddress(rx->RxdArray);
    if (rx->QueueId == 0)
    {
        adapter->CSRAddress->RDSARLow = pa.LowPart;
        adapter->CSRAddress->RDSARHigh = pa.HighPart;
    }
    else
    {
        GigaMacSetReceiveDescriptorStartAddress(adapter, rx->QueueId, pa);
    }

    WdfSpinLockAcquire(adapter->Lock);

    if (! (adapter->CSRAddress->CmdReg & CR_RE))
    {
        adapter->CSRAddress->CmdReg |= CR_RE;
    }
    adapter->RxQueues[rx->QueueId] = rxQueue;

    RtAdapterUpdateRcr(adapter);

    WdfSpinLockRelease(adapter->Lock);
}

_Use_decl_annotations_
void
EvtRxQueueStop(
    NETPACKETQUEUE rxQueue
    )
{
    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);

    WdfSpinLockAcquire(rx->Adapter->Lock);

    bool count = 0;
    for (size_t i = 0; i < ARRAYSIZE(rx->Adapter->RxQueues); i++)
    {
        if (rx->Adapter->RxQueues[i])
        {
            count++;
        }
    }

    if (1 == count)
    {
        rx->Adapter->CSRAddress->CmdReg &= ~CR_RE;
    }

    RtRxQueueSetInterrupt(rx, false);
    rx->Adapter->RxQueues[rx->QueueId] = WDF_NO_HANDLE;

    WdfSpinLockRelease(rx->Adapter->Lock);
}

_Use_decl_annotations_
void
EvtRxQueueDestroy(
    _In_ WDFOBJECT rxQueue
    )
{
    TraceEntry(TraceLoggingPointer(rxQueue, "RxQueue"));

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);

    WdfObjectDelete(rx->RxdArray);
    rx->RxdArray = NULL;

    TraceExit();
}

_Use_decl_annotations_
VOID
EvtRxQueueSetNotificationEnabled(
    _In_ NETPACKETQUEUE rxQueue,
    _In_ BOOLEAN notificationEnabled
    )
{
    TraceEntry(TraceLoggingPointer(rxQueue), TraceLoggingBoolean(notificationEnabled));

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);

    RtRxQueueSetInterrupt(rx, notificationEnabled);

    TraceExit();
}

_Use_decl_annotations_
void
EvtRxQueueAdvance(
    _In_ NETPACKETQUEUE rxQueue
    )
{
    TraceEntry(TraceLoggingPointer(rxQueue, "RxQueue"));

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);

    RxIndicateReceives(rx);
    RxPostBuffers(rx);

    TraceExit();
}

_Use_decl_annotations_
void
EvtRxQueueCancel(
    _In_ NETPACKETQUEUE rxQueue
    )
{
    TraceEntry(TraceLoggingPointer(rxQueue, "RxQueue"));

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);
    RT_ADAPTER *adapter = rx->Adapter;

    WdfSpinLockAcquire(rx->Adapter->Lock);

    adapter->CSRAddress->RCR = TCR_RCR_MXDMA_UNLIMITED << RCR_MXDMA_OFFSET;
    adapter->CSRAddress->CmdReg &= ~CR_RE;

    WdfSpinLockRelease(rx->Adapter->Lock);

    // try (but not very hard) to grab anything that may have been
    // indicated during rx disable. advance will continue to be called
    // after cancel until all packets are returned to the framework.
    RxIndicateReceives(rx);

    NetRingBufferReturnAllPackets(NetRxQueueGetDatapathDescriptor(rxQueue));

    TraceExit();
}
