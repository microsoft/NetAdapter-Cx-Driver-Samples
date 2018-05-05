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
#include "configuration.h"
#include "statistics.h"
#include "interrupt.h"
#include "link.h"
#include "phy.h"
#include "eeprom.h"

NTSTATUS
RtGetResources(
    _In_ RT_ADAPTER *adapter,
    _In_ WDFCMRESLIST resourcesRaw,
    _In_ WDFCMRESLIST resourcesTranslated)
{
    TraceEntry();

    NTSTATUS status = STATUS_SUCCESS;
    ULONG errorCode = 0;
    ULONG errorValue = 0;

    bool hasMemoryResource = false;
    ULONG memRegCnt = 0;

    // According to https://msdn.microsoft.com/windows/hardware/drivers/wdf/raw-and-translated-resources
    // "Both versions represent the same set of hardware resources, in the same order."
    ULONG rawCount = WdfCmResourceListGetCount(resourcesRaw);
    NT_ASSERT(rawCount == WdfCmResourceListGetCount(resourcesTranslated));

    PHYSICAL_ADDRESS memAddressTranslated;
    memAddressTranslated = {};

    for (ULONG i = 0; i < rawCount; i++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR rawDescriptor = WdfCmResourceListGetDescriptor(resourcesRaw, i);
        PCM_PARTIAL_RESOURCE_DESCRIPTOR translatedDescriptor = WdfCmResourceListGetDescriptor(resourcesTranslated, i);

        if (rawDescriptor->Type == CmResourceTypeMemory)
        {
            // RTL8168D has 2 memory IO regions, first region is MAC regs, second region is MSI-X
            if (memRegCnt == 0)
            {
                NT_ASSERT(rawDescriptor->u.Memory.Length >= sizeof(RT_MAC));

                memAddressTranslated = translatedDescriptor->u.Memory.Start;
                hasMemoryResource = true;
            }

            memRegCnt++;
        }
    }

    if (!hasMemoryResource)
    {
        status = STATUS_RESOURCE_TYPE_NOT_FOUND;
        errorCode = NDIS_ERROR_CODE_RESOURCE_CONFLICT;

        errorValue = ERRLOG_NO_MEMORY_RESOURCE;

        GOTO_IF_NOT_NT_SUCCESS(Exit, status, STATUS_NDIS_RESOURCE_CONFLICT);
    }

    GOTO_WITH_INSUFFICIENT_RESOURCES_IF_NULL(Exit, status,
        adapter->CSRAddress =
        static_cast<RT_MAC*>(
            MmMapIoSpaceEx(
                memAddressTranslated,
                sizeof(RT_MAC),
                PAGE_READWRITE | PAGE_NOCACHE)));

Exit:

    if (!NT_SUCCESS(status))
    {
        NdisWriteErrorLogEntry(
            adapter->NdisLegacyAdapterHandle,
            errorCode,
            1,
            errorValue);
    }

    TraceExitResult(status);
    return status;
}

NTSTATUS
RtRegisterScatterGatherDma(
    _In_ RT_ADAPTER *adapter)
{
    TraceEntryRtAdapter(adapter);

    WDF_DMA_ENABLER_CONFIG dmaEnablerConfig;
    WDF_DMA_ENABLER_CONFIG_INIT(&dmaEnablerConfig, WdfDmaProfileScatterGather64, RT_MAX_PACKET_SIZE);
    dmaEnablerConfig.Flags |= WDF_DMA_ENABLER_CONFIG_REQUIRE_SINGLE_TRANSFER;
    dmaEnablerConfig.WdmDmaVersionOverride = 3;

    NTSTATUS status = STATUS_SUCCESS;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfDmaEnablerCreate(
            adapter->WdfDevice,
            &dmaEnablerConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &adapter->DmaEnabler),
        TraceLoggingRtAdapter(adapter));

Exit:
    if (!NT_SUCCESS(status))
    {
        NdisWriteErrorLogEntry(
            adapter->NdisLegacyAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            1,
            ERRLOG_OUT_OF_SG_RESOURCES);
    }

    TraceExitResult(status);
    return status;
}

