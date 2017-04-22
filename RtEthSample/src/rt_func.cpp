/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#define _FILENUMBER     'UFTR'

VOID
MP_RealWritePhyUshort(
    PMP_ADAPTER Adapter,
    UCHAR   RegAddr,
    USHORT  RegData)
{
    ULONG TmpUlong = 0x80000000;
    ULONG Timeout = 0, WaitCount = 10;

    TmpUlong |= (((ULONG) RegAddr << 16) | (ULONG) RegData);

    Adapter->CSRAddress->PhyAccessReg = TmpUlong ;

    do
    {
        NdisStallExecution(100);

        TmpUlong = Adapter->CSRAddress->PhyAccessReg;

        Timeout++;
    }
    while ((TmpUlong & PHYAR_Flag) && (Timeout < WaitCount));


    NdisStallExecution(20);
}



VOID
MP_WritePhyUshort(
    PMP_ADAPTER Adapter,
    UCHAR   RegAddr,
    USHORT  RegData)
{
    MP_RealWritePhyUshort(Adapter, RegAddr, RegData);
}

USHORT
MP_ReadPhyUshort(
    PMP_ADAPTER Adapter,
    UCHAR   RegAddr)
{
    USHORT RegData;
    ULONG TmpUlong = 0x00000000;
    ULONG Timeout = 0, WaitCount = 10;
    USHORT RetVal = 0xffff;

    TmpUlong |= ((ULONG) RegAddr << 16);

    Adapter->CSRAddress->PhyAccessReg = TmpUlong ;
    do
    {
        NdisStallExecution(100);

        TmpUlong = Adapter->CSRAddress->PhyAccessReg;

        Timeout++;
    }
    while ((!(TmpUlong & PHYAR_Flag)) && (Timeout < WaitCount));

    if (Timeout == WaitCount && !(TmpUlong & PHYAR_Flag))
    {

    }
    else
    {
        TmpUlong = Adapter->CSRAddress->PhyAccessReg;

        RegData = (USHORT) (TmpUlong & 0x0000ffff);

        RetVal = RegData;
    }

    NdisStallExecution(20);

    return RetVal;
}

BOOLEAN CheckMacVersion(PMP_ADAPTER Adapter)
{
    ULONG MacVer;
    ULONG RevId;
    ULONG TcrValue;

    TcrValue = Adapter->CSRAddress->TCR;

    MacVer = TcrValue;
    RevId = TcrValue;
    MacVer &= 0x7C800000;
    RevId &= (BIT_20 | BIT_21 | BIT_22);

    Adapter->ChipType = RTLUNKNOWN;

    do
    {
        if (MacVer == 0x28000000)
        {
            Adapter->ChipType = RTL8168D;

            DBGPRINT(MP_WARN, ("Gig Chip Type is RTL8168D\n"));
            break;
        }
    }
    while(FALSE);


    if (Adapter->ChipType == RTLUNKNOWN)
    {
        return FALSE;
    }

    return TRUE;
}

VOID
CardWriteEthernetAddress(
    IN PMP_ADAPTER Adapter
)

/*++

Routine Description:

Write back our Ethernet address when driver is unloaded.

Arguments:

Adapter - pointer to the adapter block.

Return Value:

The address is stored in Adapter->PermanentAddress, and StationAddress if it
is currently zero.

--*/

