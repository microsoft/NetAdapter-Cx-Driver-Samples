/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "trace.h"
#include "device.h"
#include "adapter.h"
#include "oid.h"
#include "txqueue.h"
#include "rxqueue.h"

NTSTATUS 
RtInitializeAdapterContext(
    _In_ RT_ADAPTER *adapter,
    _In_ WDFDEVICE device,
    _In_ NETADAPTER netAdapter)
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

    adapter->OffloadEncapsulation.Header.Revision = NDIS_OFFLOAD_ENCAPSULATION_REVISION_1;
    adapter->OffloadEncapsulation.Header.Size = NDIS_SIZEOF_OFFLOAD_ENCAPSULATION_REVISION_1;
    adapter->OffloadEncapsulation.Header.Type = NDIS_OBJECT_TYPE_OFFLOAD_ENCAPSULATION;

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
RtAdapterQueryOffloadConfiguration(
    _In_ RT_ADAPTER const *adapter,
    _Out_ NDIS_OFFLOAD *offloadCaps)
{
    RtlZeroMemory(offloadCaps, sizeof(*offloadCaps));

    offloadCaps->Header.Type = NDIS_OBJECT_TYPE_OFFLOAD;
    offloadCaps->Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_5;
    offloadCaps->Header.Revision = NDIS_OFFLOAD_REVISION_5;

    // IPv4 : Tx
    offloadCaps->Checksum.IPv4Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (adapter->IPChksumOffv4 == RtChecksumOffloadTxEnabled ||
        adapter->IPChksumOffv4 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
        offloadCaps->Checksum.IPv4Transmit.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->TCPChksumOffv4 == RtChecksumOffloadTxEnabled ||
        adapter->TCPChksumOffv4 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        offloadCaps->Checksum.IPv4Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->UDPChksumOffv4 == RtChecksumOffloadTxEnabled ||
        adapter->UDPChksumOffv4 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    // IPv4 : Rx
    offloadCaps->Checksum.IPv4Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (adapter->IPChksumOffv4 == RtChecksumOffloadRxEnabled ||
        adapter->IPChksumOffv4 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
        offloadCaps->Checksum.IPv4Receive.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->TCPChksumOffv4 == RtChecksumOffloadRxEnabled ||
        adapter->TCPChksumOffv4 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        offloadCaps->Checksum.IPv4Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->UDPChksumOffv4 == RtChecksumOffloadRxEnabled ||
        adapter->UDPChksumOffv4 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    // IPv6 : Tx
    offloadCaps->Checksum.IPv6Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    offloadCaps->Checksum.IPv6Transmit.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;

    if (adapter->TCPChksumOffv6 == RtChecksumOffloadTxEnabled ||
        adapter->TCPChksumOffv6 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv6Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        offloadCaps->Checksum.IPv6Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->UDPChksumOffv6 == RtChecksumOffloadTxEnabled ||
        adapter->UDPChksumOffv6 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv6Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    // IPv6 : Rx
    offloadCaps->Checksum.IPv6Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    offloadCaps->Checksum.IPv6Receive.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;

    if (adapter->TCPChksumOffv6 == RtChecksumOffloadRxEnabled ||
        adapter->TCPChksumOffv6 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv6Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        offloadCaps->Checksum.IPv6Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->UDPChksumOffv6 == RtChecksumOffloadRxEnabled ||
        adapter->UDPChksumOffv6 == RtChecksumOffloadTxRxEnabled)
    {
        offloadCaps->Checksum.IPv6Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }
}

_Use_decl_annotations_
void
RtAdapterUpdateEnabledChecksumOffloads(_In_ RT_ADAPTER *adapter)
{
    adapter->IpRxHwChkSumv4 =
        (adapter->IPChksumOffv4 == RtChecksumOffloadRxEnabled ||
            adapter->IPChksumOffv4 == RtChecksumOffloadTxRxEnabled);

    adapter->TcpRxHwChkSumv4 =
        (adapter->TCPChksumOffv4 == RtChecksumOffloadRxEnabled ||
            adapter->TCPChksumOffv4 == RtChecksumOffloadTxRxEnabled);

    adapter->UdpRxHwChkSumv4 =
        (adapter->UDPChksumOffv4 == RtChecksumOffloadRxEnabled ||
            adapter->UDPChksumOffv4 == RtChecksumOffloadTxRxEnabled);

    adapter->TcpRxHwChkSumv6 =
        (adapter->TCPChksumOffv6 == RtChecksumOffloadRxEnabled ||
            adapter->TCPChksumOffv6 == RtChecksumOffloadTxRxEnabled);

    adapter->UdpRxHwChkSumv6 =
        (adapter->UDPChksumOffv6 == RtChecksumOffloadRxEnabled ||
            adapter->UDPChksumOffv6 == RtChecksumOffloadTxRxEnabled);

    WdfSpinLockAcquire(adapter->Lock); {

        USHORT cpcr = adapter->CSRAddress->CPCR;
    
        // if one kind of offload needed, enable RX HW checksum
        if (adapter->IpRxHwChkSumv4 || adapter->TcpRxHwChkSumv4 ||
            adapter->UdpRxHwChkSumv4 || adapter->TcpRxHwChkSumv6 ||
            adapter->UdpRxHwChkSumv6)
        {
            cpcr |= CPCR_RX_CHECKSUM;
        }
        else
        {
            cpcr &= ~CPCR_RX_CHECKSUM;
        }

        adapter->CSRAddress->CPCR = cpcr;

    } WdfSpinLockRelease(adapter->Lock);
}

void
RtAdapterQueryHardwareCapabilities(
    _Out_ NDIS_OFFLOAD *hardwareCaps)
{
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
}

_Use_decl_annotations_
NTSTATUS
EvtAdapterSetCapabilities(
    _In_ NETADAPTER netAdapter)
{
    TraceEntryNetAdapter(netAdapter);

    NTSTATUS status = STATUS_SUCCESS;

    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

#pragma region Link Layer Capabilities (Required)

    ULONG64 maxXmitLinkSpeed = RT_MEDIA_MAX_SPEED;
    ULONG64 maxRcvLinkSpeed = RT_MEDIA_MAX_SPEED;

    NET_ADAPTER_LINK_LAYER_CAPABILITIES linkLayerCapabilities;
    NET_ADAPTER_LINK_LAYER_CAPABILITIES_INIT(
        &linkLayerCapabilities,
        RT_SUPPORTED_FILTERS,
        RT_MAX_MCAST_LIST,
        NIC_SUPPORTED_STATISTICS,
        maxXmitLinkSpeed,
        maxRcvLinkSpeed,
        ETH_LENGTH_OF_ADDRESS,
        adapter->PermanentAddress,
        adapter->CurrentAddress);

    NetAdapterSetLinkLayerCapabilities(netAdapter, &linkLayerCapabilities);

#pragma endregion

#pragma region MTU Size (Required)

    NetAdapterSetLinkLayerMtuSize(netAdapter, RT_MAX_PACKET_SIZE - ETH_LENGTH_OF_HEADER);

#pragma endregion

#pragma region Power Capabilities (Optional)

    NET_ADAPTER_POWER_CAPABILITIES powerCapabilities;
    NET_ADAPTER_POWER_CAPABILITIES_INIT(&powerCapabilities);

    if (adapter->WakeOnMagicPacketEnabled)
    {
        powerCapabilities.SupportedWakePatterns = NET_ADAPTER_WAKE_MAGIC_PACKET;
    }

    NetAdapterSetPowerCapabilities(netAdapter, &powerCapabilities);

#pragma endregion

#pragma region Datapath Capabilities (Optional)

    NET_ADAPTER_DATAPATH_CAPABILITIES dataPathCapabilities;
    NET_ADAPTER_DATAPATH_CAPABILITIES_INIT(&dataPathCapabilities);

    dataPathCapabilities.NumTxQueues = 1;
    dataPathCapabilities.NumRxQueues = 1;

    NetAdapterSetDataPathCapabilities(netAdapter, &dataPathCapabilities);

#pragma endregion

#pragma region Offload Attributes (Legacy)

    NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES offloadAttributes;
    RtlZeroMemory(&offloadAttributes, sizeof(NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES));

    offloadAttributes.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES;
    offloadAttributes.Header.Size = sizeof(NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES);
    offloadAttributes.Header.Revision = NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1;

    NDIS_OFFLOAD hardwareCaps;
    RtAdapterQueryHardwareCapabilities(&hardwareCaps);

    offloadAttributes.HardwareOffloadCapabilities = &hardwareCaps;

    NDIS_OFFLOAD offloadCaps;
    RtAdapterQueryOffloadConfiguration(adapter, &offloadCaps);

    offloadAttributes.DefaultOffloadConfiguration = &offloadCaps;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NdisMSetMiniportAttributes(
            adapter->NdisLegacyAdapterHandle,
            reinterpret_cast<NDIS_MINIPORT_ADAPTER_ATTRIBUTES*>(
                &offloadAttributes)));

#pragma endregion

Exit:
    TraceExitResult(status);

    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtAdapterCreateTxQueue(
    _In_ NETADAPTER netAdapter,
    _Inout_ PNETTXQUEUE_INIT txQueueInit)
{
    NTSTATUS status = STATUS_SUCCESS;

    TraceEntryNetAdapter(netAdapter);

    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

#pragma region Create NETTXQUEUE

    WDF_OBJECT_ATTRIBUTES txAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&txAttributes, RT_TXQUEUE);

    txAttributes.EvtDestroyCallback = EvtTxQueueDestroy;
    
    NET_TX_DMA_QUEUE_CONFIG sgConfig;
    NET_TX_DMA_QUEUE_CONFIG_INIT(
        &sgConfig,
        adapter->WdfDevice,
        adapter->DmaEnabler,
        RT_MAX_PACKET_SIZE,
        EvtTxQueueSetNotificationEnabled,
        EvtTxQueueCancel);

    sgConfig.MaximumScatterGatherElements = RT_MAX_PHYS_BUF_COUNT;
    sgConfig.AllowDmaBypass = TRUE;

    NET_TX_DMA_QUEUE_CONFIG_SET_DEFAULT_PACKET_CONTEXT_TYPE(&sgConfig, RT_TCB);

    NETTXQUEUE txQueue;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetTxDmaQueueCreate(
            txQueueInit,
            &txAttributes,
            &sgConfig,
            &txQueue));

#pragma endregion

#pragma region Initialize RTL8168D Transmit Queue

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtTxQueueInitialize(txQueue, adapter));

#pragma endregion

    WdfSpinLockAcquire(adapter->Lock); {

        RT_TXQUEUE *tx = RtGetTxQueueContext(txQueue);
        RtTxQueueStart(tx);
        adapter->TxQueue = txQueue;

    } WdfSpinLockRelease(adapter->Lock);

Exit:
    TraceExitResult(status);

    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtAdapterCreateRxQueue(
    _In_ NETADAPTER netAdapter,
    _Inout_ PNETRXQUEUE_INIT rxQueueInit)
{
    NTSTATUS status = STATUS_SUCCESS;

    TraceEntryNetAdapter(netAdapter);

    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

#pragma region Create NETRXQUEUE

    WDF_OBJECT_ATTRIBUTES rxAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&rxAttributes, RT_RXQUEUE);

    rxAttributes.EvtDestroyCallback = EvtRxQueueDestroy;

    NET_RXQUEUE_CONFIG rxConfig;
    NET_RXQUEUE_CONFIG_INIT(
        &rxConfig,
        EvtRxQueueAdvance,
        EvtRxQueueSetNotificationEnabled,
        EvtRxQueueCancel);

    rxConfig.AlignmentRequirement = FILE_64_BYTE_ALIGNMENT;
    rxConfig.AllocationSize =
        RT_MAX_PACKET_SIZE + FRAME_CRC_SIZE + RSVD_BUF_SIZE;

    NETRXQUEUE rxQueue;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetRxQueueCreate(rxQueueInit, &rxAttributes, &rxConfig, &rxQueue));

