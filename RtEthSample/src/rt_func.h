/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#ifndef _RT_FUNC_H
#define _RT_FUNC_H

VOID
MP_WritePhyUshort(
    PMP_ADAPTER Adapter,
    UCHAR   RegAddr,
    USHORT  RegData);

USHORT
MP_ReadPhyUshort(
    PMP_ADAPTER Adapter,
    UCHAR   RegAddr);

BOOLEAN CheckMacVersion(PMP_ADAPTER Adapter);

VOID SetupHwAfterIdentifyChipType(PMP_ADAPTER Adapter);

VOID
CardWriteEthernetAddress(
    IN PMP_ADAPTER Adapter
);

VOID
MPSetPowerLowPrivateRealtek(
    PMP_ADAPTER Adapter
);

VOID
MPSetPowerD0PrivateRealtek(
    PMP_ADAPTER Adapter
);

VOID GetMulticastBit(
    IN UCHAR Address[],
    OUT UCHAR * Byte,
    OUT UCHAR * Value
);

void SetupPhyAutoNeg(IN PMP_ADAPTER Adapter);

__inline BOOLEAN IsHighPriorityPkt(PNET_BUFFER_LIST NetBufferList)
{
    /*
    0         Default, assumed to be Best Effort
    1         reserved, "less than" Best Effort
    2         reserved
    3         reserved
    4         Delay Sensitive, no bound
    5         Delay Sensitive, 100ms bound
    6         Delay Sensitive, 10ms bound
    7         Network Control
    */
    NDIS_PACKET_8021Q_INFO VlanPriInfo;
    VlanPriInfo.Value = NET_BUFFER_LIST_INFO(NetBufferList, Ieee8021QNetBufferListInfo);

    if (VlanPriInfo.TagHeader.UserPriority >= 4)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

USHORT ChkSumToHwFormat(PNDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO pChkSumInfo);

__inline VOID HwFormatToVlanPri(PNET_BUFFER_LIST NetBufferList, USHORT HwTag)
{
    NDIS_PACKET_8021Q_INFO VlanPriInfo;
    TAG_8021q VlanTag;

    VlanTag.Value = HwTag;

    VlanPriInfo.TagHeader.UserPriority = VlanTag.TagHeader.Priority;
    VlanPriInfo.TagHeader.VlanId = (VlanTag.TagHeader.VLanID1 << 8) + VlanTag.TagHeader.VLanID2;
    VlanPriInfo.TagHeader.CanonicalFormatId = 0; // Should be zero.
    VlanPriInfo.TagHeader.Reserved = 0; // Should be zero.

    NET_BUFFER_LIST_INFO(NetBufferList, Ieee8021QNetBufferListInfo) = VlanPriInfo.Value;
}

__inline USHORT GetHwFormatVlan(USHORT HwTag)
{
    TAG_8021q VlanTag;
    USHORT RetVlanId;

    VlanTag.Value = HwTag;

    RetVlanId= (VlanTag.TagHeader.VLanID1 << 8) + VlanTag.TagHeader.VLanID2;

    return RetVlanId;
}

__inline USHORT GetChkSumPacketIpVer(PMP_ADAPTER Adapter, PNET_BUFFER_LIST NetBufferList, PBOOLEAN pIsTcp, PBOOLEAN pIsUdp, PBOOLEAN pIsIpHdr)
{
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO ChkSumInfo;

    UNREFERENCED_PARAMETER(Adapter);

    ChkSumInfo.Value = NET_BUFFER_LIST_INFO(NetBufferList, TcpIpChecksumNetBufferListInfo);

    if (ChkSumInfo.Transmit.IsIPv4)
    {
        *pIsTcp = FALSE;
        *pIsUdp = FALSE;
        if (ChkSumInfo.Transmit.TcpChecksum)
        {
            *pIsTcp = TRUE;
        }
        if (ChkSumInfo.Transmit.UdpChecksum)
        {
            *pIsUdp = TRUE;
        }
        if (ChkSumInfo.Transmit.IpHeaderChecksum)
        {
            *pIsIpHdr = TRUE;
        }
        return 4;
    }


    if (ChkSumInfo.Transmit.IsIPv6)
    {
        *pIsTcp = FALSE;
        *pIsUdp = FALSE;
        if (ChkSumInfo.Transmit.TcpChecksum)
        {
            *pIsTcp = TRUE;
        }
        if (ChkSumInfo.Transmit.UdpChecksum)
        {
            *pIsUdp = TRUE;
        }
        if (ChkSumInfo.Transmit.IpHeaderChecksum)
        {
            *pIsIpHdr = TRUE;
        }
        return 6;
    }

    return 0;
}

VOID PhyPowerUp(IN PMP_ADAPTER Adapter);

__inline VOID EnableCR9346Write(PMP_ADAPTER Adapter)
{
    UCHAR ucCr9346Value;

    ucCr9346Value = Adapter->CSRAddress->CR9346;
    ucCr9346Value |= (CR9346_EEM1 | CR9346_EEM0);

    Adapter->CSRAddress->CR9346 = ucCr9346Value;
}

__inline VOID DisableCR9346Write(PMP_ADAPTER Adapter)
{
    UCHAR ucCr9346Value;

    ucCr9346Value = Adapter->CSRAddress->CR9346;
    ucCr9346Value &= ~(CR9346_EEM1 | CR9346_EEM0);

    Adapter->CSRAddress->CR9346 = ucCr9346Value;
}

VOID
SetConfigBit(
    _In_ PMP_ADAPTER MpAdapter,
    _In_ ULONG       Offset,
    _In_ UCHAR       Mask
    );

VOID
ClearConfigBit(
    _Inout_ PMP_ADAPTER MpAdapter,
    _In_    ULONG       Offset,
    _In_    UCHAR       Mask
    );

VOID
EnableMagicPacket(
    _In_ PMP_ADAPTER MpAdapter
    );

VOID
DisableMagicPacket(
    _In_ PMP_ADAPTER MpAdapter
    );

#endif  // _RT_FUNC_H