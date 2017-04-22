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
    _In_ PMP_ADAPTER adapter,
    _In_ WDFDEVICE device,
    _In_ NETADAPTER netAdapter
    )
/*++
Routine Description:

    Allocate MP_ADAPTER data block and do some initialization

Arguments:

    Adapter     Pointer to receive pointer to our adapter

Return Value:

    NTSTATUS failure code, or STATUS_SUCCESS

--*/
{
    TraceEntry();
        
    NTSTATUS status = STATUS_SUCCESS;

    adapter->Adapter = netAdapter;
    adapter->Device = device;

    //
    // Get WDF miniport device context.
    //
    RtGetDeviceContext(adapter->Device)->Adapter = adapter;

    //spinlock
    WDF_OBJECT_ATTRIBUTES  attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = adapter->Device;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfSpinLockCreate(&attributes, &adapter->Lock));

Exit:
    TraceExitResult(status);

    return status;

}

void
RtAdapterQueryOffloadConfiguration(
    _In_ MP_ADAPTER const *adapter,
    _Out_ NDIS_OFFLOAD *offloadCaps)
{
    RtlZeroMemory(offloadCaps, sizeof(*offloadCaps));

    offloadCaps->Header.Type = NDIS_OBJECT_TYPE_OFFLOAD;
    offloadCaps->Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_5;
    offloadCaps->Header.Revision = NDIS_OFFLOAD_REVISION_5;

    //
    // IPV4 : TX
    //
    offloadCaps->Checksum.IPv4Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (adapter->IPChksumOffv4 == CHKSUM_OFFLOAD_TX_ENABLED ||
        adapter->IPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED)
    {
        offloadCaps->Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->TCPChksumOffv4 == CHKSUM_OFFLOAD_TX_ENABLED ||
        adapter->TCPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED)
    {
        offloadCaps->Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->UDPChksumOffv4 == CHKSUM_OFFLOAD_TX_ENABLED ||
        adapter->UDPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED)
    {
        offloadCaps->Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    //
    // IPV4 : RX
    //
    offloadCaps->Checksum.IPv4Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (adapter->IPChksumOffv4 == CHKSUM_OFFLOAD_RX_ENABLED ||
        adapter->IPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED)
    {
        offloadCaps->Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->TCPChksumOffv4 == CHKSUM_OFFLOAD_RX_ENABLED ||
        adapter->TCPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED)
    {
        offloadCaps->Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    if (adapter->UDPChksumOffv4 == CHKSUM_OFFLOAD_RX_ENABLED ||
        adapter->UDPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED)
    {
        offloadCaps->Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }
}

_Use_decl_annotations_
void
RtAdapterUpdateEnabledChecksumOffloads(_In_ MP_ADAPTER *adapter)
{
    adapter->IpRxHwChkSumv4 =
        (adapter->IPChksumOffv4 == CHKSUM_OFFLOAD_RX_ENABLED ||
            adapter->IPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED);

    adapter->TcpRxHwChkSumv4 =
        (adapter->TCPChksumOffv4 == CHKSUM_OFFLOAD_RX_ENABLED ||
            adapter->TCPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED);

    adapter->UdpRxHwChkSumv4 =
        (adapter->UDPChksumOffv4 == CHKSUM_OFFLOAD_RX_ENABLED ||
            adapter->UDPChksumOffv4 == CHKSUM_OFFLOAD_TX_RX_ENABLED);

    USHORT cpcr = adapter->CSRAddress->CPCR;
    
    // if one kind of offload needed, enable RX HW checksum
    if (adapter->IpRxHwChkSumv4 || adapter->TcpRxHwChkSumv4 || adapter->UdpRxHwChkSumv4)
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
    _Out_ NDIS_OFFLOAD *hardwareCaps)
{
    RtlZeroMemory(hardwareCaps, sizeof(*hardwareCaps));

    hardwareCaps->Header.Type = NDIS_OBJECT_TYPE_OFFLOAD;
    hardwareCaps->Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_5;
    hardwareCaps->Header.Revision = NDIS_OFFLOAD_REVISION_5;

    //
    // ipv4 checksum offload
    //
    hardwareCaps->Checksum.IPv4Transmit.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    hardwareCaps->Checksum.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SUPPORTED;

    hardwareCaps->Checksum.IPv4Receive.Encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;
    hardwareCaps->Checksum.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
    hardwareCaps->Checksum.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
}

_Use_decl_annotations_
NTSTATUS
EvtAdapterSetCapabilities(
    _In_ NETADAPTER netAdapter)
{
    TraceEntryNetAdapter(netAdapter);

    NTSTATUS status = STATUS_SUCCESS;

    PMP_ADAPTER adapter = RtGetAdapterContext(netAdapter);

#pragma region Link Layer Capabilities (Required)

    ULONG64 maxXmitLinkSpeed = NIC_MEDIA_MAX_SPEED;
    ULONG64 maxRcvLinkSpeed = NIC_MEDIA_MAX_SPEED;

    NET_ADAPTER_LINK_LAYER_CAPABILITIES linkLayerCapabilities;
    NET_ADAPTER_LINK_LAYER_CAPABILITIES_INIT(
        &linkLayerCapabilities,
        NIC_SUPPORTED_FILTERS,
        NIC_MAX_MCAST_LIST,
        NIC_SUPPORTED_STATISTICS,
        maxXmitLinkSpeed,
        maxRcvLinkSpeed,
        ETH_LENGTH_OF_ADDRESS,
        adapter->PermanentAddress,
        adapter->CurrentAddress);

    NetAdapterSetLinkLayerCapabilities(netAdapter, &linkLayerCapabilities);

#pragma endregion

#pragma region MTU Size (Required)

    NetAdapterSetLinkLayerMtuSize(netAdapter, NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE);

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

    PMP_ADAPTER adapter = RtGetAdapterContext(netAdapter);

#pragma region Create NETTXQUEUE

    WDF_OBJECT_ATTRIBUTES txAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&txAttributes, RT_TXQUEUE);

    txAttributes.EvtDestroyCallback = EvtTxQueueDestroy;
    
    NET_TX_DMA_QUEUE_CONFIG sgConfig;
    NET_TX_DMA_QUEUE_CONFIG_INIT(
        &sgConfig,
        adapter->Device,
        adapter->DmaEnabler,
        NIC_MAX_PACKET_SIZE,
        EvtTxQueueSetNotificationEnabled,
        EvtTxQueueCancel);

    sgConfig.MaximumScatterGatherElements = NIC_MAX_PHYS_BUF_COUNT;
    sgConfig.AllowDmaBypass = TRUE;

#if _RS2_
    NET_TX_DMA_QUEUE_CONFIG_SET_DEFAULT_PACKET_CONTEXT_TYPE(&sgConfig, RT_TCB);
#else
    NET_PACKET_CONTEXT_ATTRIBUTES contextAttributes;
    NET_PACKET_CONTEXT_ATTRIBUTES_INIT_TYPE(&contextAttributes, RT_TCB);

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetTxQueueInitAddPacketContextAttributes(txQueueInit, &contextAttributes));
#endif

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

    MP_ADAPTER *adapter = RtGetAdapterContext(netAdapter);

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
        NIC_MAX_PACKET_SIZE + FRAME_CRC_SIZE + RSVD_BUF_SIZE;

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

_Use_decl_annotations_
void
RtDestroyAdapterContext(
    _In_ WDFOBJECT netAdapter)
{
    TraceEntryNetAdapter(netAdapter);

    // free any resources owned by MP_ADAPTER here

    TraceExit();
}

void
RtAdapterUpdatePmParameters(
    _In_ PMP_ADAPTER Adapter
)
{
    //
    // Use NETPOWERSETTINGS to check if we should enable wake from magic packet
    //

    NETPOWERSETTINGS powerSettings = NetAdapterGetPowerSettings(Adapter->Adapter);
    ULONG enabledWakePatterns = NetPowerSettingsGetEnabledWakePatterns(powerSettings);

    if (enabledWakePatterns & NET_ADAPTER_WAKE_MAGIC_PACKET)
    {
        EnableMagicPacket(Adapter);
    }
}