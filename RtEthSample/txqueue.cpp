/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "device.h"
#include "txqueue.h"
#include "trace.h"
#include "adapter.h"
#include "interrupt.h"

#include <preview/netringiterator.h>

void
RtUpdateSendStats(
    _In_ RT_TXQUEUE * tx,
    _In_ NET_RING_PACKET_ITERATOR const * pi
    )
{
    NET_PACKET const * packet = NetPacketIteratorGetPacket(pi);
    if (packet->Layout.Layer2Type != NET_PACKET_LAYER2_TYPE_ETHERNET)
    {
        return;
    }

    NET_RING_FRAGMENT_ITERATOR fi = NetPacketIteratorGetFragments(pi);
    NET_FRAGMENT * fragment = NetFragmentIteratorGetFragment(&fi);
    // Ethernet header should be in first fragment
    if (fragment->ValidLength < sizeof(ETHERNET_HEADER))
    {
        return;
    }

    PUCHAR ethHeader = (PUCHAR)fragment->VirtualAddress + fragment->Offset;

    ULONG length = 0;
    while (NetFragmentIteratorHasAny(&fi))
    {
        fragment = NetFragmentIteratorGetFragment(&fi);
        length += (ULONG)fragment->ValidLength;
        NetFragmentIteratorAdvance(&fi);
    }

    RT_ADAPTER *adapter = tx->Adapter;

    if (ETH_IS_BROADCAST(ethHeader))
    {
        adapter->OutBroadcastPkts++;
        adapter->OutBroadcastOctets += length;
    }
    else if (ETH_IS_MULTICAST(ethHeader))
    {
        adapter->OutMulticastPkts++;
        adapter->OutMulticastOctets += length;
    }
    else
    {
        adapter->OutUCastPkts++;
        adapter->OutUCastOctets += length;
    }
}

static
USHORT
RtGetPacketLsoStatusSetting(
    _In_ const NET_PACKET *packet
    )
{
    const USHORT layer4HeaderOffset =
        packet->Layout.Layer2HeaderLength +
        packet->Layout.Layer3HeaderLength;

    if (NetPacketIsIpv4(packet))
    {
        return TXS_IPV6RSS_GTSEN_IPV4 | (USHORT)(layer4HeaderOffset << TXS_IPV4RSS_TCPHDR_OFFSET);
    }
    else if (NetPacketIsIpv6(packet))
    {
        return TXS_IPV6RSS_GTSEN_IPV6 | (USHORT)(layer4HeaderOffset << TXS_IPV6RSS_TCPHDR_OFFSET);
    }

    return 0;
}

static
UINT16
RtGetPacketLsoMss(
    _In_ const NET_EXTENSION * extension,
    _In_ UINT32 packetIndex
)
{
    return NetExtensionGetPacketLargeSendSegmentation(extension, packetIndex)->TCP.Mss;
}

static
USHORT
RtGetPacketChecksumSetting(
    _In_ NET_PACKET const * packet,
    _In_ NET_EXTENSION const * checksumExtension,
    _In_ UINT32 packetIndex
    )
{
    NET_PACKET_CHECKSUM* checksumInfo =
        NetExtensionGetPacketChecksum(checksumExtension, packetIndex);

    if (packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV4_NO_OPTIONS ||
        packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV4_WITH_OPTIONS ||
        packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS)
    {
        // Prioritize layer4 checksum first
        if (checksumInfo->Layer4 == NET_PACKET_TX_CHECKSUM_REQUIRED)
        {
            if (packet->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP)
            {
                return TXS_IPV6RSS_TCPCS | TXS_IPV6RSS_IPV4CS;
            }

            if (packet->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_UDP)
            {
                return TXS_IPV6RSS_UDPCS | TXS_IPV6RSS_IPV4CS;
            }
        }

        // If no layer4 checksum is required, then just do layer 3 checksum
        if (checksumInfo->Layer3 == NET_PACKET_TX_CHECKSUM_REQUIRED)
        {
            return TXS_IPV6RSS_IPV4CS;
        }

        return 0;
    }
    
    if (packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV6_NO_EXTENSIONS ||
        packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV6_WITH_EXTENSIONS ||
        packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS)
    {
        if (checksumInfo->Layer4 == NET_PACKET_TX_CHECKSUM_REQUIRED)
        {
            const USHORT layer4HeaderOffset =
                packet->Layout.Layer2HeaderLength +
                packet->Layout.Layer3HeaderLength;

            if (packet->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP)
            {
                return TXS_IPV6RSS_TCPCS | TXS_IPV6RSS_IS_IPV6 |
                    (layer4HeaderOffset << TXS_IPV6RSS_TCPHDR_OFFSET);
            }

            if (packet->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_UDP)
            {
                return TXS_IPV6RSS_UDPCS | TXS_IPV6RSS_IS_IPV6 |
                    (layer4HeaderOffset << TXS_IPV6RSS_TCPHDR_OFFSET);
            }
        }

        // No IPv6 layer3 checksum
        return 0;
    }

    return 0;
}