{
    ULONG TempUlong;
    PULONG idReg;

    //
    // if the MAC address does not overrided, direct return
    //

    if (Adapter->PermanentAddress[0] == Adapter->CurrentAddress[0] &&
            Adapter->PermanentAddress[1] == Adapter->CurrentAddress[1] &&
            Adapter->PermanentAddress[2] == Adapter->CurrentAddress[2] &&
            Adapter->PermanentAddress[3] == Adapter->CurrentAddress[3] &&
            Adapter->PermanentAddress[4] == Adapter->CurrentAddress[4] &&
            Adapter->PermanentAddress[5] == Adapter->CurrentAddress[5])
    {
        return ;
    }

    EnableCR9346Write(Adapter);

    TempUlong = Adapter->PermanentAddress[3];
    TempUlong = TempUlong << 8;
    TempUlong = TempUlong + Adapter->PermanentAddress[2];
    TempUlong = TempUlong << 8;
    TempUlong = TempUlong + Adapter->PermanentAddress[1];
    TempUlong = TempUlong << 8;
    TempUlong = TempUlong + Adapter->PermanentAddress[0];

    idReg = (PULONG) & Adapter->CSRAddress->ID0;
    *idReg = TempUlong;

    TempUlong = Adapter->PermanentAddress[5];
    TempUlong = TempUlong << 8;
    TempUlong = TempUlong + Adapter->PermanentAddress[4];

    idReg = (PULONG) & Adapter->CSRAddress->ID4;
    *idReg = TempUlong;

    DisableCR9346Write(Adapter);

}

VOID
MPSetPowerLowPrivateRealtek(
    PMP_ADAPTER Adapter
)
{
    //
    // Stop hardware from sending & receiving packets
    //
    RtIssueFullReset(Adapter);
}


VOID
MPSetPowerD0PrivateRealtek(
    PMP_ADAPTER Adapter
)
{
    SetupHwAfterIdentifyChipType(Adapter);

    RtIssueFullReset(Adapter);

    PhyPowerUp(Adapter);

    ResetPhy(Adapter);

    RtIssueFullReset(Adapter);

    WdfSpinLockAcquire(Adapter->Lock); {

        HwSetupIAAddress(Adapter);

    } WdfSpinLockRelease(Adapter->Lock);

    SetupPhyAutoNeg(Adapter);

    PhyRestartNway (Adapter);
}

/*++

Routine Description:

Runs the AUTODIN II CRC algorithm on buffer Buffer of
length Length.

Arguments:

Buffer - the input buffer

Length - the length of Buffer

Return Value:

The 32-bit CRC value.

Note:

This is adapted from the comments in the assembly language
version in _GENREQ.ASM of the DWB NE1000/2000 driver.

--*/
ULONG
ComputeCrc(
    IN PUCHAR Buffer,
    IN UINT Length
)
{
    ULONG Crc, Carry;
    UINT i, j;
    UCHAR CurByte;

    Crc = 0xffffffff;

    for (i = 0; i < Length; i++)
    {
        CurByte = Buffer[i];

        for (j = 0; j < 8; j++)
        {
            Carry = ((Crc & 0x80000000) ? 1 : 0) ^ (CurByte & 0x01);
            Crc <<= 1;
            CurByte >>= 1;

            if (Carry)
            {
                Crc = (Crc ^ 0x04c11db6) | Carry;
            }
        }
    }

    return Crc;
}

/*++

Routine Description:

For a given multicast address, returns the byte and bit in
the card multicast registers that it hashes to. Calls
CardComputeCrc() to determine the CRC value.

Arguments:

Address - the address

Byte - the byte that it hashes to

Value - will have a 1 in the relevant bit

Return Value:

None.

--*/
VOID
GetMulticastBit(
    IN UCHAR Address[ETH_LENGTH_OF_ADDRESS],
    OUT UCHAR * Byte,
    OUT UCHAR * Value
)
{
    ULONG Crc;
    UINT BitNumber;

    Crc = ComputeCrc(Address, ETH_LENGTH_OF_ADDRESS);

    // The bit number is now in the 6 most significant bits of CRC.
    BitNumber = (UINT) ((Crc >> 26) & 0x3f);
    *Byte = (UCHAR) (BitNumber / 8);
    *Value = (UCHAR) ((UCHAR) 1 << (BitNumber % 8));
}

#pragma warning(default:28167)

