/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "statistics.h"
#include "trace.h"
#include "adapter.h"

bool
RtTryUpdateStats(_In_ RT_ADAPTER *adapter)
{
    adapter->CSRAddress->DTCCRHigh = adapter->TallyPhy.HighPart;
    adapter->CSRAddress->DTCCRLow = adapter->TallyPhy.LowPart | DTCCR_Cmd;

    for (UINT i = 1; i <= 20; i++)
    {
        if (DTCCR_Cmd != (adapter->CSRAddress->DTCCRLow & DTCCR_Cmd))
            return true;

        KeStallExecutionProcessor(i);
    }

    return false;
}

void
RtAdapterDumpStatsCounters(
    _In_ RT_ADAPTER *adapter)
{
    if (RtTryUpdateStats(adapter))
    {
        adapter->HwTotalRxMatchPhy = adapter->GTally->RxOKPhy;
        adapter->HwTotalRxBroadcast = adapter->GTally->RxOKBrd;
        adapter->HwTotalRxMulticast = adapter->GTally->RxOKMul;
        adapter->TotalTxErr = adapter->GTally->TxERR;
        adapter->TotalRxErr = adapter->GTally->RxERR;
        adapter->RcvResourceErrors = adapter->GTally->MissPkt;
        adapter->TxOneRetry = adapter->GTally->Tx1Col;
        adapter->TxMoreThanOneRetry = adapter->GTally->TxMCol;
        adapter->TxAbortExcessCollisions = adapter->GTally->TxAbt;
        adapter->TxDmaUnderrun = adapter->GTally->TxUndrn;
    }
    else
    {
        TraceLoggingWrite(RealtekTraceProvider, "StatsUpdateFailed");
    }
}

NTSTATUS
RtAdapterInitializeStatistics(
    _In_ RT_ADAPTER *adapter)
/*++
Routine Description:

    Allocate all the memory blocks for send, receive and others

Arguments:

    adapter     Pointer to our adapter

Return Value:

    STATUS_SUCCESS
    STATUS_FAILURE
    STATUS_INSUFFICIENT_RESOURCES

--*/
{
    TraceEntryRtAdapter(adapter);

    NTSTATUS status = STATUS_SUCCESS;

    // Allocate memory for Tally counter
    WDFCOMMONBUFFER HwTallyMemAlloc = WDF_NO_HANDLE;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfCommonBufferCreate(
            adapter->DmaEnabler,
            sizeof(RT_TALLY),
            WDF_NO_OBJECT_ATTRIBUTES,
            &HwTallyMemAlloc));

    adapter->GTally = static_cast<RT_TALLY*>(WdfCommonBufferGetAlignedVirtualAddress(HwTallyMemAlloc));
    adapter->TallyPhy = WdfCommonBufferGetAlignedLogicalAddress(HwTallyMemAlloc);

    RtlZeroMemory(adapter->GTally, sizeof(*adapter->GTally));

    adapter->CSRAddress->DTCCRHigh = adapter->TallyPhy.HighPart;
    adapter->CSRAddress->DTCCRLow = adapter->TallyPhy.LowPart;

    // Clear the internal counters
    RtAdapterResetStatistics(adapter);

Exit:
    TraceExitResult(status);
    return status;
}

void
RtAdapterResetStatistics(
    _In_ RT_ADAPTER *adapter)
    /*++
    Routine Description:

    This routine will clear the hardware error statistic counters

    Arguments:

    NetAdapter     Pointer to our adapter

    --*/
{
    adapter->CSRAddress->DTCCRLow = adapter->TallyPhy.LowPart | DTCCR_Clr;
}

_Use_decl_annotations_
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
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

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

_Use_decl_annotations_
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
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

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