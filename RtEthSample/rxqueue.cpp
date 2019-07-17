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

#include "netringiterator.h"

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
    _In_    UINT32 packetIndex,
    _Inout_ NET_PACKET *packet
    )
{
    RT_ADAPTER* adapter = rx->Adapter;
    NET_PACKET_CHECKSUM* checksumInfo =
        NetExtensionGetPacketChecksum(
            &rx->ChecksumExtension,
            packetIndex);

    packet->Layout.Layer2Type = NetPacketLayer2TypeEthernet;
    checksumInfo->Layer2 =
        (rxd->RxDescDataIpv6Rss.status & RXS_CRC)
        ? NetPacketRxChecksumEvaluationInvalid
        : NetPacketRxChecksumEvaluationValid;

    USHORT const isIpv4 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV4;
    USHORT const isIpv6 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV6;

    NT_ASSERT(!(isIpv4 && isIpv6));

    if (isIpv4)
    {
        packet->Layout.Layer3Type = NetPacketLayer3TypeIPv4UnspecifiedOptions;

        if (adapter->IpHwChkSum)
        {
            checksumInfo->Layer3 =
                (rxd->RxDescDataIpv6Rss.status & RXS_IPF)
                ? NetPacketRxChecksumEvaluationInvalid
                : NetPacketRxChecksumEvaluationValid;
        }
    }
    else if (isIpv6)
    {
        packet->Layout.Layer3Type = NetPacketLayer3TypeIPv6UnspecifiedExtensions;
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
        packet->Layout.Layer4Type = NetPacketLayer4TypeTcp;

        if (adapter->TcpHwChkSum)
        {
            checksumInfo->Layer4 =
                (rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_TCPF)
                ? NetPacketRxChecksumEvaluationInvalid
                : NetPacketRxChecksumEvaluationValid;
        }
    }
    else if (isUdp)
    {
        packet->Layout.Layer4Type = NetPacketLayer4TypeUdp;

        if (adapter->UdpHwChkSum)
        {
            checksumInfo->Layer4 =
                (rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_UDPF)
                ? NetPacketRxChecksumEvaluationInvalid
                : NetPacketRxChecksumEvaluationValid;
        }
    }
}

static
void
RxFillRtl8111EChecksumInfo(
    _In_    RT_RXQUEUE const *rx,
    _In_    RT_RX_DESC const *rxd,
    _In_    UINT32 packetIndex,
    _Inout_ NET_PACKET *packet
    )
{
    RT_ADAPTER* adapter = rx->Adapter;
    NET_PACKET_CHECKSUM* checksumInfo =
        NetExtensionGetPacketChecksum(
            &rx->ChecksumExtension,
            packetIndex);

    packet->Layout.Layer2Type = NetPacketLayer2TypeEthernet;
    checksumInfo->Layer2 =
        (rxd->RxDescDataIpv6Rss.status & RXS_CRC)
        ? NetPacketRxChecksumEvaluationInvalid
        : NetPacketRxChecksumEvaluationValid;

    USHORT const isIpv4 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV4;
    USHORT const isIpv6 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV6;

    NT_ASSERT(!(isIpv4 && isIpv6));

    if (isIpv4)
    {
        packet->Layout.Layer3Type = NetPacketLayer3TypeIPv4UnspecifiedOptions;

        if (adapter->IpHwChkSum)
        {
            checksumInfo->Layer3 =
                (rxd->RxDescDataIpv6Rss.status & RXS_IPF)
                ? NetPacketRxChecksumEvaluationInvalid
                : NetPacketRxChecksumEvaluationValid;
        }
    }
    else if (isIpv6)
    {
        packet->Layout.Layer3Type = NetPacketLayer3TypeIPv6UnspecifiedExtensions;
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
        packet->Layout.Layer4Type = NetPacketLayer4TypeTcp;

        if (adapter->TcpHwChkSum)
        {
            checksumInfo->Layer4 =
                (rxd->RxDescDataIpv6Rss.TcpUdpFailure & TXS_TCPCS)
                ? NetPacketRxChecksumEvaluationInvalid
                : NetPacketRxChecksumEvaluationValid;
        }
    }
    else if (isUdp)
    {
        packet->Layout.Layer4Type = NetPacketLayer4TypeUdp;

        if (adapter->UdpHwChkSum)
        {
            checksumInfo->Layer4 =
                (rxd->RxDescDataIpv6Rss.TcpUdpFailure & TXS_UDPCS)
                ? NetPacketRxChecksumEvaluationInvalid
                : NetPacketRxChecksumEvaluationValid;
        }
    }
}

static
void
RtFillRxChecksumInfo(
    _In_    RT_RXQUEUE const *rx,
    _In_    RT_RX_DESC const *rxd,
    _In_    UINT32 packetIndex,
    _Inout_ NET_PACKET *packet
    )
{
    packet->Layout = {};

    switch (rx->Adapter->ChipType)
    {
    case RTL8168D:
        RxFillRtl8111DChecksumInfo(rx, rxd, packetIndex, packet);
        break;
    case RTL8168D_REV_C_REV_D:
    case RTL8168E:
        RxFillRtl8111EChecksumInfo(rx, rxd, packetIndex, packet);
        break;
    }
}