static
UINT16
RtProgramOffloadDescriptor(
    _In_ RT_TXQUEUE const * tx,
    _In_ NET_PACKET const * packet,
    _In_ RT_TX_DESC * txd,
    _In_ UINT32 packetIndex
    )
{
    UINT16 status = 0;

    txd->TxDescDataIpv6Rss_All.OffloadGsoMssTagc = 0;
    RT_ADAPTER* adapter = tx->Adapter;

    if (packet->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP
        && RtGetPacketLsoMss(&tx->LsoExtension, packetIndex) > 0)
    {
        if (tx->LsoExtension.Enabled &&
            (adapter->LSOv4 == RtLsoOffloadEnabled || adapter->LSOv6 == RtLsoOffloadEnabled))
        {
            status |= RtGetPacketLsoStatusSetting(packet);
            txd->TxDescDataIpv6Rss_All.OffloadGsoMssTagc =
                RtGetPacketLsoMss(&tx->LsoExtension, packetIndex) << TXS_IPV6RSS_MSS_OFFSET;
        }
    }
    else
    {
        if (tx->ChecksumExtension.Enabled &&
            (adapter->TcpHwChkSum || adapter->IpHwChkSum || adapter->UdpHwChkSum))
        {
            txd->TxDescDataIpv6Rss_All.OffloadGsoMssTagc =
                RtGetPacketChecksumSetting(packet, &tx->ChecksumExtension, packetIndex);
        }
    }

    return status;
}

static
void
RtPostTxDescriptor(
    _In_ RT_TXQUEUE * tx,
    _In_ RT_TCB const * tcb,
    _In_ NET_PACKET const * packet,
    _In_ UINT32 packetIndex
    )
{
    NET_RING * fr = NetRingCollectionGetFragmentRing(tx->Rings);
    RT_TX_DESC * txd = &tx->TxdBase[tx->TxDescIndex];
    UINT16 status = TXS_OWN;

    if (tx->TxDescIndex == tx->NumTxDesc - 1)
    {
        status |= TXS_EOR;
    }

    // first fragment
    if (tcb->NumTxDesc == 0)
    {
        status |= TXS_FS;
    }

    // last fragment
    if (tcb->NumTxDesc + 1 == packet->FragmentCount)
    {
        status |= TXS_LS;
    }

    // calculate the index in the fragment ring and retrieve
    // the fragment being posted to populate the hardware descriptor
    UINT32 const index = (packet->FragmentIndex + tcb->NumTxDesc) & fr->ElementIndexMask;
    NET_FRAGMENT const * fragment = NetRingGetFragmentAtIndex(fr, index);

    txd->BufferAddress = fragment->Mapping.DmaLogicalAddress + fragment->Offset;
    txd->TxDescDataIpv6Rss_All.length = (USHORT)fragment->ValidLength;
    txd->TxDescDataIpv6Rss_All.VLAN_TAG.Value = 0;

    status |= RtProgramOffloadDescriptor(tx, packet, txd, packetIndex);

    MemoryBarrier();

    txd->TxDescDataIpv6Rss_All.status = status;
    tx->TxDescIndex = (tx->TxDescIndex + 1) % tx->NumTxDesc;
}

static
void
RtFlushTransation(
    _In_ RT_TXQUEUE *tx
    )
{
    MemoryBarrier();
    *tx->TPPoll = TPPoll_NPQ;
}

static
RT_TCB*
GetTcbFromPacket(
    _In_ RT_TXQUEUE *tx,
    _In_ UINT32 Index
    )
{
    return &tx->PacketContext[Index];
}

