/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "trace.h"
#include "phy.h"
#include "adapter.h"

void
RtAdapterWritePhyUint16(_In_ RT_ADAPTER *adapter, UCHAR regAddr, USHORT regData)
{
    adapter->CSRAddress->PhyAccessReg = PHYAR_Flag | ((ULONG)regAddr << 16) | (ULONG)regData;

    ULONG accessReg = adapter->CSRAddress->PhyAccessReg;
    for (ULONG timeout = 0; timeout < 20; timeout++)
    {
        if (!(accessReg & PHYAR_Flag))
            break;

        KeStallExecutionProcessor(50);

        accessReg = adapter->CSRAddress->PhyAccessReg;
    }

    KeStallExecutionProcessor(20);
}

USHORT
RtAdapterReadPhyUint16(
    RT_ADAPTER *adapter,
    UCHAR regAddr)
{
    USHORT RetVal = USHORT_MAX;

    adapter->CSRAddress->PhyAccessReg = ((ULONG)regAddr << 16);

    ULONG accessReg = adapter->CSRAddress->PhyAccessReg;
    for (ULONG timeout = 0; timeout < 20; timeout++)
    {
        if (accessReg & PHYAR_Flag)
        {
            RetVal = (USHORT)(accessReg & 0x0000ffff);
            break;
        }

        KeStallExecutionProcessor(50);

        accessReg = adapter->CSRAddress->PhyAccessReg;
    }

    KeStallExecutionProcessor(20);

    return RetVal;
}