#pragma endregion

#pragma region Initialize RTL8168D Receive Queue

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtRxQueueInitialize(rxQueue, adapter));

#pragma endregion

    WdfSpinLockAcquire(adapter->Lock); {

        // Starting the receive queue must be synchronized with any OIDs that
        // modify the receive queue's behavior.

        RtRxQueueStart(RtGetRxQueueContext(rxQueue));
        adapter->RxQueue = rxQueue;

    } WdfSpinLockRelease(adapter->Lock);

Exit:
    TraceExitResult(status);

    return status;
}

NTSTATUS
RtAdapterReadAddress(
    _In_ RT_ADAPTER *adapter)
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

    for (UINT i = 0; i < ETHERNET_ADDRESS_LENGTH; i++)
    {
        adapter->PermanentAddress[i] = adapter->CSRAddress->ID[i];
    }

    if (ETH_IS_MULTICAST(adapter->PermanentAddress) ||
            ETH_IS_BROADCAST(adapter->PermanentAddress))
    {
        NdisWriteErrorLogEntry(
            adapter->NdisLegacyAdapterHandle,
            NDIS_ERROR_CODE_NETWORK_ADDRESS,
            0);

        GOTO_IF_NOT_NT_SUCCESS(Exit, status, STATUS_NDIS_INVALID_ADDRESS);
    }

    if (!adapter->OverrideAddress)
    {
        memcpy(
            adapter->CurrentAddress,
            adapter->PermanentAddress,
            sizeof(adapter->PermanentAddress));
    }

    TraceLoggingWrite(
        RealtekTraceProvider,
        "PermanentAddressRead",
        TraceLoggingBinary(adapter->CurrentAddress, 6, "CurrentAddress"));

