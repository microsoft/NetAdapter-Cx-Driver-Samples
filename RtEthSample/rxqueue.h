/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

struct RT_RXQUEUE
{
    RT_ADAPTER *Adapter;
    RT_INTERRUPT *Interrupt;

    NET_RING_COLLECTION const * Rings;

    WDFCOMMONBUFFER RxdArray;
    RT_RX_DESC *RxdBase;
    size_t RxdSize;

    NET_EXTENSION ChecksumExtension;
    NET_EXTENSION LogicalAddressExtension;

    ULONG QueueId;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_RXQUEUE, RtGetRxQueueContext);

NTSTATUS RtRxQueueInitialize(_In_ NETPACKETQUEUE rxQueue, _In_ RT_ADAPTER * adapter);

_Requires_lock_held_(adapter->Lock)
void RtAdapterUpdateRcr(_In_ RT_ADAPTER *adapter);

EVT_WDF_OBJECT_CONTEXT_DESTROY EvtRxQueueDestroy;

EVT_PACKET_QUEUE_SET_NOTIFICATION_ENABLED EvtRxQueueSetNotificationEnabled;
EVT_PACKET_QUEUE_ADVANCE EvtRxQueueAdvance;
EVT_PACKET_QUEUE_CANCEL EvtRxQueueCancel;
EVT_PACKET_QUEUE_START EvtRxQueueStart;
EVT_PACKET_QUEUE_STOP EvtRxQueueStop;
