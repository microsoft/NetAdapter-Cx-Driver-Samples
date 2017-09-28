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

void
RtUpdateRecvStats(
    _In_ RT_RXQUEUE *rx,
    _In_ RT_RX_DESC const *rxd,
         ULONG length)
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
    _In_    RT_ADAPTER const *adapter,
    _In_    RT_RX_DESC const *rxd,
    _Inout_ NET_PACKET *packet)
{
    packet->Layout.Layer2Type = NET_PACKET_LAYER2_TYPE_ETHERNET;
    packet->Checksum.Layer2 =
        (rxd->RxDescDataIpv6Rss.status & RXS_CRC)
        ? NET_PACKET_RX_CHECKSUM_INVALID
        : NET_PACKET_RX_CHECKSUM_VALID;

    USHORT const isIpv4 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV4;
    USHORT const isIpv6 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV6;

    NT_ASSERT(!(isIpv4 && isIpv6));

    if (isIpv4)
    {
        packet->Layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS;

        if (adapter->IpRxHwChkSumv4)
        {
            packet->Checksum.Layer3 =
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

        if ((isIpv4 && adapter->TcpRxHwChkSumv4) ||
            (isIpv6 && adapter->TcpRxHwChkSumv6))
        {
            packet->Checksum.Layer4 =
                (rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_TCPF)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
    else if (isUdp)
    {
        packet->Layout.Layer4Type = NET_PACKET_LAYER4_TYPE_UDP;

        if ((isIpv4 && adapter->UdpRxHwChkSumv4) ||
            (isIpv6 && adapter->UdpRxHwChkSumv6))
        {
            packet->Checksum.Layer4 =
                (rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_UDPF)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
}

static
void
RxFillRtl8111EChecksumInfo(
    _In_    RT_ADAPTER const *adapter,
    _In_    RT_RX_DESC const *rxd,
    _Inout_ NET_PACKET *packet)
{
    packet->Layout.Layer2Type = NET_PACKET_LAYER2_TYPE_ETHERNET;
    packet->Checksum.Layer2 =
        (rxd->RxDescDataIpv6Rss.status & RXS_CRC)
        ? NET_PACKET_RX_CHECKSUM_INVALID
        : NET_PACKET_RX_CHECKSUM_VALID;

    USHORT const isIpv4 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV4;
    USHORT const isIpv6 = rxd->RxDescDataIpv6Rss.IpRssTava & RXS_IPV6RSS_IS_IPV6;

    NT_ASSERT(!(isIpv4 && isIpv6));

    if (isIpv4)
    {
        packet->Layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS;

        if (adapter->IpRxHwChkSumv4)
        {
            packet->Checksum.Layer3 =
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

        if ((isIpv4 && adapter->TcpRxHwChkSumv4) ||
            (isIpv6 && adapter->TcpRxHwChkSumv6))
        {
            packet->Checksum.Layer4 =
                (rxd->RxDescDataIpv6Rss.TcpUdpFailure & TXS_TCPCS)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
    else if (isUdp)
    {
        packet->Layout.Layer4Type = NET_PACKET_LAYER4_TYPE_UDP;

        if ((isIpv4 && adapter->UdpRxHwChkSumv4) ||
            (isIpv6 && adapter->UdpRxHwChkSumv6))
        {
            packet->Checksum.Layer4 =
                (rxd->RxDescDataIpv6Rss.TcpUdpFailure & TXS_UDPCS)
                ? NET_PACKET_RX_CHECKSUM_INVALID
                : NET_PACKET_RX_CHECKSUM_VALID;
        }
    }
}

static
void
RtFillRxChecksumInfo(
    _In_    RT_ADAPTER const *adapter,
    _In_    RT_RX_DESC const *rxd,
    _Inout_ NET_PACKET *packet)
{
    switch (adapter->ChipType)
    {
    case RTL8168D:
        RxFillRtl8111DChecksumInfo(adapter, rxd, packet);
        break;
    case RTL8168D_REV_C_REV_D:
    case RTL8168E:
        RxFillRtl8111EChecksumInfo(adapter, rxd, packet);
        break;
    }
}

void
RxIndicateReceives(
    _In_ RT_RXQUEUE *rx)
{
    NET_RING_BUFFER *rb = rx->RingBuffer;

    UINT32 i;

    for (i = rb->BeginIndex; i != rb->NextIndex; i = NetRingBufferIncrementIndex(rb, i))
    {
        RT_RX_DESC *rxd = &rx->RxdBase[i];
        NET_PACKET *packet = NetRingBufferGetPacketAtIndex(rb, i);

        if (0 != (rxd->RxDescDataIpv6Rss.status & RXS_OWN))
            break;

        packet->Data.ValidLength = rxd->RxDescDataIpv6Rss.length - FRAME_CRC_SIZE;
        packet->Data.Offset = 0;

        packet->Data.LastFragmentOfFrame = true;
        packet->Data.LastPacketOfChain = true;
        
        RtFillRxChecksumInfo(rx->Adapter, rxd, packet);

        RtUpdateRecvStats(rx, rxd, packet->Data.ValidLength);
    }

    rb->BeginIndex = i;
}

void
RxPostBuffers(_In_ RT_RXQUEUE *rx)
{
    NET_RING_BUFFER *rb = rx->RingBuffer;

    UINT32 initialIndex = rb->NextIndex;

    while (true)
    {
        UINT32 index = rb->NextIndex;
        RT_RX_DESC *rxd = &rx->RxdBase[index];

        NET_PACKET *packet = NetRingBufferAdvanceNextPacket(rb);
        if (!packet)
            break;

        rxd->BufferAddress = packet->Data.Mapping.DmaLogicalAddress;
        rxd->RxDescDataIpv6Rss.TcpUdpFailure = 0;
        rxd->RxDescDataIpv6Rss.length = packet->Data.Capacity;
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
RtRxQueueInitialize(_In_ NETRXQUEUE rxQueue, _In_ RT_ADAPTER *adapter)
{
    NTSTATUS status = STATUS_SUCCESS;

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);

    rx->Adapter = adapter;
    rx->Interrupt = adapter->Interrupt;
    rx->RingBuffer = NetRxQueueGetRingBuffer(rxQueue);

    // allocate descriptors
    {
        WdfDeviceSetAlignmentRequirement(adapter->WdfDevice, FILE_256_BYTE_ALIGNMENT);

        SIZE_T rxdSize = rx->RingBuffer->NumberOfElements * sizeof(RT_RX_DESC);
        GOTO_IF_NOT_NT_SUCCESS(Exit, status,
            WdfCommonBufferCreate(
                rx->Adapter->DmaEnabler,
                rxdSize,
                WDF_NO_OBJECT_ATTRIBUTES,
                &rx->RxdArray));

        rx->RxdBase = static_cast<RT_RX_DESC*>(WdfCommonBufferGetAlignedVirtualAddress(rx->RxdArray));
        RtlZeroMemory(rx->RxdBase, rxdSize);
    }

Exit:
    return status;
}

ULONG
RtConvertPacketFilterToRcr(NET_PACKET_FILTER_TYPES_FLAGS packetFilter)
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
RtAdapterUpdateRcr(_In_ RT_ADAPTER *adapter)
{
    adapter->CSRAddress->RCR =
        (TCR_RCR_MXDMA_UNLIMITED << RCR_MXDMA_OFFSET) |
            RtConvertPacketFilterToRcr(adapter->PacketFilter);
}

_Use_decl_annotations_
void
RtRxQueueStart(_In_ RT_RXQUEUE *rx)
{
    RT_ADAPTER *adapter = rx->Adapter;

    adapter->CSRAddress->RMS = RT_MAX_FRAME_SIZE;

    USHORT cpcr = adapter->CSRAddress->CPCR;

    if (adapter->IpRxHwChkSumv4 || adapter->TcpRxHwChkSumv4 || adapter->UdpRxHwChkSumv4)
    {
        cpcr |= CPCR_RX_CHECKSUM;
    }
    else
    {
        cpcr &= ~CPCR_RX_CHECKSUM;
    }

    adapter->CSRAddress->CPCR = cpcr;

    PHYSICAL_ADDRESS pa = WdfCommonBufferGetAlignedLogicalAddress(rx->RxdArray);

    adapter->CSRAddress->RDSARLow = pa.LowPart;
    adapter->CSRAddress->RDSARHigh = pa.HighPart;

    RtAdapterUpdateRcr(adapter);

    adapter->CSRAddress->CmdReg |= CR_RE;
}

void
RtRxQueueSetInterrupt(_In_ RT_RXQUEUE *rx, _In_ BOOLEAN notificationEnabled)
{
    InterlockedExchange(&rx->Interrupt->RxNotifyArmed, notificationEnabled);
    RtUpdateImr(rx->Interrupt);

    if (!notificationEnabled)
        // block this thread until we're sure any outstanding DPCs are complete.
        // This is to guarantee we don't return from this function call until
        // any oustanding rx notification is complete.
        KeFlushQueuedDpcs();
}

_Use_decl_annotations_
void
EvtRxQueueDestroy(_In_ WDFOBJECT rxQueue)
{
    TraceEntry(TraceLoggingPointer(rxQueue, "RxQueue"));

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);

    WdfSpinLockAcquire(rx->Adapter->Lock); {

        rx->Adapter->CSRAddress->CmdReg &= ~CR_RE;

        RtRxQueueSetInterrupt(rx, false);

        rx->Adapter->CSRAddress->CPCR &= ~(CPCR_RX_VLAN | CPCR_RX_CHECKSUM);

        rx->Adapter->RxQueue = WDF_NO_HANDLE;

    } WdfSpinLockRelease(rx->Adapter->Lock);

    WdfObjectDelete(rx->RxdArray);
    rx->RxdArray = NULL;

    TraceExit();
}

_Use_decl_annotations_
VOID
EvtRxQueueSetNotificationEnabled(
    _In_ NETRXQUEUE rxQueue,
    _In_ BOOLEAN notificationEnabled)
{
    TraceEntry(TraceLoggingPointer(rxQueue), TraceLoggingBoolean(notificationEnabled));

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);

    RtRxQueueSetInterrupt(rx, notificationEnabled);

    TraceExit();
}

_Use_decl_annotations_
void
EvtRxQueueAdvance(
    _In_ NETRXQUEUE rxQueue)
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
    _In_ NETRXQUEUE rxQueue)
{
    TraceEntry(TraceLoggingPointer(rxQueue, "RxQueue"));

    NET_RING_BUFFER *ringBuffer = NetRxQueueGetRingBuffer(rxQueue);
    ringBuffer->BeginIndex = ringBuffer->NextIndex = ringBuffer->EndIndex;

    TraceExit();
}