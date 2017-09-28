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

void
RtUpdateSendStats(
    _In_ RT_TXQUEUE *tx,
    _In_ NET_PACKET *packet)
{
    if (packet->Layout.Layer2Type != NET_PACKET_LAYER2_TYPE_ETHERNET)
    {
        return;
    }

    // Ethernet header should be in first fragment
    if (packet->Data.ValidLength < sizeof(ETHERNET_HEADER))
    {
        return;
    }

    PUCHAR ethHeader = (PUCHAR)packet->Data.VirtualAddress + packet->Data.Offset;

    ULONG length = 0;
    for (NET_PACKET_FRAGMENT *fragment = &packet->Data;
        fragment;
        fragment = NET_PACKET_FRAGMENT_GET_NEXT(fragment))
    {
        length += (ULONG)fragment->ValidLength;

        if (fragment->LastFragmentOfFrame)
            break;
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

TX_DMA_BOUNCE_ANALYSIS
EvtSgBounceAnalysis(
    _In_ NETTXQUEUE txQueue,
    _In_ NET_PACKET *packet)
{
    UNREFERENCED_PARAMETER((txQueue, packet));

    return TxDmaTransmitInPlace;
}

USHORT
RtGetPacketChecksumSetting(NET_PACKET *packet)
{
    if (packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV4_NO_OPTIONS ||
        packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV4_WITH_OPTIONS ||
        packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS)
    {
        // Prioritize layer4 checksum first
        if (packet->Checksum.Layer4 == NET_PACKET_TX_CHECKSUM_REQUIRED)
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
        if (packet->Checksum.Layer3 == NET_PACKET_TX_CHECKSUM_REQUIRED)
        {
            return TXS_IPV6RSS_IPV4CS;
        }

        return 0;
    }
    
    if (packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV6_NO_EXTENSIONS ||
        packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV6_WITH_EXTENSIONS ||
        packet->Layout.Layer3Type == NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS)
    {
        if (packet->Checksum.Layer4 == NET_PACKET_TX_CHECKSUM_REQUIRED)
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

void
EvtSgProgramDescriptors(
    _In_ NETTXQUEUE txQueue,
    _In_ NET_PACKET *packet,
    _In_ SCATTER_GATHER_LIST *sgl)
{
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    RtUpdateSendStats(tx, packet);

    for (ULONG sgeIndex = 0; sgeIndex < sgl->NumberOfElements; sgeIndex++)
    {
        SCATTER_GATHER_ELEMENT *sge = &sgl->Elements[sgeIndex];
        RT_TX_DESC *txd = &tx->TxdBase[tx->TxDescGetptr];
        USHORT status = TXS_OWN;

        // Last TXD; next should wrap
        if (tx->TxDescGetptr == tx->NumTxDesc - 1)
        {
            status |= TXS_EOR;
        }

        // First fragment of packet
        if (sgeIndex == 0)
        {
            status |= TXS_FS;
        }

        // Last fragment of packet
        if (sgeIndex + 1 == sgl->NumberOfElements)
        {
            status |= TXS_LS;

            // Store the hardware descriptor of the last
            // scatter/gather element
            RT_TCB *tcb = GetTcbFromPacketFromToken(packet, tx->TcbToken);
            tcb->LastTxDesc = &tx->TxdBase[tx->TxDescGetptr];
        }

        // TODO: vlan

        txd->BufferAddress = sge->Address;
        txd->TxDescDataIpv6Rss_All.length = (USHORT)sge->Length;
        txd->TxDescDataIpv6Rss_All.VLAN_TAG.Value = 0;
        txd->TxDescDataIpv6Rss_All.OffloadGsoMssTagc = RtGetPacketChecksumSetting(packet);

        MemoryBarrier();
        txd->TxDescDataIpv6Rss_All.status = status;

        tx->TxDescGetptr = (tx->TxDescGetptr + 1) % tx->NumTxDesc;
    }
}

void
EvtSgFlushTransation(_In_ NETTXQUEUE txQueue)
{
    auto tx = RtGetTxQueueContext(txQueue);
    MemoryBarrier();
    *tx->TPPoll = TPPoll_NPQ;
}

NTSTATUS
EvtSgGetPacketStatus(
    _In_ NETTXQUEUE txQueue,
    _In_ NET_PACKET *packet)
{
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);
    RT_TX_DESC *txd = GetTcbFromPacketFromToken(packet, tx->TcbToken)->LastTxDesc;

    // Look at the status flags on the last fragment in the packet.
    // If the hardware-ownership flag is still set, then the packet isn't done.
    if (0 != (txd->TxDescDataIpv6Rss_All.status & TXS_OWN))
        return STATUS_PENDING;

    return STATUS_SUCCESS;
}

NTSTATUS
RtTxQueueInitialize(_In_ NETTXQUEUE txQueue, _In_ RT_ADAPTER * adapter)
{
    NTSTATUS status = STATUS_SUCCESS;
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    tx->Adapter = adapter;

    tx->TcbToken = NET_TXQUEUE_GET_PACKET_CONTEXT_TOKEN(txQueue, RT_TCB);

    tx->TPPoll = &adapter->CSRAddress->TPPoll;
    tx->Interrupt = adapter->Interrupt;
    tx->NumTxDesc = (USHORT)(adapter->NumTcb * RT_MAX_PHYS_BUF_COUNT);

    // Allocate descriptors
    {
        ULONG allocSize;
        GOTO_IF_NOT_NT_SUCCESS(Exit, status,
            RtlULongMult(tx->NumTxDesc, sizeof(RT_TX_DESC), &allocSize));

        GOTO_IF_NOT_NT_SUCCESS(Exit, status,
            WdfCommonBufferCreate(
                tx->Adapter->DmaEnabler,
                allocSize,
                WDF_NO_OBJECT_ATTRIBUTES,
                &tx->TxdArray));

        tx->TxdBase = static_cast<RT_TX_DESC*>(
            WdfCommonBufferGetAlignedVirtualAddress(tx->TxdArray));
        tx->TxDescGetptr = 0;

        RtlZeroMemory(tx->TxdBase, allocSize);
    }

Exit:
    return status;
}

_Use_decl_annotations_
void RtTxQueueStart(_In_ RT_TXQUEUE *tx)
{
    RT_ADAPTER *adapter = tx->Adapter;

    adapter->CSRAddress->TDFNR = 8;

    // Max transmit packet size
    adapter->CSRAddress->MtpsReg.MTPS = (RT_MAX_FRAME_SIZE + 128 - 1) / 128;

    PHYSICAL_ADDRESS pa = WdfCommonBufferGetAlignedLogicalAddress(tx->TxdArray);

    // let hardware know where transmit descriptors are at
    adapter->CSRAddress->TNPDSLow = pa.LowPart;
    adapter->CSRAddress->TNPDSHigh = pa.HighPart;

    adapter->CSRAddress->CmdReg |= CR_TE;

    // data sheet says TCR should only be modified after the transceiver is enabled
    adapter->CSRAddress->TCR = (TCR_RCR_MXDMA_UNLIMITED << TCR_MXDMA_OFFSET) | (TCR_IFG0 | TCR_IFG1);
}

void
RtTxQueueSetInterrupt(_In_ RT_TXQUEUE *tx, _In_ BOOLEAN notificationEnabled)
{
    InterlockedExchange(&tx->Interrupt->TxNotifyArmed, notificationEnabled);
    RtUpdateImr(tx->Interrupt);

    if (!notificationEnabled)
        // block this thread until we're sure any outstanding DPCs are complete.
        // This is to guarantee we don't return from this function call until
        // any oustanding tx notification is complete.
        KeFlushQueuedDpcs();
}

_Use_decl_annotations_
void
EvtTxQueueDestroy(_In_ WDFOBJECT txQueue)
{
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    WdfSpinLockAcquire(tx->Adapter->Lock); {

        tx->Adapter->CSRAddress->CmdReg &= ~CR_TE;

        RtTxQueueSetInterrupt(tx, false);

        tx->Adapter->TxQueue = WDF_NO_HANDLE;

    } WdfSpinLockRelease(tx->Adapter->Lock);

    WdfObjectDelete(tx->TxdArray);
    tx->TxdArray = NULL;
}

_Use_decl_annotations_
VOID
EvtTxQueueSetNotificationEnabled(
    _In_ NETTXQUEUE txQueue,
    _In_ BOOLEAN notificationEnabled)
{
    TraceEntry(TraceLoggingPointer(txQueue), TraceLoggingBoolean(notificationEnabled));

    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);

    RtTxQueueSetInterrupt(tx, notificationEnabled);

    TraceExit();
}

_Use_decl_annotations_
void
EvtTxQueueCancel(
    _In_ NETTXQUEUE txQueue)
{
    TraceEntry(TraceLoggingPointer(txQueue, "TxQueue"));

    //
    // If the chipset is able to cancel outstanding IOs, then it should do so
    // here. However, the RTL8168D does not seem to support such a feature, so
    // the queue will continue to be drained like normal.
    //

    TraceExit();
}
