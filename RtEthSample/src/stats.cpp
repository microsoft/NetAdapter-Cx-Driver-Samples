/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "stats.h"
#include "trace.h"

bool
RtTryUpdateStats(_In_ MP_ADAPTER *adapter)
{
    adapter->CSRAddress->DTCCRHigh = NdisGetPhysicalAddressHigh(adapter->TallyPhy);
    adapter->CSRAddress->DTCCRLow = DTCCR_Cmd | NdisGetPhysicalAddressLow(adapter->TallyPhy);

    for (UINT i = 1; i <= 20; i++)
    {
        if (DTCCR_Cmd != (adapter->CSRAddress->DTCCRLow & DTCCR_Cmd))
            return true;

        NdisStallExecution(i);
    }

    return false;
}

void
RtAdapterDumpStatsCounters(
    _In_ MP_ADAPTER *adapter)
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