NTSTATUS
RtInitializeHardware(
    _In_ RT_ADAPTER *adapter,
    _In_ WDFCMRESLIST resourcesRaw,
    _In_ WDFCMRESLIST resourcesTranslated)
{
    //
    // Read the registry parameters
    //
    NTSTATUS status = STATUS_SUCCESS;

    adapter->NdisLegacyAdapterHandle =
        NetAdapterWdmGetNdisHandle(adapter->NetAdapter);

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtAdapterReadConfiguration(adapter));

    // Map in phy
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtGetResources(adapter, resourcesRaw, resourcesTranslated));

    adapter->Interrupt->Imr[0].Address16 = &adapter->CSRAddress->IMR0;
    adapter->Interrupt->Imr[1].Address8 = &adapter->CSRAddress->IMR1;
    adapter->Interrupt->Imr[2].Address8 = &adapter->CSRAddress->IMR2;
    adapter->Interrupt->Imr[3].Address8 = &adapter->CSRAddress->IMR3;

    adapter->Interrupt->Isr[0].Address16 = &adapter->CSRAddress->ISR0;
    adapter->Interrupt->Isr[1].Address8 = &adapter->CSRAddress->ISR1;
    adapter->Interrupt->Isr[2].Address8 = &adapter->CSRAddress->ISR2;
    adapter->Interrupt->Isr[3].Address8 = &adapter->CSRAddress->ISR3;

    if (!RtAdapterQueryChipType(adapter, &adapter->ChipType))
    {
        //
        // Unsupported card
        //
        NdisWriteErrorLogEntry(
            adapter->NdisLegacyAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            0);
        GOTO_IF_NOT_NT_SUCCESS(Exit, status, STATUS_NOT_FOUND);
    }

    TraceLoggingWrite(
        RealtekTraceProvider,
        "ChipType",
        TraceLoggingUInt32(adapter->ChipType));

    UINT16 eepromId, pciId;
    if (!RtAdapterReadEepromId(adapter, &eepromId, &pciId))
    {
        adapter->EEPROMSupported = false;
    }
    else
    {
        TraceLoggingWrite(
            RealtekTraceProvider,
            "EepromId",
            TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
            TraceLoggingUInt32(eepromId),
            TraceLoggingUInt32(pciId));

        adapter->EEPROMSupported = (eepromId == 0x8129 && pciId == 0x10ec);
    }
    
    if (!adapter->EEPROMSupported)
    {
        TraceLoggingWrite(
            RealtekTraceProvider,
            "UnsupportedEEPROM",
            TraceLoggingLevel(TRACE_LEVEL_WARNING));
    }

    RtAdapterSetupHardware(adapter);

    // Push SpeedDuplex PHY settings
    RtAdapterPushPhySettings(adapter);

    RtAdapterIssueFullReset(adapter);

    RtAdapterPowerUpPhy(adapter);

    RtAdapterIssueFullReset(adapter);

    //
    // Read additional info from NIC such as MAC address
    //
    GOTO_IF_NOT_NT_SUCCESS(Exit, status, 
       RtAdapterReadAddress(adapter));

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtRegisterScatterGatherDma(adapter),
        TraceLoggingRtAdapter(adapter));

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtAdapterInitializeStatistics(adapter),
        TraceLoggingRtAdapter(adapter));

    // Init the hardware and set up everything
    RtAdapterRefreshCurrentAddress(adapter);

    // PHY Version
    RtAdapterWritePhyUint16(adapter, 0x1F, 0x0000);

    adapter->CSRAddress->CPCR = 0;

    // Gathers and indicates current link state to NDIS
    //
    // Normally need to take the adapter lock before updating the NIC's
    // media state, but preparehardware already is serialized against all 
    // other callbacks to the NetAdapter.

    NET_ADAPTER_LINK_STATE linkState;
    RtAdapterQueryLinkState(adapter, &linkState);

    NetAdapterSetCurrentLinkState(adapter->NetAdapter, &linkState);

Exit:
    TraceExitResult(status);
    return status;
}

void
RtReleaseHardware(
    _In_ RT_ADAPTER *adapter)
{
    if (adapter->CSRAddress)
    {
        MmUnmapIoSpace(
            adapter->CSRAddress,
            sizeof(RT_MAC));
        adapter->CSRAddress = NULL;
    }
}

_Use_decl_annotations_
NTSTATUS
EvtDevicePrepareHardware(
    _In_ WDFDEVICE device,
    _In_ WDFCMRESLIST resourcesRaw,
    _In_ WDFCMRESLIST resourcesTranslated)
{
    RT_ADAPTER *adapter = RtGetDeviceContext(device)->Adapter;

    TraceEntryRtAdapter(adapter);

    NTSTATUS status = STATUS_SUCCESS;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status, RtInitializeHardware(adapter, resourcesRaw, resourcesTranslated));

Exit:
    TraceExitResult(status);
    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtDeviceReleaseHardware(
    _In_ WDFDEVICE device,
    _In_ WDFCMRESLIST resourcesTranslated)
{
    UNREFERENCED_PARAMETER(resourcesTranslated);
    RT_ADAPTER *adapter = RtGetDeviceContext(device)->Adapter;

    TraceEntryRtAdapter(adapter);

    RtReleaseHardware(adapter);

    NTSTATUS status = STATUS_SUCCESS;
    TraceExitResult(status);
    return status;
}