Exit:
    TraceExitResult(status);

    return status;
}

void
RtAdapterRefreshCurrentAddress(
    _In_ RT_ADAPTER *adapter)
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
RtAdapterEnableCR9346Write(_In_ RT_ADAPTER *adapter)
{
    UCHAR ucCr9346Value;

    ucCr9346Value = adapter->CSRAddress->CR9346;
    ucCr9346Value |= (CR9346_EEM1 | CR9346_EEM0);

    adapter->CSRAddress->CR9346 = ucCr9346Value;
}

void
RtAdapterDisableCR9346Write(_In_ RT_ADAPTER *adapter)
{
    UCHAR ucCr9346Value;

    ucCr9346Value = adapter->CSRAddress->CR9346;
    ucCr9346Value &= ~(CR9346_EEM1 | CR9346_EEM0);

    adapter->CSRAddress->CR9346 = ucCr9346Value;
}

void
RtAdapterSetupCurrentAddress(
    _In_ RT_ADAPTER *adapter)
{
    RtAdapterEnableCR9346Write(adapter); {

        for (UINT i = 0; i < ETHERNET_ADDRESS_LENGTH; i++)
        {
            adapter->CSRAddress->ID[i] = adapter->CurrentAddress[i];
        }

    } RtAdapterDisableCR9346Write(adapter);
}