static
bool
RtIsPacketTransferComplete(
    _In_ RT_TXQUEUE *tx,
    _In_ NET_RING_PACKET_ITERATOR const * pi
    )
{
    NET_PACKET const * packet = NetPacketIteratorGetPacket(pi);
    if (! packet->Ignore)
    {
        RT_TCB const * tcb = GetTcbFromPacket(tx, NetPacketIteratorGetIndex(pi));
        RT_TX_DESC * txd = &tx->TxdBase[tcb->FirstTxDescIdx];

        // Look at the status flags on the last fragment in the packet.
        // If the hardware-ownership flag is still set, then the packet isn't done.
        if (0 != (txd->TxDescDataIpv6Rss_All.status & TXS_OWN))
        {
            return false;
        }

        NET_RING_FRAGMENT_ITERATOR fi = NetPacketIteratorGetFragments(pi);
        for (size_t idx = 0; idx < tcb->NumTxDesc; idx++)
        {
            size_t nextTxDescIdx = (tcb->FirstTxDescIdx + idx) % tx->NumTxDesc;
            txd = &tx->TxdBase[nextTxDescIdx];
            txd->TxDescDataIpv6Rss_All.status = 0;
            NetFragmentIteratorAdvance(&fi);
        }
        RtUpdateSendStats(tx, pi);
        fi.Iterator.Rings->Rings[NET_RING_TYPE_FRAGMENT]->BeginIndex
            = NetFragmentIteratorGetIndex(&fi);
    }

    return true;
}

static
void
RtTransmitPackets(
    _In_ RT_TXQUEUE *tx
    )
{
    bool programmedPackets = false;

    NET_RING_PACKET_ITERATOR pi = NetRingGetPostPackets(tx->Rings);
    while (NetPacketIteratorHasAny(&pi))
    {
        NET_PACKET * packet = NetPacketIteratorGetPacket(&pi);
        if (! packet->Ignore)
        {
            RT_TCB* tcb = GetTcbFromPacket(tx, NetPacketIteratorGetIndex(&pi));

            tcb->FirstTxDescIdx = tx->TxDescIndex;

            NET_RING_FRAGMENT_ITERATOR fi = NetPacketIteratorGetFragments(&pi);
            for (tcb->NumTxDesc = 0; NetFragmentIteratorHasAny(&fi); tcb->NumTxDesc++)
            {
                RtPostTxDescriptor(tx, tcb, packet, NetPacketIteratorGetIndex(&pi));
                NetFragmentIteratorAdvance(&fi);
            }
            fi.Iterator.Rings->Rings[NET_RING_TYPE_FRAGMENT]->NextIndex
                = NetFragmentIteratorGetIndex(&fi);

            programmedPackets = true;
        }
        NetPacketIteratorAdvance(&pi);
    }
    NetPacketIteratorSet(&pi);

    if (programmedPackets)
    {
        RtFlushTransation(tx);
    }
}

static
void
RtCompleteTransmitPackets(
    _In_ RT_TXQUEUE *tx
    )
{

    NET_RING_PACKET_ITERATOR pi = NetRingGetDrainPackets(tx->Rings);
    while (NetPacketIteratorHasAny(&pi))
    {
        if (! RtIsPacketTransferComplete(tx, &pi))
        {
            break;
        }

        NetPacketIteratorAdvance(&pi);
    }
    NetPacketIteratorSet(&pi);
}

_Use_decl_annotations_
void
EvtTxQueueAdvance(
    _In_ NETPACKETQUEUE txQueue
    )
{
    TraceEntry(TraceLoggingPointer(txQueue, "TxQueue"));

    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    RtTransmitPackets(tx);
    RtCompleteTransmitPackets(tx);

    TraceExit();
}

