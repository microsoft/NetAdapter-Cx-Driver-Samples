/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include <preview/netadapteroffload.h>

#include "trace.h"
#include "device.h"
#include "adapter.h"
#include "oid.h"
#include "txqueue.h"
#include "rxqueue.h"
#include "eeprom.h"
#include "gigamac.h"

#define ETH_IS_ZERO(Address) ( \
    (((PUCHAR)(Address))[0] == ((UCHAR)0x00)) && \
    (((PUCHAR)(Address))[1] == ((UCHAR)0x00)) && \
    (((PUCHAR)(Address))[2] == ((UCHAR)0x00)) && \
    (((PUCHAR)(Address))[3] == ((UCHAR)0x00)) && \
    (((PUCHAR)(Address))[4] == ((UCHAR)0x00)) && \
    (((PUCHAR)(Address))[5] == ((UCHAR)0x00)))

NTSTATUS 
RtInitializeAdapterContext(
    _In_ RT_ADAPTER *adapter,
    _In_ WDFDEVICE device,
    _In_ NETADAPTER netAdapter
    )
/*++
Routine Description:

    Allocate RT_ADAPTER data block and do some initialization

Arguments:

    adapter     Pointer to receive pointer to our adapter

Return Value:

    NTSTATUS failure code, or STATUS_SUCCESS

--*/
{
    TraceEntry();
        
    NTSTATUS status = STATUS_SUCCESS;

    adapter->NetAdapter = netAdapter;
    adapter->WdfDevice = device;

    //
    // Get WDF miniport device context.
    //
    RtGetDeviceContext(adapter->WdfDevice)->Adapter = adapter;

    RtlZeroMemory(adapter->RssIndirectionTable, sizeof(adapter->RssIndirectionTable));

    adapter->EEPROMInUse = false;

    //spinlock
    WDF_OBJECT_ATTRIBUTES  attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = adapter->WdfDevice;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfSpinLockCreate(&attributes, &adapter->Lock));

Exit:
    TraceExitResult(status);

    return status;

}


void
RtAdapterUpdateHardwareChecksum(
    _In_ RT_ADAPTER *adapter
    )
{
    USHORT cpcr = adapter->CSRAddress->CPCR;
    
    // if one of the checksum offloads is needed
    // or one of the LSO offloads is enabled,
    // enable HW checksum
    if (adapter->IpHwChkSum ||
        adapter->TcpHwChkSum ||
        adapter->UdpHwChkSum ||
        adapter->LSOv4 == RtLsoOffloadEnabled ||
        adapter->LSOv6 == RtLsoOffloadEnabled)
    {
        cpcr |= CPCR_RX_CHECKSUM;
    }
    else
    {
        cpcr &= ~CPCR_RX_CHECKSUM;
    }

    adapter->CSRAddress->CPCR = cpcr;
}

void
RtAdapterQueryHardwareCapabilities(
    _Out_ NDIS_OFFLOAD *hardwareCaps
    )
{
    // TODO: when vlan is implemented, each of TCP_OFFLOAD's encapsulation
    // type has to be updated to include NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q_IN_OOB

    RtlZeroMemory(hardwareCaps, sizeof(*hardwareCaps));

    hardwareCaps->Header.Type = NDIS_OBJECT_TYPE_OFFLOAD;
    hardwareCaps->Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_5;
    hardwareCaps->Header.Revision = NDIS_OFFLOAD_REVISION_5;

    // IPv4 checksum offloads supported
    hardwareCaps->Checksum.IPv4Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    hardwareCaps->Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Transmit.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;

    hardwareCaps->Checksum.IPv4Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    hardwareCaps->Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Receive.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;

    // IPv6 checksum offloads supported
    hardwareCaps->Checksum.IPv6Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    hardwareCaps->Checksum.IPv6Transmit.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv6Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv6Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv6Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;

    hardwareCaps->Checksum.IPv6Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    hardwareCaps->Checksum.IPv6Receive.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv6Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv6Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv6Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;

    // LSOv1 IPv4 offload NOT supported
    hardwareCaps->LsoV1.IPv4.Encapsulation = NDIS_ENCAPSULATION_NOT_SUPPORTED;
    hardwareCaps->LsoV1.IPv4.MaxOffLoadSize = 0;
    hardwareCaps->LsoV1.IPv4.MinSegmentCount = 0;
    hardwareCaps->LsoV1.IPv4.TcpOptions = NDIS_OFFLOAD_NOT_SUPPORTED;
    hardwareCaps->LsoV1.IPv4.IpOptions = NDIS_OFFLOAD_NOT_SUPPORTED;

    // LSOv2 IPv4 offload supported
    hardwareCaps->LsoV2.IPv4.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    hardwareCaps->LsoV2.IPv4.MaxOffLoadSize = RT_LSO_OFFLOAD_MAX_SIZE;
    hardwareCaps->LsoV2.IPv4.MinSegmentCount = RT_LSO_OFFLOAD_MIN_SEGMENT_COUNT;

    // LSOv2 IPv6 offload supported
    hardwareCaps->LsoV2.IPv6.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    hardwareCaps->LsoV2.IPv6.MaxOffLoadSize = RT_LSO_OFFLOAD_MAX_SIZE;
    hardwareCaps->LsoV2.IPv6.MinSegmentCount = RT_LSO_OFFLOAD_MIN_SEGMENT_COUNT;
    hardwareCaps->LsoV2.IPv6.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->LsoV2.IPv6.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
}