USHORT ChkSumToHwFormat(PNDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO pChkSumInfo)
{
    USHORT chksum = 0;

    if (pChkSumInfo->Transmit.IsIPv4)
    {
        if (pChkSumInfo->Transmit.IpHeaderChecksum)
        {
            chksum = TXS_IPV6RSS_IPV4CS;
        }

        if (pChkSumInfo->Transmit.TcpChecksum)
        {
            chksum = TXS_IPV6RSS_TCPCS | TXS_IPV6RSS_IPV4CS;
        }

        if (pChkSumInfo->Transmit.UdpChecksum)
        {
            chksum = TXS_IPV6RSS_UDPCS | TXS_IPV6RSS_IPV4CS;
        }
    }

    if (pChkSumInfo->Transmit.IsIPv6)
    {
        if (pChkSumInfo->Transmit.IpHeaderChecksum)
        {
            //
            // IPV6 does not require to calc the checksum
            //
        }

        if (pChkSumInfo->Transmit.TcpChecksum)
        {
            chksum = TXS_IPV6RSS_TCPCS | TXS_IPV6RSS_IS_IPV6 | (USHORT) (pChkSumInfo->Transmit.TcpHeaderOffset << TXS_IPV6RSS_TCPHDR_OFFSET);
        }

        if (pChkSumInfo->Transmit.UdpChecksum)
        {
            chksum = TXS_IPV6RSS_UDPCS | TXS_IPV6RSS_IS_IPV6 | (USHORT) (pChkSumInfo->Transmit.TcpHeaderOffset << TXS_IPV6RSS_TCPHDR_OFFSET);
        }
    }

    return chksum;
}

VOID PhyPowerUp(IN PMP_ADAPTER Adapter)
{
    MP_WritePhyUshort(Adapter, 0x1F, 0x0000);
    MP_WritePhyUshort(Adapter, 0x0e, 0x0000);
    MP_WritePhyUshort(Adapter, PHY_REG_BMCR, MDI_CR_AUTO_SELECT);
}

VOID SetupHwAfterIdentifyChipType(PMP_ADAPTER Adapter)
{
    UCHAR TmpUchar;

    TmpUchar = Adapter->CSRAddress->REG_F0_F3.RESV_F3;
    TmpUchar |= BIT_2;
    Adapter->CSRAddress->REG_F0_F3.RESV_F3 = TmpUchar;

    TmpUchar = Adapter->CSRAddress->REG_D0_D3.RESV_D1;
    TmpUchar |= (BIT_7 | BIT_1);
    Adapter->CSRAddress->REG_D0_D3.RESV_D1 = TmpUchar;
}

VOID
SetConfigBit(
    _In_ PMP_ADAPTER MpAdapter,
    _In_ ULONG       Offset,
    _In_ UCHAR       Mask
    )
{
    PUCHAR  configAddr;
    UCHAR   tmpUchar;

    //
    // it must enable cr9346 before writing the config regsters
    //
    EnableCR9346Write(MpAdapter); {

        configAddr = &MpAdapter->CSRAddress->CONFIG0;
        configAddr += Offset;
        tmpUchar = *configAddr;
        tmpUchar |= Mask;
        *configAddr = tmpUchar;

    } DisableCR9346Write(MpAdapter);
}

VOID
ClearConfigBit(
    _Inout_ PMP_ADAPTER MpAdapter,
    _In_    ULONG       Offset,
    _In_    UCHAR       Mask
    )
{
    PUCHAR  configAddr;
    UCHAR   tmpUchar;

    //
    // it must enable cr9346 before writing the config regsters
    //

    EnableCR9346Write(MpAdapter); {

        configAddr = &MpAdapter->CSRAddress->CONFIG0;
        configAddr += Offset;
        tmpUchar = *configAddr;
        tmpUchar &= ~Mask;
        *configAddr = tmpUchar;

    } DisableCR9346Write(MpAdapter);
}

VOID
EnableMagicPacket(
    _In_ PMP_ADAPTER MpAdapter
    )
{
    SetConfigBit(MpAdapter, NIC_CONFIG3_REG_OFFSET, CONFIG3_Magic);
}

VOID
DisableMagicPacket(
    _In_ PMP_ADAPTER MpAdapter
    )
{
    ClearConfigBit(MpAdapter, NIC_CONFIG3_REG_OFFSET, CONFIG3_Magic);
}