NTSTATUS
RtTxQueueInitialize(
    _In_ NETPACKETQUEUE txQueue,
    _In_ RT_ADAPTER * adapter
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    tx->Adapter = adapter;

    tx->TPPoll = &adapter->CSRAddress->TPPoll;
    tx->Interrupt = adapter->Interrupt;
    tx->NumTxDesc = (USHORT)(adapter->NumTcb * RT_MAX_PHYS_BUF_COUNT);
    
    tx->Rings = NetTxQueueGetRingCollection(txQueue);

    NET_RING * pr = NetRingCollectionGetPacketRing(tx->Rings);
    WDF_OBJECT_ATTRIBUTES tcbAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&tcbAttributes);
    tcbAttributes.ParentObject = txQueue;
    WDFMEMORY memory = NULL;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfMemoryCreate(
            &tcbAttributes,
            NonPagedPoolNx,
            0,
            sizeof(RT_TCB) * pr->NumberOfElements,
            &memory,
            (void**)&tx->PacketContext
        ));

    ULONG txSize;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtlULongMult(tx->NumTxDesc, sizeof(RT_TX_DESC), &txSize));

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfCommonBufferCreate(
            tx->Adapter->DmaEnabler,
            txSize,
            WDF_NO_OBJECT_ATTRIBUTES,
            &tx->TxdArray));

    tx->TxdBase = static_cast<RT_TX_DESC*>(
        WdfCommonBufferGetAlignedVirtualAddress(tx->TxdArray));
    tx->TxSize = txSize;

Exit:
    return status;
}

void
RtTxQueueSetInterrupt(
    _In_ RT_TXQUEUE *tx,
    _In_ BOOLEAN notificationEnabled
    )
{
    InterlockedExchange(&tx->Interrupt->TxNotifyArmed, notificationEnabled);
    RtUpdateImr(tx->Interrupt, 0);

    if (!notificationEnabled)
        // block this thread until we're sure any outstanding DPCs are complete.
        // This is to guarantee we don't return from this function call until
        // any oustanding tx notification is complete.
        KeFlushQueuedDpcs();
}

_Use_decl_annotations_
void
EvtTxQueueStart(
    _In_ NETPACKETQUEUE txQueue
    )
{
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);
    RT_ADAPTER *adapter = tx->Adapter;

    RtlZeroMemory(tx->TxdBase, tx->TxSize);

    tx->TxDescIndex = 0;

    WdfSpinLockAcquire(adapter->Lock);

    adapter->CSRAddress->TDFNR = 8;

    // Max transmit packet size
    adapter->CSRAddress->MtpsReg.MTPS = (RT_MAX_FRAME_SIZE + 128 - 1) / 128;

    PHYSICAL_ADDRESS pa = WdfCommonBufferGetAlignedLogicalAddress(tx->TxdArray);

    // let hardware know where transmit descriptors are at
    adapter->CSRAddress->TNPDSLow = pa.LowPart;
    adapter->CSRAddress->TNPDSHigh = pa.HighPart;

    adapter->CSRAddress->CmdReg |= CR_TE;

    // data sheet says TCR should only be modified after the transceiver is enabled
    adapter->CSRAddress->TCR = (TCR_RCR_MXDMA_UNLIMITED << TCR_MXDMA_OFFSET) | (TCR_IFG0 | TCR_IFG1 | TCR_BIT0);
    adapter->TxQueue = txQueue;

    WdfSpinLockRelease(adapter->Lock);
}

_Use_decl_annotations_
void
EvtTxQueueStop(
    NETPACKETQUEUE txQueue
    )
{
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    WdfSpinLockAcquire(tx->Adapter->Lock);

    tx->Adapter->CSRAddress->CmdReg &= ~CR_TE;

    RtTxQueueSetInterrupt(tx, false);
    tx->Adapter->TxQueue = WDF_NO_HANDLE;

    WdfSpinLockRelease(tx->Adapter->Lock);
}

_Use_decl_annotations_
void
EvtTxQueueDestroy(
    _In_ WDFOBJECT txQueue
    )
{
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    WdfObjectDelete(tx->TxdArray);
    tx->TxdArray = NULL;
}

_Use_decl_annotations_
VOID
EvtTxQueueSetNotificationEnabled(
    _In_ NETPACKETQUEUE txQueue,
    _In_ BOOLEAN notificationEnabled
    )
{
    TraceEntry(TraceLoggingPointer(txQueue), TraceLoggingBoolean(notificationEnabled));

    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    RtTxQueueSetInterrupt(tx, notificationEnabled);

    TraceExit();
}

_Use_decl_annotations_
void
EvtTxQueueCancel(
    _In_ NETPACKETQUEUE txQueue
    )
{
    TraceEntry(TraceLoggingPointer(txQueue, "TxQueue"));

    //
    // If the chipset is able to cancel outstanding IOs, then it should do so
    // here. However, the RTL8168D does not seem to support such a feature, so
    // the queue will continue to be drained like normal.
    //

    TraceExit();
}
