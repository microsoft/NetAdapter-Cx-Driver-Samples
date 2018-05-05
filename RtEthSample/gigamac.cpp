/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "gigamac.h"
#include "adapter.h"
#include "rxqueue.h"

#define GIGAMAC_WRITE 0x80000000
#define GIGAMAC_READ  0x00000000
#define GIGAMAC_READ_DONE(access) (((access) & 0x80000000) != 0)
#define GIGAMAC_WRITE_DONE(access) (((access) & 0x80000000) == 0)

#define GIGAMAC_BYTES_4 0x0000f000
#define GIGAMAC_WRITE_4 (GIGAMAC_WRITE | GIGAMAC_BYTES_4)

#define GIGAMAC_WAIT_TIME 100 // 100us
#define GIGAMAC_WAIT_EXIT_TIME 20 // 20us
#define GIGAMAC_WAIT_COUNT 10

#define GIGAMAC_RSS_KEY_DW 0x0090
#define GIGAMAC_RSS_CONTROL 0x00b8
#define GIGAMAC_RDSAR1 0x00d0
#define GIGAMAC_RDSAR2 0x00d8
#define GIGAMAC_RDSAR3 0x00e0

static
bool
GigaMacWrite(
    _In_ RT_ADAPTER *adapter,
    _In_ UINT32 access,
    _In_ UINT32 data
    )
{
    NT_ASSERT(! adapter->GigaMacInUse);

    adapter->GigaMacInUse = true;
    adapter->CSRAddress->ERIData = data;
    adapter->CSRAddress->ERIAccess = access;

    for (size_t count = 0; count < GIGAMAC_WAIT_COUNT; count++)
    {
        KeStallExecutionProcessor(GIGAMAC_WAIT_TIME);

        if (GIGAMAC_WRITE_DONE(adapter->CSRAddress->ERIAccess))
        {
            KeStallExecutionProcessor(GIGAMAC_WAIT_EXIT_TIME);
            adapter->GigaMacInUse = false;

            return true;
        }
    }
    adapter->GigaMacInUse = false;

    return false;
}

static
bool
GigaMacWrite4(
    _In_ RT_ADAPTER *adapter,
    _In_ UINT16 address,
    _In_ UINT32 data
    )
{
    // address shall not specify bits indicating part of the write size
    NT_ASSERT((0xf000 & address) == 0);
    // address shall not specify lower two bits (double word alignment)
    NT_ASSERT((0x0003 & address) == 0);

    return GigaMacWrite(adapter, GIGAMAC_WRITE_4 | address, data);
}

_Use_decl_annotations_
bool
GigaMacSetReceiveDescriptorStartAddress(
    RT_ADAPTER *adapter,
    ULONG queueId,
    PHYSICAL_ADDRESS const physicalAddress
    )
{
    NT_FRE_ASSERTMSG("GigaMac cannot be used with QueueId", ! (queueId < 1));
    NT_FRE_ASSERTMSG("GigaMac cannot be used with QueueId", ! (queueId > 3));

    UINT16 addressLow = 0U;
    switch (queueId)
    {

    case 1:
        addressLow = GIGAMAC_RDSAR1;
        break;

    case 2:
        addressLow = GIGAMAC_RDSAR2;
        break;

    case 3:
        addressLow = GIGAMAC_RDSAR3;
        break;
    }

    UINT16 addressHigh = addressLow + sizeof(adapter->CSRAddress->ERIAccess);
    return GigaMacWrite4(adapter, addressLow, physicalAddress.LowPart) &&
        GigaMacWrite4(adapter, addressHigh, physicalAddress.HighPart);
}

_Use_decl_annotations_
bool
GigaMacRssSetHashSecretKey(
    RT_ADAPTER *adapter,
    const UINT32 hashSecretKey[],
    size_t hashSecretKeySize
    )
{
    UINT32 address = GIGAMAC_RSS_KEY_DW;
    for (size_t i = 0; i < hashSecretKeySize; i++, address += sizeof(address))
    {
        NT_ASSERT((0xffff0000 & address) == 0);
        if (! GigaMacWrite4(adapter, (UINT16)address, hashSecretKey[i]))
        {
            return false;
        }
    }

    return true;
}


_Use_decl_annotations_
bool
GigaMacRssSetControl(
    RT_ADAPTER *adapter,
    UINT32 controlHashMultiCpu
    )
{
    return GigaMacWrite4(adapter, GIGAMAC_RSS_CONTROL, controlHashMultiCpu);
}