_Use_decl_annotations_
NTSTATUS
EvtAdapterCreateTxQueue(
    _In_ NETADAPTER netAdapter,
    _Inout_ NETTXQUEUE_INIT * txQueueInit
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    TraceEntryNetAdapter(netAdapter);

    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

#pragma region Create NETTXQUEUE

    WDF_OBJECT_ATTRIBUTES txAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&txAttributes, RT_TXQUEUE);

    txAttributes.EvtDestroyCallback = EvtTxQueueDestroy;

    NET_PACKET_QUEUE_CONFIG txConfig;
    NET_PACKET_QUEUE_CONFIG_INIT(
        &txConfig,
        EvtTxQueueAdvance,
        EvtTxQueueSetNotificationEnabled,
        EvtTxQueueCancel);
    txConfig.EvtStart = EvtTxQueueStart;
    txConfig.EvtStop = EvtTxQueueStop;

    NETPACKETQUEUE txQueue;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetTxQueueCreate(
            txQueueInit,
            &txAttributes,
            &txConfig,
            &txQueue));

#pragma endregion

#pragma region Get packet extension offsets
    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);
    NET_PACKET_EXTENSION_QUERY extension;
    NET_PACKET_EXTENSION_QUERY_INIT(
        &extension,
        NET_PACKET_EXTENSION_CHECKSUM_NAME,
        NET_PACKET_EXTENSION_CHECKSUM_VERSION_1);
    
    NetTxQueueGetExtension(txQueue, &extension, &tx->ChecksumExtension);

    NET_PACKET_EXTENSION_QUERY_INIT(
        &extension,
        NET_PACKET_EXTENSION_LSO_NAME,
        NET_PACKET_EXTENSION_LSO_VERSION_1);

    NetTxQueueGetExtension(txQueue, &extension, &tx->LsoExtension);

#pragma endregion

#pragma region Initialize RTL8168D Transmit Queue

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtTxQueueInitialize(txQueue, adapter));

#pragma endregion

Exit:
    TraceExitResult(status);

    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtAdapterCreateRxQueue(
    _In_ NETADAPTER netAdapter,
    _Inout_ NETRXQUEUE_INIT * rxQueueInit
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    TraceEntryNetAdapter(netAdapter);

    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

#pragma region Create NETRXQUEUE

    WDF_OBJECT_ATTRIBUTES rxAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&rxAttributes, RT_RXQUEUE);

    rxAttributes.EvtDestroyCallback = EvtRxQueueDestroy;

    NET_PACKET_QUEUE_CONFIG rxConfig;
    NET_PACKET_QUEUE_CONFIG_INIT(
        &rxConfig,
        EvtRxQueueAdvance,
        EvtRxQueueSetNotificationEnabled,
        EvtRxQueueCancel);
    rxConfig.EvtStart = EvtRxQueueStart;
    rxConfig.EvtStop = EvtRxQueueStop;

    const ULONG queueId = NetRxQueueInitGetQueueId(rxQueueInit);
    NETPACKETQUEUE rxQueue;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetRxQueueCreate(rxQueueInit, &rxAttributes, &rxConfig, &rxQueue));

