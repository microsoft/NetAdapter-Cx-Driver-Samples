/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include <wil/common.h>

#include "device.h"
#include "rxqueue.h"
#include "trace.h"
#include "adapter.h"
#include "interrupt.h"
#include "gigamac.h"

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

        if (adapter->RxIpHwChkSum)
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

        if (adapter->RxTcpHwChkSum)
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

        if (adapter->RxUdpHwChkSum)
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

        if (adapter->RxIpHwChkSum)
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

        if (adapter->RxTcpHwChkSum)
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

        if (adapter->RxUdpHwChkSum)
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

static
void
RtFillReceiveScalingInfo(
    _In_ RT_RXQUEUE const *rx,
    _In_ RT_RX_DESC const *rxd,
    _In_ UINT32 packetIndex
    )
{
    NET_PACKET_HASH * hash =
        NetExtensionGetPacketHash(
            &rx->HashValueExtension,
            packetIndex);

    hash->ProtocolType = NetPacketHashProtocolTypeNone;
    hash->HashValue = 0;

    if (WI_IsFlagSet(rxd->RxDescDataIpv6Rss.IpRssTava, RXS_IPV6RSS_RSS_IPV4))
    {
        WI_SetFlag(hash->ProtocolType, NetPacketHashProtocolTypeIPv4);

        if (WI_IsFlagSet(rxd->RxDescDataIpv6Rss.IpRssTava, RXS_IPV6RSS_RSS_TCP))
        {
            WI_SetFlag(hash->ProtocolType, NetPacketHashProtocolTypeTcp);
        }
    }

    if (WI_IsFlagSet(rxd->RxDescDataIpv6Rss.IpRssTava, RXS_IPV6RSS_RSS_IPV6))
    {
        WI_SetFlag(hash->ProtocolType, NetPacketHashProtocolTypeIPv6);

        if (WI_IsFlagSet(rxd->RxDescDataIpv6Rss.IpRssTava, RXS_IPV6RSS_RSS_TCP))
        {
            WI_SetFlag(hash->ProtocolType, NetPacketHashProtocolTypeTcp);
        }
    }

    if (hash->ProtocolType)
    {
        auto fr = NetRingCollectionGetFragmentRing(rx->Rings);

        auto fragmentVirtualAddress = NetExtensionGetFragmentVirtualAddress(&rx->VirtualAddressExtension, fr->BeginIndex);
        auto hashValueAddress = reinterpret_cast<unsigned char *>(fragmentVirtualAddress->VirtualAddress) + rxd->RxDescDataIpv6Rss.length;
        hashValueAddress += 3; // reversed

        auto hashValue = reinterpret_cast<unsigned char *>(&hash->HashValue);
        *hashValue++ = *hashValueAddress--;
        *hashValue++ = *hashValueAddress--;
        *hashValue++ = *hashValueAddress--;
        *hashValue = *hashValueAddress;
    }
}

void
RxIndicateReceives(
    _In_ RT_RXQUEUE *rx
    )
{
    NET_RING * pr = NetRingCollectionGetPacketRing(rx->Rings);
    NET_RING * fr = NetRingCollectionGetFragmentRing(rx->Rings);

    while (fr->BeginIndex != fr->NextIndex)
    {
        UINT32 const fragmentIndex = fr->BeginIndex;
        RT_RX_DESC const * rxd = &rx->RxdBase[fragmentIndex];

        if (0 != (rxd->RxDescDataIpv6Rss.status & RXS_OWN))
            break;

        if (pr->BeginIndex == pr->EndIndex)
            break;

        // If there is a packet available we are guaranteed to have a fragment as well
        NT_FRE_ASSERT(fr->BeginIndex != fr->EndIndex);

        NET_FRAGMENT * fragment = NetRingGetFragmentAtIndex(fr, fragmentIndex);
        fragment->ValidLength = rxd->RxDescDataIpv6Rss.length - FRAME_CRC_SIZE;
        fragment->Capacity = fragment->ValidLength;
        fragment->Offset = 0;

        // Link fragment and packet
        UINT32 const packetIndex = pr->BeginIndex;
        NET_PACKET* packet = NetRingGetPacketAtIndex(pr, packetIndex);
        packet->FragmentIndex = fragmentIndex;
        packet->FragmentCount = 1;

        if (rx->ChecksumExtension.Enabled)
        {
            // fill packetTcpChecksum
            RtFillRxChecksumInfo(rx, rxd, packetIndex, packet);
        }

        if (rx->HashValueExtension.Enabled && rx->Adapter->RssEnabled)
        {
            // fill packet hash value
            RtFillReceiveScalingInfo(rx, rxd, packetIndex);
        }

        packet->Layout.Layer2Type = NetPacketLayer2TypeEthernet;

        pr->BeginIndex = NetRingIncrementIndex(pr, pr->BeginIndex);
        fr->BeginIndex = NetRingIncrementIndex(fr, fr->BeginIndex);
    }
}

static
void
RtPostRxDescriptor(
    _In_ RT_RX_DESC * desc,
    _In_ NET_FRAGMENT_LOGICAL_ADDRESS const * logicalAddress,
    _In_ UINT16 status
    )
{
    desc->BufferAddress = logicalAddress->LogicalAddress;
    desc->RxDescDataIpv6Rss.TcpUdpFailure = 0;
    desc->RxDescDataIpv6Rss.length = RT_MAX_FRAME_SIZE;
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

    while (fr->NextIndex != fr->EndIndex)
    {
        UINT32 const index = fr->NextIndex;

        RtPostRxDescriptor(&rx->RxdBase[index],
            NetExtensionGetFragmentLogicalAddress(&rx->LogicalAddressExtension, index),
            RXS_OWN | (fr->ElementIndexMask == index ? RXS_EOR : 0));

        fr->NextIndex = NetRingIncrementIndex(fr, fr->NextIndex);
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

    size_t count = 0;
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

    NET_RING * pr = NetRingCollectionGetPacketRing(rx->Rings);

    while (pr->BeginIndex != pr->EndIndex)
    {
        NET_PACKET * packet = NetRingGetPacketAtIndex(pr, pr->BeginIndex);
        packet->Ignore = 1;

        pr->BeginIndex = NetRingIncrementIndex(pr, pr->BeginIndex);
    }

    NET_RING * fr = NetRingCollectionGetFragmentRing(rx->Rings);
    fr->BeginIndex = fr->EndIndex;

    TraceExit();
}
