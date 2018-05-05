/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

// Lock not required during power-up/power-down
_Requires_lock_held_(adapter->Lock)
void
RtAdapterReadEEPROMPermanentAddress(_In_ RT_ADAPTER *adapter);

_Requires_lock_held_(adapter->Lock)
bool
RtAdapterReadEepromId(
    _In_ RT_ADAPTER *adapter,
    _Out_ UINT16 *id,
    _Out_ UINT16 *pciId);