#pragma endregion

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);
    NET_PACKET_EXTENSION_QUERY extension;
    NET_PACKET_EXTENSION_QUERY_INIT(
        &extension,
        NET_PACKET_EXTENSION_CHECKSUM_NAME,
        NET_PACKET_EXTENSION_CHECKSUM_VERSION_1);

    rx->QueueId = queueId;

    NetRxQueueGetExtension(rxQueue, &extension, &rx->ChecksumExtension);

#pragma region Initialize RTL8168D Receive Queue

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtRxQueueInitialize(rxQueue, adapter));

#pragma endregion

Exit:
    TraceExitResult(status);

    return status;
}

static
NTSTATUS
RtReceiveScalingEnable(
    _In_ RT_ADAPTER *adapter,
    _In_ NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE protocols
    )
{
    UINT32 controlBitsEnable = RSS_MULTI_CPU_ENABLE | RSS_HASH_BITS_ENABLE;

    if (protocols & NetAdapterReceiveScalingProtocolTypeIPv4)
    {
        controlBitsEnable |= RSS_IPV4_ENABLE;

        if (protocols & NetAdapterReceiveScalingProtocolTypeTcp)
        {
            controlBitsEnable |= RSS_IPV4_TCP_ENABLE;
        }
    }

    if (protocols & NetAdapterReceiveScalingProtocolTypeIPv6)
    {
        controlBitsEnable |= RSS_IPV6_ENABLE;

        if (protocols & NetAdapterReceiveScalingProtocolTypeTcp)
        {
            controlBitsEnable |= RSS_IPV6_TCP_ENABLE;
        }
    }

    if (! GigaMacRssSetControl(adapter, controlBitsEnable))
    {
        WdfDeviceSetFailed(adapter->WdfDevice, WdfDeviceFailedAttemptRestart);
        return STATUS_UNSUCCESSFUL;
    }

    // TBD restart rx queue

    return STATUS_SUCCESS;
}