ULONG
ComputeCrc(
    _In_ UCHAR *buffer,
         UINT length)
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
    _In_  UCHAR address[],
    _Out_ UCHAR *byte,
    _Out_ UCHAR *value)
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
         ULONG multiCast_03,
         ULONG multiCast_47)
{
    PULONG pMultiCastReg;

    pMultiCastReg = (PULONG)(&adapter->CSRAddress->MulticastReg0);

    *pMultiCastReg = multiCast_03;
    pMultiCastReg = (PULONG)(&adapter->CSRAddress->MulticastReg4);

    *pMultiCastReg = multiCast_47;
}

void RtAdapterPushMulticastList(_In_ RT_ADAPTER *adapter)
{
    UCHAR Byte;
    UCHAR  Bit;
    ULONG MultiCast_03;
    ULONG MultiCast_47;
    ULONG  AddressCount = adapter->MCAddressCount;
    ULONG  MultiCastR1;
    ULONG  MultiCastR2;

    UCHAR  NicMulticastRegs[MAX_NIC_MULTICAST_REG] = { 0 };

    // Now turn on the bit for each address in the multicast list.
    for (UINT i = 0; (i < AddressCount) && (i < RT_MAX_MCAST_LIST); i++)
    {
        GetMulticastBit(adapter->MCList[i], &Byte, &Bit);
        NicMulticastRegs[Byte] |= Bit;
    }

    //Write Multicast bits to register
    MultiCast_03 = 0;

    MultiCast_47 = 0;

    for (UINT i = 0; i < 4; i++)
    {
        MultiCast_03 = MultiCast_03 + (NicMulticastRegs[i] << (8 * i));
    }

    for (UINT i = 4; i < 8; i++)
    {
        MultiCast_47 = MultiCast_47 + (NicMulticastRegs[i] << (8 * (i - 4)));
    }

    MultiCastR1 = 0;
    MultiCastR2 = 0;

    MultiCastR1 |= (MultiCast_47 & 0x000000ff) << 24;
    MultiCastR1 |= (MultiCast_47 & 0x0000ff00) << 8;
    MultiCastR1 |= (MultiCast_47 & 0x00ff0000) >> 8;
    MultiCastR1 |= (MultiCast_47 & 0xff000000) >> 24;

    MultiCastR2 |= (MultiCast_03 & 0x000000ff) << 24;
    MultiCastR2 |= (MultiCast_03 & 0x0000ff00) << 8;
    MultiCastR2 |= (MultiCast_03 & 0x00ff0000) >> 8;
    MultiCastR2 |= (MultiCast_03 & 0xff000000) >> 24;

    MultiCast_03 = MultiCastR1;
    MultiCast_47 = MultiCastR2;

    RtAdapterSetMulticastReg(adapter, MultiCast_03, MultiCast_47);
}

