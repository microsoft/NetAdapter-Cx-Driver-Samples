/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

typedef struct _RT_INTERRUPT RT_INTERRUPT;
typedef struct _RT_TALLY RT_TALLY;

#pragma region Setting Enumerations

typedef enum _RT_DUPLEX_STATE : UCHAR {
    RtDuplexNone = 0,
    RtDuplexHalf = 1,
    RtDuplexFull = 2,
} RT_DUPLEX_STATE;

typedef enum _RT_CHKSUM_OFFLOAD : UCHAR
{
    RtChecksumOffloadDisabled = 0,
    RtChecksumOffloadTxEnabled = 1,
    RtChecksumOffloadRxEnabled = 2,
    RtChecksumOffloadTxRxEnabled = 3,
} RT_CHKSUM_OFFLOAD;

typedef enum _RT_IM_MODE
{
    RtInterruptModerationOff = 0,
    RtInterruptModerationLow = 1,
    RtInterruptModerationMedium = 2,
} RT_IM_MODE;

typedef enum _RT_CHIP_TYPE
{
    RTLUNKNOWN,
    RTL8168D
} RT_CHIP_TYPE;

typedef enum _RT_SPEED_DUPLEX_MODE {

    RtSpeedDuplexModeAutoNegotiation = 0,
    RtSpeedDuplexMode10MHalfDuplex = 1,
    RtSpeedDuplexMode10MFullDuplex = 2,
    RtSpeedDuplexMode100MHalfDuplex = 3,
    RtSpeedDuplexMode100MFullDuplex = 4,
    RtSpeedDuplexMode1GFullDuplex = 5,

} RT_SPEED_DUPLEX_MODE;

#pragma endregion

// Context for NETADAPTER
typedef struct _RT_ADAPTER
{
    // WDF handles associated with this context
    NETADAPTER NetAdapter;
    WDFDEVICE WdfDevice;

    // Handle to default Tx and Rx Queues
    NETTXQUEUE TxQueue;
    NETRXQUEUE RxQueue;

    // Pointer to interrupt object
    RT_INTERRUPT *Interrupt;

    // Handle given by NDIS when the NetAdapter registered itself.
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
    USHORT LinkSpeed;
    NET_IF_MEDIA_DUPLEX_STATE DuplexMode;

    // multicast list
    UINT MCAddressCount;
    UCHAR MCList[RT_MAX_MCAST_LIST][ETH_LENGTH_OF_ADDRESS];

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

    RT_MAC *volatile CSRAddress;

    // user "*SpeedDuplex"  setting
    RT_SPEED_DUPLEX_MODE SpeedDuplex;

    WDFDMAENABLER DmaEnabler;

    PHYSICAL_ADDRESS TallyPhy;
    RT_TALLY *GTally;

    RT_CHIP_TYPE ChipType;

    BOOLEAN LinkAutoNeg;

    NDIS_OFFLOAD_ENCAPSULATION OffloadEncapsulation;

    RT_CHKSUM_OFFLOAD UDPChksumOffv4;
    RT_CHKSUM_OFFLOAD UDPChksumOffv6;
    RT_CHKSUM_OFFLOAD IPChksumOffv4;
    RT_CHKSUM_OFFLOAD TCPChksumOffv4;
    RT_CHKSUM_OFFLOAD TCPChksumOffv6;

    USHORT TransmitBuffers;

    BOOLEAN IpRxHwChkSumv4;
    BOOLEAN TcpRxHwChkSumv4;
    BOOLEAN UdpRxHwChkSumv4;

    BOOLEAN TcpRxHwChkSumv6;
    BOOLEAN UdpRxHwChkSumv6;

    ULONG ChksumErrRxIpv4Cnt;
    ULONG ChksumErrRxTcpIpv6Cnt;
    ULONG ChksumErrRxTcpIpv4Cnt;
    ULONG ChksumErrRxUdpIpv6Cnt;
    ULONG ChksumErrRxUdpIpv4Cnt;

    // Tracks *WakeOnLan Keyword
    bool WakeOnMagicPacketEnabled;

    // Hardware capability, managed by INF keyword
    RT_IM_MODE InterruptModerationMode;
    // Runtime disablement, controlled by OID
    bool InterruptModerationDisabled;
} RT_ADAPTER;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_ADAPTER, RtGetAdapterContext);

EVT_NET_ADAPTER_SET_CAPABILITIES EvtAdapterSetCapabilities;
EVT_NET_ADAPTER_CREATE_TXQUEUE   EvtAdapterCreateTxQueue;
EVT_NET_ADAPTER_CREATE_RXQUEUE   EvtAdapterCreateRxQueue;

EVT_WDF_DEVICE_CONTEXT_DESTROY   RtDestroyAdapterContext;

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
    _In_ RT_ADAPTER *adapter,
    _In_ WDFDEVICE device,
    _In_ NETADAPTER netAdapter);

void RtAdapterUpdateInterruptModeration(_In_ RT_ADAPTER *adapter);

void
RtAdapterQueryOffloadConfiguration(
    _In_  RT_ADAPTER const *adapter,
    _Out_ NDIS_OFFLOAD *offloadCaps);

_Requires_lock_held_(adapter->Lock)
void
RtAdapterUpdateEnabledChecksumOffloads(_In_ RT_ADAPTER *adapter);

NTSTATUS
RtAdapterReadAddress(_In_ RT_ADAPTER *adapter);

void
RtAdapterRefreshCurrentAddress(_In_ RT_ADAPTER *adapter);

void
RtAdapterSetupHardware(RT_ADAPTER *adapter);

void
RtAdapterIssueFullReset(_In_ RT_ADAPTER *adapter);

void
RtAdapterEnableCR9346Write(_In_ RT_ADAPTER *adapter);

void
RtAdapterDisableCR9346Write(_In_ RT_ADAPTER *adapter);

void
RtAdapterSetupCurrentAddress(_In_ RT_ADAPTER *adapter);

void
RtAdapterPushMulticastList(_In_ RT_ADAPTER *adapter);

bool
RtAdapterQueryChipType(_In_ RT_ADAPTER *adapter, _Out_ RT_CHIP_TYPE *chipType);
