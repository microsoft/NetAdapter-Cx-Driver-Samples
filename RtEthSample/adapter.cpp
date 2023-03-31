/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include <netadaptercx.h>

#include "trace.h"
#include "device.h"
#include "adapter.h"
#include "txqueue.h"
#include "rxqueue.h"
#include "eeprom.h"
#include "gigamac.h"

// Uncomment the line below to use Checksum APIs from Netcx 2.0
// #define USE_NETCX20_CHECKSUM

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
    if (adapter->TxIpHwChkSum ||
        adapter->TxTcpHwChkSum ||
        adapter->TxUdpHwChkSum ||
        adapter->RxIpHwChkSum ||
        adapter->RxTcpHwChkSum ||
        adapter->RxUdpHwChkSum ||
        adapter->LSOv4 == RtGsoOffloadEnabled ||
        adapter->LSOv6 == RtGsoOffloadEnabled)
    {
        cpcr |= CPCR_RX_CHECKSUM;
    }
    else
    {
        cpcr &= ~CPCR_RX_CHECKSUM;
    }

    adapter->CSRAddress->CPCR = cpcr;
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

    const ULONG queueId = NetTxQueueInitGetQueueId(txQueueInit);

    NETPACKETQUEUE txQueue;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetTxQueueCreate(
            txQueueInit,
            &txAttributes,
            &txConfig,
            &txQueue));

    RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);
    tx->QueueId = queueId;
    tx->Priority = TPPoll_NPQ;

    NET_EXTENSION_QUERY extension;
    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_PACKET_EXTENSION_CHECKSUM_NAME,
        NET_PACKET_EXTENSION_CHECKSUM_VERSION_1,
        NetExtensionTypePacket);

    NetTxQueueGetExtension(txQueue, &extension, &tx->ChecksumExtension);

    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_PACKET_EXTENSION_GSO_NAME,
        NET_PACKET_EXTENSION_GSO_VERSION_1,
        NetExtensionTypePacket);

    NetTxQueueGetExtension(txQueue, &extension, &tx->GsoExtension);

    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_PACKET_EXTENSION_IEEE8021Q_NAME,
        NET_PACKET_EXTENSION_IEEE8021Q_VERSION_1,
        NetExtensionTypePacket);

    NetTxQueueGetExtension(txQueue, &extension, &tx->Ieee8021qExtension);

    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
        NetExtensionTypeFragment);

    NetTxQueueGetExtension(txQueue, &extension, &tx->VirtualAddressExtension);

    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
        NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
        NetExtensionTypeFragment);

    NetTxQueueGetExtension(txQueue, &extension, &tx->LogicalAddressExtension);

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtTxQueueInitialize(txQueue, adapter));

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

    RT_RXQUEUE *rx = RtGetRxQueueContext(rxQueue);
    NET_EXTENSION_QUERY extension;
    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_PACKET_EXTENSION_CHECKSUM_NAME,
        NET_PACKET_EXTENSION_CHECKSUM_VERSION_1,
        NetExtensionTypePacket);

    rx->QueueId = queueId;

    NetRxQueueGetExtension(rxQueue, &extension, &rx->ChecksumExtension);

    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
        NetExtensionTypeFragment);

    NetRxQueueGetExtension(rxQueue, &extension, &rx->VirtualAddressExtension);

    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
        NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
        NetExtensionTypeFragment);

    NetRxQueueGetExtension(rxQueue, &extension, &rx->LogicalAddressExtension);

    NET_EXTENSION_QUERY_INIT(
        &extension,
        NET_PACKET_EXTENSION_HASH_NAME,
        NET_PACKET_EXTENSION_HASH_VERSION_1,
        NetExtensionTypePacket);

    NetRxQueueGetExtension(rxQueue, &extension, &rx->HashValueExtension);

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtRxQueueInitialize(rxQueue, adapter));

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

    adapter->RssEnabled = true;

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

    adapter->RssEnabled = false;
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
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS const * address,
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
    ULONG crc = ComputeCrc(address->Address, address->Length);

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
        (NetPacketFilterFlagPromiscuous | NetPacketFilterFlagAllMulticast))
    {
        RtlFillMemory(multicastRegs, MAX_NIC_MULTICAST_REG, 0xFF);
    }
    else
    {
        // Now turn on the bit for each address in the multicast list.
        for (UINT i = 0; i < adapter->MCAddressCount; i++)
        {
            UCHAR byte, bit;
            GetMulticastBit(&adapter->MCList[i], &byte, &bit);
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

void
EvtSetReceiveFilter(
    _In_ NETADAPTER NetAdapter,
    _In_ NETRECEIVEFILTER Handle
    )
{
    RT_ADAPTER *adapter = RtGetAdapterContext(NetAdapter);

    WdfSpinLockAcquire(adapter->Lock); {

        adapter->PacketFilter = NetReceiveFilterGetPacketFilter(Handle);
        RtAdapterUpdateRcr(adapter);

        adapter->MCAddressCount = (UINT)NetReceiveFilterGetMulticastAddressCount(Handle);;

        RtlZeroMemory(
            adapter->MCList,
            sizeof(NET_ADAPTER_LINK_LAYER_ADDRESS) * RT_MAX_MCAST_LIST);

        if (adapter->MCAddressCount != 0U)
        {
            NET_ADAPTER_LINK_LAYER_ADDRESS const * MulticastAddressList = NetReceiveFilterGetMulticastAddressList(Handle);
            RtlCopyMemory(
                adapter->MCList,
                MulticastAddressList,
                sizeof(NET_ADAPTER_LINK_LAYER_ADDRESS) * adapter->MCAddressCount);
        }

        RtAdapterPushMulticastList(adapter);

    } WdfSpinLockRelease(adapter->Lock);
}

static
void
RtAdapterSetReceiveFilterCapabilities(
    _In_ RT_ADAPTER *adapter
    )
{
    NET_ADAPTER_RECEIVE_FILTER_CAPABILITIES receiveFilterCapabilities;
    NET_ADAPTER_RECEIVE_FILTER_CAPABILITIES_INIT(
        &receiveFilterCapabilities,
        EvtSetReceiveFilter);
    receiveFilterCapabilities.SupportedPacketFilters = RT_SUPPORTED_FILTERS;
    receiveFilterCapabilities.MaximumMulticastAddresses = RT_MAX_MCAST_LIST;

    NetAdapterSetReceiveFilterCapabilities(adapter->NetAdapter, &receiveFilterCapabilities);
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
    NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES receiveScalingCapabilities;
    NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES_INIT(
        &receiveScalingCapabilities,
        RT_NUMBER_OF_RX_QUEUES,
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

static
void
RtAdapterSetPowerCapabilities(
    _In_ RT_ADAPTER const *adapter
    )
{
    NET_ADAPTER_WAKE_MAGIC_PACKET_CAPABILITIES magicPacketCapabilities;
    NET_ADAPTER_WAKE_MAGIC_PACKET_CAPABILITIES_INIT(&magicPacketCapabilities);
    magicPacketCapabilities.MagicPacket = TRUE;

    NetAdapterWakeSetMagicPacketCapabilities(adapter->NetAdapter, &magicPacketCapabilities);
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
        1);

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

#ifdef USE_NETCX20_CHECKSUM
static
void
EvtAdapterOffloadSetChecksum(
    _In_ NETADAPTER netAdapter,
    _In_ NETOFFLOAD offload
    )
{
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    adapter->TxIpHwChkSum = adapter->RxIpHwChkSum = NetOffloadIsChecksumIPv4Enabled(offload);
    adapter->TxTcpHwChkSum = adapter->RxTcpHwChkSum = NetOffloadIsChecksumTcpEnabled(offload);
    adapter->TxUdpHwChkSum = adapter->RxUdpHwChkSum = NetOffloadIsChecksumUdpEnabled(offload);

    RtAdapterUpdateHardwareChecksum(adapter);
}
#else
static
void
EvtAdapterOffloadSetTxChecksum(
    _In_ NETADAPTER netAdapter,
    _In_ NETOFFLOAD offload
    )
{
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    adapter->TxIpHwChkSum = NetOffloadIsTxChecksumIPv4Enabled(offload);
    adapter->TxTcpHwChkSum = NetOffloadIsTxChecksumTcpEnabled(offload);
    adapter->TxUdpHwChkSum = NetOffloadIsTxChecksumUdpEnabled(offload);

    RtAdapterUpdateHardwareChecksum(adapter);
}

static
void
EvtAdapterOffloadSetRxChecksum(
    _In_ NETADAPTER netAdapter,
    _In_ NETOFFLOAD offload
    )
{
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    adapter->RxIpHwChkSum = NetOffloadIsRxChecksumIPv4Enabled(offload);
    adapter->RxTcpHwChkSum = NetOffloadIsRxChecksumTcpEnabled(offload);
    adapter->RxUdpHwChkSum = NetOffloadIsRxChecksumUdpEnabled(offload);

    RtAdapterUpdateHardwareChecksum(adapter);
}
#endif

static
void
EvtAdapterOffloadSetGso(
    _In_ NETADAPTER netAdapter,
    _In_ NETOFFLOAD offload
    )
{
    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

    adapter->LSOv4 = NetOffloadIsLsoIPv4Enabled(offload)
        ? RtGsoOffloadEnabled : RtGsoOffloadDisabled;
    adapter->LSOv6 = NetOffloadIsLsoIPv6Enabled(offload)
        ? RtGsoOffloadEnabled : RtGsoOffloadDisabled;

    RtAdapterUpdateHardwareChecksum(adapter);
}

static
void
RtAdapterSetOffloadCapabilities(
    _In_ RT_ADAPTER const *adapter
    )
{
#ifdef USE_NETCX20_CHECKSUM
    NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES checksumOffloadCapabilities;

    NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES_INIT(
        &checksumOffloadCapabilities,
        TRUE,
        TRUE,
        TRUE,
        EvtAdapterOffloadSetChecksum);

    NetAdapterOffloadSetChecksumCapabilities(adapter->NetAdapter, &checksumOffloadCapabilities);
#else
    NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES txChecksumOffloadCapabilities;

    auto const layer3Flags = NetAdapterOffloadLayer3FlagIPv4NoOptions |
        NetAdapterOffloadLayer3FlagIPv4WithOptions |
        NetAdapterOffloadLayer3FlagIPv6NoExtensions |
        NetAdapterOffloadLayer3FlagIPv6WithExtensions;

    auto const layer4Flags = NetAdapterOffloadLayer4FlagTcpNoOptions |
        NetAdapterOffloadLayer4FlagTcpWithOptions |
        NetAdapterOffloadLayer4FlagUdp;

    NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES_INIT(
        &txChecksumOffloadCapabilities,
        layer3Flags,
        EvtAdapterOffloadSetTxChecksum);

    txChecksumOffloadCapabilities.Layer4Flags = layer4Flags;
    txChecksumOffloadCapabilities.Layer4HeaderOffsetLimit = RT_CHECKSUM_OFFLOAD_LAYER_4_HEADER_OFFSET_LIMIT;

    NetAdapterOffloadSetTxChecksumCapabilities(adapter->NetAdapter, &txChecksumOffloadCapabilities);

    NET_ADAPTER_OFFLOAD_RX_CHECKSUM_CAPABILITIES rxChecksumOffloadCapabilities;

    NET_ADAPTER_OFFLOAD_RX_CHECKSUM_CAPABILITIES_INIT(
        &rxChecksumOffloadCapabilities,
        EvtAdapterOffloadSetRxChecksum);

    NetAdapterOffloadSetRxChecksumCapabilities(adapter->NetAdapter, &rxChecksumOffloadCapabilities);
#endif

    NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES gsoOffloadCapabilities;

    auto const layer3GsoFlags = NetAdapterOffloadLayer3FlagIPv4NoOptions |
        NetAdapterOffloadLayer3FlagIPv4WithOptions |
        NetAdapterOffloadLayer3FlagIPv6NoExtensions |
        NetAdapterOffloadLayer3FlagIPv6WithExtensions;

    auto const layer4GsoFlags = NetAdapterOffloadLayer4FlagTcpNoOptions |
        NetAdapterOffloadLayer4FlagTcpWithOptions;

    NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES_INIT(
        &gsoOffloadCapabilities,
        layer3GsoFlags,
        layer4GsoFlags,
        RT_GSO_OFFLOAD_MAX_SIZE,
        RT_GSO_OFFLOAD_MIN_SEGMENT_COUNT,
        EvtAdapterOffloadSetGso);

    gsoOffloadCapabilities.Layer4HeaderOffsetLimit = RT_GSO_OFFLOAD_LAYER_4_HEADER_OFFSET_LIMIT;

    NetAdapterOffloadSetGsoCapabilities(adapter->NetAdapter, &gsoOffloadCapabilities);

    auto const ieee8021qTaggingFlag = NetAdapterOffloadIeee8021PriorityTaggingFlag |
        NetAdapterOffloadIeee8021VlanTaggingFlag;

    NET_ADAPTER_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES ieee8021qTagOffloadCapabilities;

    NET_ADAPTER_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES_INIT(
        &ieee8021qTagOffloadCapabilities,
        ieee8021qTaggingFlag);

    NetAdapterOffloadSetIeee8021qTagCapabilities(adapter->NetAdapter, &ieee8021qTagOffloadCapabilities);
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

    RtAdapterSetReceiveFilterCapabilities(adapter);

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

