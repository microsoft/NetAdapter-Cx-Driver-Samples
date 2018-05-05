/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

#define TX_DMA_FX_ALLOC_TAG 'xTtR'

#include "txdmafxtypes.h"

EVT_TX_DMA_QUEUE_PROGRAM_DESCRIPTORS EvtSgProgramDescriptors;
EVT_TX_DMA_QUEUE_FLUSH_TRANSACTION EvtSgFlushTransation;
EVT_TX_DMA_QUEUE_GET_PACKET_STATUS  EvtSgGetPacketStatus;
EVT_TX_DMA_QUEUE_BOUNCE_ANALYSIS EvtSgBounceAnalysis;

#define TX_DMA_FX_PROGRAM_DESCRIPTORS EvtSgProgramDescriptors
#define TX_DMA_FX_GET_PACKET_STATUS EvtSgGetPacketStatus
#define TX_DMA_FX_FLUSH_TRANSACTION EvtSgFlushTransation
#define TX_DMA_FX_BOUNCE_ANALYSIS EvtSgBounceAnalysis

#include "txdmafx.h"

typedef struct _RT_TXQUEUE
{
    RT_ADAPTER *Adapter;
    RT_INTERRUPT *Interrupt;

    PCNET_DATAPATH_DESCRIPTOR DatapathDescriptor;
    NET_PACKET_CONTEXT_TOKEN *TcbToken;

    // descriptor information
    WDFCOMMONBUFFER TxdArray;
    RT_TX_DESC *TxdBase;

    USHORT NumTxDesc;
    USHORT TxDescGetptr;

    UCHAR volatile *TPPoll;

    size_t ChecksumExtensionOffSet;
    size_t LsoExtensionOffset;
} RT_TXQUEUE;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_TXQUEUE, RtGetTxQueueContext);


//--------------------------------------
// TCB (Transmit Control Block)
//--------------------------------------

typedef struct _RT_TCB
{
    USHORT FirstTxDescIdx;
    ULONG NumTxDesc;
} RT_TCB;

NET_PACKET_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_TCB, GetTcbFromPacket);

NTSTATUS RtTxQueueInitialize(_In_ NETTXQUEUE txQueue, _In_ RT_ADAPTER *adapter);

_Requires_lock_held_(tx->Adapter->Lock)
void RtTxQueueStart(_In_ RT_TXQUEUE *tx);

EVT_WDF_OBJECT_CONTEXT_DESTROY EvtTxQueueDestroy;
EVT_TXQUEUE_SET_NOTIFICATION_ENABLED EvtTxQueueSetNotificationEnabled;
EVT_TXQUEUE_CANCEL EvtTxQueueCancel;
