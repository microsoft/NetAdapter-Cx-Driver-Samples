/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

void RtAdapterDumpStatsCounters(
    _In_ RT_ADAPTER *adapter);

NTSTATUS
RtAdapterInitializeStatistics(
    _In_ RT_ADAPTER *adapter);

void
RtAdapterResetStatistics(
    _In_ RT_ADAPTER *adapter);

EVT_NET_REQUEST_QUERY_DATA EvtNetRequestQueryAllStatistics;
EVT_NET_REQUEST_QUERY_DATA EvtNetRequestQueryIndividualStatistics;