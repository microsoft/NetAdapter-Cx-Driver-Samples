/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

//--------------------------------------
// TCB (Transmit Control Block)
//--------------------------------------

typedef struct _RT_TCB
{
    USHORT FirstTxDescIdx;
    ULONG NumTxDesc;
} RT_TCB;

typedef struct _RT_TXQUEUE
{
    RT_ADAPTER *Adapter;
    RT_INTERRUPT *Interrupt;

    NET_RING_COLLECTION const * Rings;
    RT_TCB* PacketContext;

    // descriptor information
    WDFCOMMONBUFFER TxdArray;
    RT_TX_DESC *TxdBase;
    size_t TxSize;

    USHORT NumTxDesc;
    USHORT TxDescIndex;

    UCHAR volatile *TPPoll;

    NET_EXTENSION ChecksumExtension;
    NET_EXTENSION LsoExtension;
} RT_TXQUEUE;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_TXQUEUE, RtGetTxQueueContext);

NTSTATUS RtTxQueueInitialize(_In_ NETPACKETQUEUE txQueue, _In_ RT_ADAPTER *adapter);

_Requires_lock_held_(tx->Adapter->Lock)
void RtTxQueueStart(_In_ RT_TXQUEUE *tx);

EVT_WDF_OBJECT_CONTEXT_DESTROY EvtTxQueueDestroy;

EVT_PACKET_QUEUE_SET_NOTIFICATION_ENABLED EvtTxQueueSetNotificationEnabled;
EVT_PACKET_QUEUE_ADVANCE EvtTxQueueAdvance;
EVT_PACKET_QUEUE_CANCEL EvtTxQueueCancel;
EVT_PACKET_QUEUE_START EvtTxQueueStart;
EVT_PACKET_QUEUE_STOP EvtTxQueueStop;
