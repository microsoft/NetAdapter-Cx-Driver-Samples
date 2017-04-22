/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#ifndef _MP_H
#define _MP_H

typedef struct _RT_INTERRUPT RT_INTERRUPT;

//--------------------------------------
// The miniport adapter structure
//--------------------------------------

typedef struct _MP_ADAPTER
{
    // WDF handles associated with this context
    NETADAPTER Adapter;
    WDFDEVICE Device;
    RT_INTERRUPT *Interrupt;

    // Handle to default Tx and Rx Queues
    NETTXQUEUE TxQueue;
    NETRXQUEUE RxQueue;

    // Handle given by NDIS when the Adapter registered itself.
    NDIS_HANDLE NdisLegacyAdapterHandle;

    // configuration
    UCHAR PermanentAddress[ETH_LENGTH_OF_ADDRESS];
    UCHAR CurrentAddress[ETH_LENGTH_OF_ADDRESS];
    BOOLEAN OverrideAddress;

    ULONG NumTcb;             // Total number of TCBs

    // spin locks
    WDFSPINLOCK Lock;

    // Packet Filter and look ahead size.
    ULONG PacketFilter;
    USHORT usLinkSpeed;
    USHORT usDuplexMode;

    BOOLEAN TxFlowCtrl;
    BOOLEAN RxFlowCtrl;

    // multicast list
    UINT MCAddressCount;
    UCHAR MCList[NIC_MAX_MCAST_LIST][ETH_LENGTH_OF_ADDRESS];

    // Packet counts
    ULONG64 InUcastOctets;
    ULONG64 InMulticastOctets;
    ULONG64 InBroadcastOctets;
    ULONG64 OutUCastPkts;
    ULONG64 OutMulticastPkts;
    ULONG64 OutBroadcastPkts;
    ULONG64 OutUCastOctets;
    ULONG64 OutMulticastOctets;
    ULONG64 OutBroadcastOctets;

    ULONG64 TotalTxErr;
    ULONG   TotalRxErr;

    ULONG64 HwTotalRxMatchPhy;
    ULONG64 HwTotalRxBroadcast;
    ULONG64 HwTotalRxMulticast;

    // Count of transmit errors
    ULONG TxAbortExcessCollisions;
    ULONG TxDmaUnderrun;
    ULONG TxOneRetry;
    ULONG TxMoreThanOneRetry;

    // Count of receive errors
    ULONG RcvResourceErrors;

    volatile PHW_CSR CSRAddress;

    UCHAR AiForceDpx;         // duplex setting
    USHORT AiTempSpeed;        // 'Speed', user over-ride of line speed
    UCHAR SpeedDuplex;        // New reg value for speed/duplex

    //
    // new fields for NDIS 6.0 version to report unknown
    // states and speed
    //
    NDIS_MEDIA_CONNECT_STATE MediaState;
    NDIS_MEDIA_DUPLEX_STATE MediaDuplexState;
    ULONG64 LinkSpeed;

    WDFDMAENABLER DmaEnabler;

    PHYSICAL_ADDRESS TallyPhy;
    GDUMP_TALLY *GTally;

    NIC_CHIP_TYPE ChipType;

    BOOLEAN LinkAutoNeg;

    UCHAR UDPChksumOffv4;
    UCHAR UDPChksumOffv6;
    UCHAR IPChksumOffv4;
    UCHAR TCPChksumOffv4;
    UCHAR TCPChksumOffv6;

    USHORT ReceiveBuffers;
    USHORT TransmitBuffers;

    BOOLEAN IpRxHwChkSumv4;
    BOOLEAN TcpRxHwChkSumv4;
    BOOLEAN UdpRxHwChkSumv4;

    ULONG ChksumErrRxIpv4Cnt;
    ULONG ChksumErrRxTcpIpv6Cnt;
    ULONG ChksumErrRxTcpIpv4Cnt;
    ULONG ChksumErrRxUdpIpv6Cnt;
    ULONG ChksumErrRxUdpIpv4Cnt;

    // Tracks *WakeOnLan Keyword
    bool WakeOnMagicPacketEnabled;
} MP_ADAPTER, *PMP_ADAPTER;

#endif  // _MP_H