void
RxIndicateReceives(
    _In_ RT_RXQUEUE *rx
    )
{
    NET_RING_FRAGMENT_ITERATOR fi = NetRingGetDrainFragments(rx->Rings);
    NET_RING_PACKET_ITERATOR pi = NetRingGetAllPackets(rx->Rings);
    while (NetFragmentIteratorHasAny(&fi))
    {
        UINT32 index = NetFragmentIteratorGetIndex(&fi);
        RT_RX_DESC const * rxd = &rx->RxdBase[index];

        if (0 != (rxd->RxDescDataIpv6Rss.status & RXS_OWN))
            break;

        NET_FRAGMENT * fragment = NetFragmentIteratorGetFragment(&fi);
        fragment->ValidLength = rxd->RxDescDataIpv6Rss.length - FRAME_CRC_SIZE;
        fragment->Offset = 0;

        NET_PACKET * packet = NetPacketIteratorGetPacket(&pi);
        packet->FragmentIndex = index;
        packet->FragmentCount = 1;

        if (rx->ChecksumExtension.Enabled)
        {
            // fill packetTcpChecksum
            RtFillRxChecksumInfo(rx, rxd, NetPacketIteratorGetIndex(&pi), packet);
        }

        RtUpdateRecvStats(rx, rxd, fragment->ValidLength);

        NetFragmentIteratorAdvance(&fi);
        NetPacketIteratorAdvance(&pi);
    }
    NetFragmentIteratorSet(&fi);
    NetPacketIteratorSet(&pi);
}

static
void
RtPostRxDescriptor(
    _In_ RT_RX_DESC * desc,
    _In_ NET_FRAGMENT const * fragment,
    _In_ NET_FRAGMENT_LOGICAL_ADDRESS const * logicalAddress,
    _In_ UINT16 status
    )
{
    desc->BufferAddress = logicalAddress->LogicalAddress;
    desc->RxDescDataIpv6Rss.TcpUdpFailure = 0;
    desc->RxDescDataIpv6Rss.length = fragment->Capacity;
    desc->RxDescDataIpv6Rss.VLAN_TAG.Value = 0;

    MemoryBarrier();

    desc->RxDescDataIpv6Rss.status = status;
}

static
void
RxPostBuffers(
    _In_ RT_RXQUEUE *rx
    )
{
    NET_RING * fr = NetRingCollectionGetFragmentRing(rx->Rings);
    NET_RING_FRAGMENT_ITERATOR fi = NetRingGetPostFragments(rx->Rings);

    while (NetFragmentIteratorHasAny(&fi))
    {
        UINT32 const index = NetFragmentIteratorGetIndex(&fi);
        NET_FRAGMENT_LOGICAL_ADDRESS const * logicalAddress = NetExtensionGetFragmentLogicalAddress(
            &rx->LogicalAddressExtension, index);

        RtPostRxDescriptor(&rx->RxdBase[index],
            NetFragmentIteratorGetFragment(&fi),
            logicalAddress,
            RXS_OWN | (fr->ElementIndexMask == index ? RXS_EOR : 0));
        NetFragmentIteratorAdvance(&fi);
    }
    NetFragmentIteratorSet(&fi);
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
    rx->Rings = NetRxQueueGetRingCollection(rxQueue);

    // allocate descriptors
    NET_RING * pr = NetRingCollectionGetPacketRing(rx->Rings);
    UINT32 const rxdSize = pr->NumberOfElements * sizeof(RT_RX_DESC);
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
    _In_ NET_PACKET_FILTER_FLAGS packetFilter
    )
{
    if (packetFilter & NetPacketFilterFlagPromiscuous)
    {
        return (RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_AR | RCR_AER);
    }

    return
        ((packetFilter & NetPacketFilterFlagAllMulticast) ? RCR_AM  : 0) |
        ((packetFilter & NetPacketFilterFlagMulticast)     ? RCR_AM  : 0) |
        ((packetFilter & NetPacketFilterFlagBroadcast)     ? RCR_AB  : 0) |
        ((packetFilter & NetPacketFilterFlagDirected)      ? RCR_APM : 0);
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

    NET_RING_PACKET_ITERATOR pi = NetRingGetAllPackets(rx->Rings);
    while(NetPacketIteratorHasAny(&pi))
    {
        NetPacketIteratorGetPacket(&pi)->Ignore = 1;
        NetPacketIteratorAdvance(&pi);
    }
    NetPacketIteratorSet(&pi);

    NET_RING_FRAGMENT_ITERATOR fi = NetRingGetAllFragments(rx->Rings);
    NetFragmentIteratorAdvanceToTheEnd(&fi);
    NetFragmentIteratorSet(&fi);

    TraceExit();
}
