/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

NTSTATUS
RtInitializeAdapterRequestQueue(
    _In_ MP_ADAPTER *adapterContext);

EVT_NET_REQUEST_DEFAULT_SET_DATA EvtAdapterGenericSetInformation;

VOID
MPSetPowerD0(
    _In_ PMP_ADAPTER Adapter
    );

VOID
MPSetPowerLow(
    _In_ PMP_ADAPTER Adapter
    );