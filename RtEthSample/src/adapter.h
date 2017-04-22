/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MP_ADAPTER, RtGetAdapterContext);

inline
NTSTATUS
RtConvertNdisStatusToNtStatus(
    _In_ NDIS_STATUS ndisStatus)
{
    if (ndisStatus == NDIS_STATUS_BUFFER_TOO_SHORT)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }
    else if (ndisStatus == NDIS_STATUS_REQUEST_ABORTED)
    {
        return STATUS_CANCELLED;
    }
    else
    {
        return (NTSTATUS)ndisStatus;
    }
}

NTSTATUS
RtInitializeAdapterContext(
    _In_ PMP_ADAPTER adapterContext,
    _In_ WDFDEVICE device,
    _In_ NETADAPTER adapter);

void
RtAdapterUpdatePmParameters(
    _In_ PMP_ADAPTER adapter);

void
RtAdapterQueryOffloadConfiguration(
    _In_  MP_ADAPTER const *adapter,
    _Out_ NDIS_OFFLOAD *offloadCaps);

_Requires_lock_held_(adapter->Lock)
void
RtAdapterUpdateEnabledChecksumOffloads(
    _In_ MP_ADAPTER *adapter);

EVT_NET_ADAPTER_SET_CAPABILITIES EvtAdapterSetCapabilities;
EVT_NET_ADAPTER_CREATE_TXQUEUE   EvtAdapterCreateTxQueue;
EVT_NET_ADAPTER_CREATE_RXQUEUE   EvtAdapterCreateRxQueue;

EVT_WDF_DEVICE_CONTEXT_DESTROY   RtDestroyAdapterContext;