bool
RtAdapterQueryChipType(_In_ RT_ADAPTER *adapter, _Out_ RT_CHIP_TYPE *chipType)
{
    ULONG MacVer = adapter->CSRAddress->TCR;
    MacVer &= 0x7C800000;

    *chipType = RTLUNKNOWN;

    if (MacVer == 0x28000000)
    {
        *chipType = RTL8168D;
        return true;
    }

    return false;
}

void
RtAdapterSetupHardware(RT_ADAPTER *adapter)
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
    RT_ADAPTER *adapter)
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
RtAdapterUpdateInterruptModeration(_In_ RT_ADAPTER *adapter)
{
    WdfSpinLockAcquire(adapter->Lock); {

        USHORT timerFlags = 0;

        if (adapter->InterruptModerationDisabled ||
            adapter->InterruptModerationMode == RtInterruptModerationOff)
        {
            // No interrupt moderation
            adapter->CSRAddress->IntMiti.RxTimerNum = 0;
            adapter->CSRAddress->IntMiti.TxTimerNum = 0;
        }
        else
        {
            switch (adapter->InterruptModerationMode)
            {
            case RtInterruptModerationLow:
                timerFlags |= CPCR_INT_MITI_TIMER_UNIT_0;
                timerFlags |= CPCR_INT_MITI_TIMER_UNIT_1;

                // Rx: Approximately 500us delay before interrupting
                adapter->CSRAddress->IntMiti.RxTimerNum = 0x30;
                // Tx: Completion isn't time-critical; give it the maximum slack.
                adapter->CSRAddress->IntMiti.TxTimerNum = 0xf0;
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
