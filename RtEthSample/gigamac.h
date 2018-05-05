/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

struct RT_RXQUEUE;

bool
GigaMacSetReceiveDescriptorStartAddress(
    _In_ RT_ADAPTER *adapter,
    _In_ ULONG queueId,
    _In_ PHYSICAL_ADDRESS const physicalAddress
    );

bool
GigaMacRssSetHashSecretKey(
    _In_ RT_ADAPTER *adapter,
    _In_reads_(secretHashKeySize) const UINT32 secretHashKey[],
    _In_ size_t secretHashKeySize
    );

bool
GigaMacRssSetControl(
    _In_ RT_ADAPTER *adapter,
    _In_ UINT32 controlHashMultiCpu
    );

