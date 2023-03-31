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

typedef enum _RT_GSO_OFFLOAD : UCHAR
{
    RtGsoOffloadDisabled = 0,
    RtGsoOffloadEnabled = 1,
} RT_GSO_OFFLOAD;

#define RT_GSO_OFFLOAD_MAX_SIZE 64000
#define RT_GSO_OFFLOAD_MIN_SEGMENT_COUNT 2
#define RT_GSO_OFFLOAD_LAYER_4_HEADER_OFFSET_LIMIT 127
#define RT_CHECKSUM_OFFLOAD_LAYER_4_HEADER_OFFSET_LIMIT 1023

typedef enum _RT_IM_MODE
{
    RtInterruptModerationDisabled = 0,
    RtInterruptModerationEnabled = 1,
} RT_IM_MODE;

typedef enum _RT_IM_LEVEL
{
    RtInterruptModerationLow = 0,
    RtInterruptModerationMedium = 1,
} RT_IM_LEVEL;

typedef enum _RT_FLOW_CONTROL
{
    RtFlowControlDisabled = 0,
    RtFlowControlTxEnabled = 1,
    RtFlowControlRxEnabled = 2,
    RtFlowControlTxRxEnabled = 3,
} RT_FLOW_CONTROL;

typedef enum _RT_CHIP_TYPE
{
    RTLUNKNOWN,
    RTL8168D,
    RTL8168D_REV_C_REV_D,
    RTL8168E
} RT_CHIP_TYPE;

typedef enum _RT_SPEED_DUPLEX_MODE {

    RtSpeedDuplexModeAutoNegotiation = 0,
    RtSpeedDuplexMode10MHalfDuplex = 1,
    RtSpeedDuplexMode10MFullDuplex = 2,
    RtSpeedDuplexMode100MHalfDuplex = 3,
    RtSpeedDuplexMode100MFullDuplex = 4,
    // 1Gb Half Duplex is not supported
    RtSpeedDuplexMode1GFullDuplex = 6,

} RT_SPEED_DUPLEX_MODE;

// Context for NETADAPTER
typedef struct _RT_ADAPTER
{
    // WDF handles associated with this context
    NETADAPTER NetAdapter;
    WDFDEVICE WdfDevice;

    // Handle to default Tx and Rx Queues
    NETPACKETQUEUE TxQueues[RT_NUMBER_OF_TX_QUEUES];
    NETPACKETQUEUE RxQueues[RT_NUMBER_OF_RX_QUEUES];

    // Pointer to interrupt object
    RT_INTERRUPT *Interrupt;

    // configuration
    NET_ADAPTER_LINK_LAYER_ADDRESS PermanentAddress;
    NET_ADAPTER_LINK_LAYER_ADDRESS CurrentAddress;
    BOOLEAN OverrideAddress;

    ULONG NumTcb;             // Total number of TCBs

                              // spin locks
    WDFSPINLOCK Lock;

    // Packet Filter and look ahead size.
    NET_PACKET_FILTER_FLAGS PacketFilter;
    USHORT LinkSpeed;
    NET_IF_MEDIA_DUPLEX_STATE DuplexMode;

    // multicast list
    UINT MCAddressCount;
    NET_ADAPTER_LINK_LAYER_ADDRESS MCList[RT_MAX_MCAST_LIST];

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

    WDFCOMMONBUFFER HwTallyMemAlloc;
    PHYSICAL_ADDRESS TallyPhy;
    RT_TALLY *GTally;

    RT_CHIP_TYPE ChipType;

    bool LinkAutoNeg;

    USHORT ReceiveBuffers;
    USHORT TransmitBuffers;

    BOOLEAN TxIpHwChkSum;
    BOOLEAN TxTcpHwChkSum;
    BOOLEAN TxUdpHwChkSum;

    BOOLEAN RxIpHwChkSum;
    BOOLEAN RxTcpHwChkSum;
    BOOLEAN RxUdpHwChkSum;

    ULONG ChksumErrRxIpv4Cnt;
    ULONG ChksumErrRxTcpIpv6Cnt;
    ULONG ChksumErrRxTcpIpv4Cnt;
    ULONG ChksumErrRxUdpIpv6Cnt;
    ULONG ChksumErrRxUdpIpv4Cnt;

    // Tracks *WakeOnLan Keyword
    bool WakeOnMagicPacketEnabled;

    // Hardware capability, managed by INF keyword
    RT_IM_MODE InterruptModerationMode;
    // Moderation Degree, managed by INF keyword
    RT_IM_LEVEL InterruptModerationLevel;
    // Runtime disablement, controlled by OID
    bool InterruptModerationDisabled;

    // basic detection of concurrent EEPROM use
    bool EEPROMSupported;
    bool EEPROMInUse;

    // ReceiveScaling
    bool RssEnabled;
    UINT32 RssIndirectionTable[RT_INDIRECTION_TABLE_SIZE];

    RT_FLOW_CONTROL FlowControl;

    RT_GSO_OFFLOAD LSOv4;
    RT_GSO_OFFLOAD LSOv6;
} RT_ADAPTER;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_ADAPTER, RtGetAdapterContext);

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

NTSTATUS
RtAdapterStart(
    _In_ RT_ADAPTER *adapter);

void RtAdapterUpdateInterruptModeration(_In_ RT_ADAPTER *adapter);

void
RtAdapterUpdateHardwareChecksum(_In_ RT_ADAPTER *adapter);

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