static
NTSTATUS
RtReceiveScalingSetHashSecretKey(
    _In_ RT_ADAPTER *adapter,
    _In_ NET_ADAPTER_RECEIVE_SCALING_HASH_SECRET_KEY const *hashSecretKey
    )
{
    const UINT32 * key = (const UINT32 *)hashSecretKey->Key;
    const size_t keySize = hashSecretKey->Length / sizeof(*key);
    if (! GigaMacRssSetHashSecretKey(adapter, key, keySize))
    {
        WdfDeviceSetFailed(adapter->WdfDevice, WdfDeviceFailedAttemptRestart);
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
EvtAdapterReceiveScalingEnable(
    _In_ NETADAPTER netAdapter,
    _In_ NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE hashType,
    _In_ NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE protocolType
    )
{
    UNREFERENCED_PARAMETER(hashType);

    TraceEntryNetAdapter(netAdapter);

    NTSTATUS status = STATUS_SUCCESS;
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtReceiveScalingEnable(adapter, protocolType));

Exit:
    TraceExitResult(status);

    return status;
}

static
VOID
EvtAdapterReceiveScalingDisable(
    _In_ NETADAPTER netAdapter
    )
{
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    if (! GigaMacRssSetControl(adapter, RSS_MULTI_CPU_DISABLE))
    {
        WdfDeviceSetFailed(adapter->WdfDevice, WdfDeviceFailedAttemptRestart);
    }
}

static
NTSTATUS
EvtAdapterReceiveScalingSetHashSecretKey(
    _In_ NETADAPTER netAdapter,
    _In_ NET_ADAPTER_RECEIVE_SCALING_HASH_SECRET_KEY const *hashSecretKey
    )
{
    return RtReceiveScalingSetHashSecretKey(RtGetAdapterContext(netAdapter), hashSecretKey);
}

static
NTSTATUS
EvtAdapterReceiveScalingSetIndirectionEntries(
    _In_ NETADAPTER netAdapter,
    _In_ NET_ADAPTER_RECEIVE_SCALING_INDIRECTION_ENTRIES *indirectionEntries
    )
{
    RT_ADAPTER * adapter = RtGetAdapterContext(netAdapter);

    for (size_t i = 0; i < indirectionEntries->Length; i++)
    {
        const ULONG queueId = RtGetRxQueueContext(indirectionEntries->Entries[i].PacketQueue)->QueueId;
        const UINT32 index = indirectionEntries->Entries[i].Index;

        const size_t bit0 = index >> 5;
        const size_t bit1 = bit0 + ARRAYSIZE(adapter->RssIndirectionTable) / 2;
        const UINT32 bitv = 1 << (index & 0x1f);

        adapter->RssIndirectionTable[bit0] = queueId & 1
            ? (adapter->RssIndirectionTable[bit0] | bitv)
            : (adapter->RssIndirectionTable[bit0] & ~bitv);

        adapter->RssIndirectionTable[bit1] = queueId & 2
            ? (adapter->RssIndirectionTable[bit1] | bitv)
            : (adapter->RssIndirectionTable[bit1] & ~bitv);
    }

    // we just blat the entire table, could be optimized further
    for (size_t i = 0; i < ARRAYSIZE(adapter->CSRAddress->RssIndirectionTable); i++)
    {
        adapter->CSRAddress->RssIndirectionTable[i] = adapter->RssIndirectionTable[i];
    }

    return STATUS_SUCCESS;
}

NTSTATUS
RtAdapterReadAddress(
    _In_ RT_ADAPTER *adapter
    )
/*++
Routine Description:

    Read the mac addresss from the adapter

Arguments:

    adapter     Pointer to our adapter

Return Value:

    STATUS_SUCCESS
    STATUS_NDIS_INVALID_ADDRESS

--*/
{
    TraceEntryRtAdapter(adapter);

    NTSTATUS status = STATUS_SUCCESS;

    if (adapter->EEPROMSupported)
    {
        RtAdapterReadEEPROMPermanentAddress(adapter);
    }
    else
    {
        for (ULONG i = 0; i < ETH_LENGTH_OF_ADDRESS; i += 2)
        {
            *((PUSHORT)(&adapter->PermanentAddress.Address[i])) =
                *(USHORT*)&adapter->CSRAddress->ID[i];
        }

        adapter->PermanentAddress.Length = ETHERNET_ADDRESS_LENGTH;
    }

    TraceLoggingWrite(
        RealtekTraceProvider,
        "PermanentAddressRead",
        TraceLoggingBinary(&adapter->PermanentAddress.Address, adapter->PermanentAddress.Length, "PermanentAddress"));

    if (ETH_IS_MULTICAST(adapter->PermanentAddress.Address) ||
            ETH_IS_BROADCAST(adapter->PermanentAddress.Address) ||
            ETH_IS_ZERO(adapter->PermanentAddress.Address))
    {
        GOTO_IF_NOT_NT_SUCCESS(Exit, status, STATUS_NDIS_INVALID_ADDRESS);
    }

    if (!adapter->OverrideAddress)
    {
        RtlCopyMemory(
            &adapter->CurrentAddress,
            &adapter->PermanentAddress,
            sizeof(adapter->PermanentAddress));
    }

Exit:
    TraceExitResult(status);

    return status;
}

void
RtAdapterRefreshCurrentAddress(
    _In_ RT_ADAPTER *adapter
    )
{
    UCHAR TmpUchar = adapter->CSRAddress->REG_F0_F3.RESV_F3;
    TmpUchar |= BIT_2;
    adapter->CSRAddress->REG_F0_F3.RESV_F3 = TmpUchar;

    // NIC hardware register to default
    RtAdapterIssueFullReset(adapter);

    // Program current address
    RtAdapterSetupCurrentAddress(adapter);
}

void
RtAdapterEnableCR9346Write(
    _In_ RT_ADAPTER *adapter
    )
{
    UCHAR ucCr9346Value;

    ucCr9346Value = adapter->CSRAddress->CR9346;
    ucCr9346Value |= (CR9346_EEM1 | CR9346_EEM0);

    adapter->CSRAddress->CR9346 = ucCr9346Value;
}

void
RtAdapterDisableCR9346Write(
    _In_ RT_ADAPTER *adapter
    )
{
    UCHAR ucCr9346Value;

    ucCr9346Value = adapter->CSRAddress->CR9346;
    ucCr9346Value &= ~(CR9346_EEM1 | CR9346_EEM0);

    adapter->CSRAddress->CR9346 = ucCr9346Value;
}

void
RtAdapterSetupCurrentAddress(
    _In_ RT_ADAPTER *adapter
    )
{
    RtAdapterEnableCR9346Write(adapter); {

        for (UINT i = 0; i < ETHERNET_ADDRESS_LENGTH; i++)
        {
            adapter->CSRAddress->ID[i] = adapter->CurrentAddress.Address[i];
        }

    } RtAdapterDisableCR9346Write(adapter);
}

ULONG
ComputeCrc(
    _In_reads_(length) UCHAR const *buffer,
         UINT length
    )
{
    ULONG crc = 0xffffffff;

    for (UINT i = 0; i < length; i++)
    {
        UCHAR curByte = buffer[i];

        for (UINT j = 0; j < 8; j++)
        {
            ULONG carry = ((crc & 0x80000000) ? 1 : 0) ^ (curByte & 0x01);
            crc <<= 1;
            curByte >>= 1;

            if (carry)
            {
                crc = (crc ^ 0x04c11db6) | carry;
            }
        }
    }

    return crc;
}

void
GetMulticastBit(
    _In_reads_(ETH_LENGTH_OF_ADDRESS) UCHAR const *address,
    _Out_ _Post_satisfies_(*byte < MAX_NIC_MULTICAST_REG) UCHAR *byte,
    _Out_ UCHAR *value
    )
/*++

Routine Description:

    For a given multicast address, returns the byte and bit in
    the card multicast registers that it hashes to. Calls
    ComputeCrc() to determine the CRC value.

--*/
{
    ULONG crc = ComputeCrc(address, ETH_LENGTH_OF_ADDRESS);

    // The bit number is now in the 6 most significant bits of CRC.
    UINT bitNumber = (UINT)((crc >> 26) & 0x3f);
    *byte = (UCHAR)(bitNumber / 8);
    *value = (UCHAR)((UCHAR)1 << (bitNumber % 8));
}

void
RtAdapterSetMulticastReg(
    _In_ RT_ADAPTER *adapter,
    _In_reads_bytes_(MAX_NIC_MULTICAST_REG) UCHAR const *multicastRegs
    )
{
    // The control block stores the multicast registers in descending order.
    // Write to them using DWORD writes.

    {
        UCHAR mar7_4[4];

        mar7_4[0] = multicastRegs[7];
        mar7_4[1] = multicastRegs[6];
        mar7_4[2] = multicastRegs[5];
        mar7_4[3] = multicastRegs[4];

        *reinterpret_cast<DWORD volatile *>(&adapter->CSRAddress->MulticastReg7) =
            *reinterpret_cast<DWORD*>(&mar7_4[0]);
    }

    {
        UCHAR mar3_0[4];

        mar3_0[0] = multicastRegs[3];
        mar3_0[1] = multicastRegs[2];
        mar3_0[2] = multicastRegs[1];
        mar3_0[3] = multicastRegs[0];

        *reinterpret_cast<DWORD volatile *>(&adapter->CSRAddress->MulticastReg3) =
            *reinterpret_cast<DWORD*>(&mar3_0[0]);
    }
}

void RtAdapterPushMulticastList(
    _In_ RT_ADAPTER *adapter
    )
{
    UCHAR multicastRegs[MAX_NIC_MULTICAST_REG] = { 0 };

    if (adapter->PacketFilter &
        (NET_PACKET_FILTER_TYPE_PROMISCUOUS |
            NET_PACKET_FILTER_TYPE_ALL_MULTICAST))
    {
        RtlFillMemory(multicastRegs, MAX_NIC_MULTICAST_REG, 0xFF);
    }
    else
    {
        // Now turn on the bit for each address in the multicast list.
        for (UINT i = 0; i < adapter->MCAddressCount; i++)
        {
            UCHAR byte, bit;
            GetMulticastBit(adapter->MCList[i], &byte, &bit);
            multicastRegs[byte] |= bit;
        }
    }

    RtAdapterSetMulticastReg(adapter, multicastRegs);
}

bool
RtAdapterQueryChipType(
    _In_ RT_ADAPTER *adapter,
    _Out_ RT_CHIP_TYPE *chipType
    )
{
    ULONG const tcr = adapter->CSRAddress->TCR;
    ULONG const rcr = adapter->CSRAddress->RCR;

    ULONG const macVer = tcr & 0x7C800000;
    ULONG const revId = tcr & (BIT_20 | BIT_21 | BIT_22);

    ULONG const eepromSelect = rcr & RCR_9356SEL;

    TraceLoggingWrite(
        RealtekTraceProvider,
        "ChipInfo",
        TraceLoggingUInt32(macVer),
        TraceLoggingUInt32(revId),
        TraceLoggingUInt32(eepromSelect));

    *chipType = RTLUNKNOWN;

    switch (macVer)
    {
    case 0x28000000:
        switch (revId)
        {
        case 0x300000:
            *chipType = RTL8168D_REV_C_REV_D;
            break;
        case 0x000000:
            *chipType = RTL8168D;
            break;
        default:
            *chipType = RTL8168D_REV_C_REV_D;
            break;
        }
        // RTL8168D && EEPROM 93C46
        return eepromSelect == 0;
    case 0x2c000000:
        *chipType = RTL8168E;
        return true;
    default:
        return false;
    }
}

void
RtAdapterSetupHardware(
    _In_ RT_ADAPTER *adapter
    )
{
    UCHAR TmpUchar;

    TmpUchar = adapter->CSRAddress->REG_F0_F3.RESV_F3;
    TmpUchar |= BIT_2;
    adapter->CSRAddress->REG_F0_F3.RESV_F3 = TmpUchar;

    TmpUchar = adapter->CSRAddress->REG_D0_D3.RESV_D1;
    TmpUchar |= (BIT_7 | BIT_1);
    adapter->CSRAddress->REG_D0_D3.RESV_D1 = TmpUchar;
}

void
RtAdapterIssueFullReset(
    _In_ RT_ADAPTER *adapter
    )
{
    adapter->CSRAddress->CmdReg = CR_RST;

    for (UINT i = 0; i < 20; i++)
    {
        KeStallExecutionProcessor(20);

        if (!(adapter->CSRAddress->CmdReg & CR_RST))
        {
            break;
        }
    }
}

void 
RtAdapterUpdateInterruptModeration(
    _In_ RT_ADAPTER *adapter
    )
{
    WdfSpinLockAcquire(adapter->Lock); {

        USHORT timerFlags = 0;

        if (adapter->InterruptModerationDisabled ||
            adapter->InterruptModerationMode == RtInterruptModerationDisabled)
        {
            // No interrupt moderation
            adapter->CSRAddress->IntMiti.RxTimerNum = 0;
            adapter->CSRAddress->IntMiti.TxTimerNum = 0;
        }
        else
        {
            switch (adapter->InterruptModerationLevel)
            {
            case RtInterruptModerationLow:
                timerFlags |= CPCR_INT_MITI_TIMER_UNIT_0;
                timerFlags |= CPCR_INT_MITI_TIMER_UNIT_1;

                // Rx: Approximately 500us delay before interrupting
                adapter->CSRAddress->IntMiti.RxTimerNum = 0x30;
                // Tx: Completion is one unit bigger than rx interrupt
                adapter->CSRAddress->IntMiti.TxTimerNum = 0x50;
                break;
            case RtInterruptModerationMedium:
                timerFlags |= CPCR_INT_MITI_TIMER_UNIT_0;
                timerFlags |= CPCR_INT_MITI_TIMER_UNIT_1;

                // Rx: Approximately 2400us delay before interrupting
                adapter->CSRAddress->IntMiti.RxTimerNum = 0xf0;
                // Tx: Completion isn't time-critical; give it the maximum slack.
                adapter->CSRAddress->IntMiti.TxTimerNum = 0xf0;
                break;
            }

        }

        USHORT currentCpcr = adapter->CSRAddress->CPCR;
        USHORT newCpcr = currentCpcr;

        newCpcr &= ~(CPCR_INT_MITI_TIMER_UNIT_0 | CPCR_INT_MITI_TIMER_UNIT_1);
        newCpcr |= timerFlags;

        if (currentCpcr != newCpcr)
        {
            adapter->CSRAddress->CPCR = newCpcr;
        }

    } WdfSpinLockRelease(adapter->Lock);
}

static
void
RtAdapterSetLinkLayerCapabilities(
    _In_ RT_ADAPTER *adapter
    )
{
    ULONG64 maxXmitLinkSpeed = RT_MEDIA_MAX_SPEED;
    ULONG64 maxRcvLinkSpeed = RT_MEDIA_MAX_SPEED;

    NET_ADAPTER_LINK_LAYER_CAPABILITIES linkLayerCapabilities;
    NET_ADAPTER_LINK_LAYER_CAPABILITIES_INIT(
        &linkLayerCapabilities,
        RT_SUPPORTED_FILTERS,
        RT_MAX_MCAST_LIST,
        NIC_SUPPORTED_STATISTICS,
        maxXmitLinkSpeed,
        maxRcvLinkSpeed);

    NetAdapterSetLinkLayerCapabilities(adapter->NetAdapter, &linkLayerCapabilities);
    NetAdapterSetLinkLayerMtuSize(adapter->NetAdapter, RT_MAX_PACKET_SIZE - ETH_LENGTH_OF_HEADER);
    NetAdapterSetPermanentLinkLayerAddress(adapter->NetAdapter, &adapter->PermanentAddress);
    NetAdapterSetCurrentLinkLayerAddress(adapter->NetAdapter, &adapter->CurrentAddress);
}

static
void
RtAdapterSetReceiveScalingCapabilities(
    _In_ RT_ADAPTER const *adapter
    )
{
    if (adapter->RssEnabled)
    {
        NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES receiveScalingCapabilities;
        NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES_INIT(
            &receiveScalingCapabilities,
            4, // NumberOfQueues
            NetAdapterReceiveScalingUnhashedTargetTypeHashIndex,
            NetAdapterReceiveScalingHashTypeToeplitz,
            NetAdapterReceiveScalingProtocolTypeIPv4 |
            NetAdapterReceiveScalingProtocolTypeIPv6 |
            NetAdapterReceiveScalingProtocolTypeTcp,
            EvtAdapterReceiveScalingEnable,
            EvtAdapterReceiveScalingDisable,
            EvtAdapterReceiveScalingSetHashSecretKey,
            EvtAdapterReceiveScalingSetIndirectionEntries);
        receiveScalingCapabilities.SynchronizeSetIndirectionEntries = true;
        NetAdapterSetReceiveScalingCapabilities(adapter->NetAdapter, &receiveScalingCapabilities);
    }
}

static
void
RtAdapterSetPowerCapabilities(
    _In_ RT_ADAPTER const *adapter
    )
{
    NET_ADAPTER_POWER_CAPABILITIES powerCapabilities;
    NET_ADAPTER_POWER_CAPABILITIES_INIT(&powerCapabilities);

    powerCapabilities.SupportedWakePatterns = NET_ADAPTER_WAKE_MAGIC_PACKET;

    NetAdapterSetPowerCapabilities(adapter->NetAdapter, &powerCapabilities);
}

static
void
RtAdapterSetDatapathCapabilities(
    _In_ RT_ADAPTER const *adapter
    )
{
    NET_ADAPTER_DMA_CAPABILITIES txDmaCapabilities;
    NET_ADAPTER_DMA_CAPABILITIES_INIT(&txDmaCapabilities, adapter->DmaEnabler);

    NET_ADAPTER_TX_CAPABILITIES txCapabilities;
    NET_ADAPTER_TX_CAPABILITIES_INIT_FOR_DMA(
        &txCapabilities,
        &txDmaCapabilities,
        RT_MAX_FRAGMENT_SIZE,
        1);

    // LSO goes to 64K payload header + extra
    //sgConfig.MaximumPacketSize = RT_MAX_FRAGMENT_SIZE * RT_MAX_PHYS_BUF_COUNT;

    txCapabilities.FragmentRingNumberOfElementsHint = adapter->NumTcb * RT_MAX_PHYS_BUF_COUNT;
    txCapabilities.MaximumNumberOfFragments = RT_MAX_PHYS_BUF_COUNT;
    
    NET_ADAPTER_DMA_CAPABILITIES rxDmaCapabilities;
    NET_ADAPTER_DMA_CAPABILITIES_INIT(&rxDmaCapabilities, adapter->DmaEnabler);

    NET_ADAPTER_RX_CAPABILITIES rxCapabilities;
    NET_ADAPTER_RX_CAPABILITIES_INIT_SYSTEM_MANAGED_DMA(
        &rxCapabilities,
        &rxDmaCapabilities,
        RT_MAX_PACKET_SIZE + FRAME_CRC_SIZE + RSVD_BUF_SIZE,
        1);

    rxCapabilities.FragmentBufferAlignment = 64;
    rxCapabilities.FragmentRingNumberOfElementsHint = adapter->ReceiveBuffers;

    NetAdapterSetDataPathCapabilities(adapter->NetAdapter, &txCapabilities, &rxCapabilities);

}

static
void
EvtAdapterOffloadSetChecksum(
    _In_ NETADAPTER netAdapter,
    _In_ NETOFFLOAD offload
    )
{
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    adapter->IpHwChkSum = NetOffloadIsChecksumIPv4Enabled(offload);
    adapter->TcpHwChkSum = NetOffloadIsChecksumTcpEnabled(offload);
    adapter->UdpHwChkSum = NetOffloadIsChecksumUdpEnabled(offload);

    RtAdapterUpdateHardwareChecksum(adapter);
}

static
void
EvtAdapterOffloadSetLso(
    _In_ NETADAPTER netAdapter,
    _In_ NETOFFLOAD offload
    )
{
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    adapter->LSOv4 = NetOffloadIsLsoIPv4Enabled(offload)
        ? RtLsoOffloadEnabled : RtLsoOffloadDisabled;
    adapter->LSOv6 = NetOffloadIsLsoIPv6Enabled(offload)
        ? RtLsoOffloadEnabled : RtLsoOffloadDisabled;

    RtAdapterUpdateHardwareChecksum(adapter);
}

static
void
RtAdapterSetOffloadCapabilities(
    _In_ RT_ADAPTER const *adapter
    )
{
    NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES checksumOffloadCapabilities;

    NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES_INIT(
        &checksumOffloadCapabilities,
        TRUE,
        TRUE,
        TRUE,
        EvtAdapterOffloadSetChecksum);

    NetAdapterOffloadSetChecksumCapabilities(adapter->NetAdapter, &checksumOffloadCapabilities);

    NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES lsoOffloadCapabilities;

    NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES_INIT(
        &lsoOffloadCapabilities,
        TRUE,
        TRUE,
        RT_LSO_OFFLOAD_MAX_SIZE,
        RT_LSO_OFFLOAD_MIN_SEGMENT_COUNT,
        EvtAdapterOffloadSetLso);

    NetAdapterOffloadSetLsoCapabilities(adapter->NetAdapter, &lsoOffloadCapabilities);
}

_Use_decl_annotations_
NTSTATUS
RtAdapterStart(
    RT_ADAPTER *adapter
    )
{
    TraceEntryNetAdapter(adapter->NetAdapter);

    NTSTATUS status = STATUS_SUCCESS;

    RtAdapterSetLinkLayerCapabilities(adapter);

    RtAdapterSetReceiveScalingCapabilities(adapter);

    RtAdapterSetPowerCapabilities(adapter);

    RtAdapterSetDatapathCapabilities(adapter);

    RtAdapterSetOffloadCapabilities(adapter);

    GOTO_IF_NOT_NT_SUCCESS(
        Exit, status,
        NetAdapterStart(adapter->NetAdapter));

Exit:
    TraceExitResult(status);

    return status;
}

