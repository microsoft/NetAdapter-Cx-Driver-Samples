/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "eeprom.h"
#include "adapter.h"

// RTL8168D <-> 93C46 EEPROM
#define EEPROM_EESK_WAIT 16 // 16us
#define EEPROM_OPCODE_BITS 3
#define EEPROM_ADDRESS_BITS 6
#define EEPROM_READ 6 // 110
#define EEPROM_ETHERNET_WORD 7

static
void
ClockRaise(const RT_ADAPTER *adapter)
{
    UCHAR ucCr9346Value = adapter->CSRAddress->CR9346;
    ucCr9346Value |= CR9346_EESK;
    adapter->CSRAddress->CR9346 = ucCr9346Value;
    KeStallExecutionProcessor(EEPROM_EESK_WAIT);
}

static
void
ClockLower(const RT_ADAPTER *adapter)
{
    UCHAR ucCr9346Value = adapter->CSRAddress->CR9346;
    ucCr9346Value &= ~CR9346_EESK;
    adapter->CSRAddress->CR9346 = ucCr9346Value;
    KeStallExecutionProcessor(EEPROM_EESK_WAIT);
}

static
USHORT
ShiftBitsIn(const RT_ADAPTER *adapter)
{
    USHORT data = 0;
    for (size_t i = 0; i < sizeof(data) * 8; i++)
    {
        data = data << 1;

        ClockRaise(adapter);

        UCHAR ucCr9346Value = adapter->CSRAddress->CR9346;
        ucCr9346Value &= ~CR9346_EEDI;
        if (ucCr9346Value & CR9346_EEDO) {
            data |= 1;
        }

        ClockLower(adapter);
    }

    return data;
}

static
void
ShiftBitsOut(const RT_ADAPTER *adapter, USHORT data, size_t count)
{
    // 93C46, Start (1) + Opcode (2) + Address (6)
    NT_VERIFY(count != 0);
    NT_VERIFY(count <= 9);

    USHORT mask = 1 << (count - 1);

    do
    {
        UCHAR ucCr9346Value = adapter->CSRAddress->CR9346;
        ucCr9346Value &= ~CR9346_EEDI;
        if (data & mask)
        {
            ucCr9346Value |= CR9346_EEDI;
        }
        adapter->CSRAddress->CR9346 = ucCr9346Value;

        ClockRaise(adapter);
        ClockLower(adapter);

        mask = mask >> 1;

    } while (mask);
}

static
USHORT
ReadEEPROM(const RT_ADAPTER *adapter, USHORT address)
{
    UCHAR ucCr9346Value = adapter->CSRAddress->CR9346;

    // operating mode: programming & lower chip select
    ucCr9346Value = CR9346_EEM1;
    adapter->CSRAddress->CR9346 = ucCr9346Value;

    // complete any in progress cycle
    ClockRaise(adapter);
    ClockLower(adapter);

    // raise chip select
    ucCr9346Value &= ~(CR9346_EEM0 | CR9346_EEDI);
    ucCr9346Value |= CR9346_EEM1 | CR9346_EECS;
    adapter->CSRAddress->CR9346 = ucCr9346Value;

    ShiftBitsOut(adapter, EEPROM_READ, EEPROM_OPCODE_BITS);
    ShiftBitsOut(adapter, address, EEPROM_ADDRESS_BITS);
    USHORT data = ShiftBitsIn(adapter);

    // lower chip select
    ucCr9346Value &= ~(CR9346_EEDI | CR9346_EECS);

    // complete any in progress cycle
    ClockRaise(adapter);
    ClockLower(adapter);

    return data;
}

_Use_decl_annotations_
void
RtAdapterReadEEPROMPermanentAddress(RT_ADAPTER *adapter)
/*++
Routine Description:

    Read the permanent mac addresss from the eeprom

Arguments:

    adapter     Pointer to our adapter

--*/
{
    NT_ASSERT(!adapter->EEPROMInUse);

    adapter->EEPROMInUse = true;
    const UCHAR ucCr9346Saved = adapter->CSRAddress->CR9346;

    for (USHORT i = 0; i < ETHERNET_ADDRESS_LENGTH / sizeof(USHORT); i++)
    {
        USHORT word = ReadEEPROM(adapter, i + EEPROM_ETHERNET_WORD);
        adapter->PermanentAddress.Address[sizeof(word) * i] = word & 0xff;
        adapter->PermanentAddress.Address[sizeof(word) * i + 1] = (word>>8) & 0xff;
    }
    adapter->PermanentAddress.Length = ETHERNET_ADDRESS_LENGTH;

    adapter->CSRAddress->CR9346 = ucCr9346Saved;
    adapter->EEPROMInUse = false;
}

_Use_decl_annotations_
bool
RtAdapterReadEepromId(
    _In_ RT_ADAPTER *adapter,
    _Out_ UINT16 *id,
    _Out_ UINT16 *pciId)
{
    NT_ASSERT(!adapter->EEPROMInUse);

    adapter->EEPROMInUse = true;

    const UCHAR ucCr9346Saved = adapter->CSRAddress->CR9346;

    const USHORT idD0 = ReadEEPROM(adapter, 0x00);
    const USHORT idD1 = ReadEEPROM(adapter, 0x01);

    //normal mode
    adapter->CSRAddress->CR9346 = ucCr9346Saved;

    KeStallExecutionProcessor(EEPROM_EESK_WAIT);

    if (idD0 == 0 && idD1 == 0)
    {
        adapter->EEPROMInUse = false;

        return false;
    }

    *id = idD0;
    *pciId = idD1;

    adapter->EEPROMInUse = false;

    return true;
}
