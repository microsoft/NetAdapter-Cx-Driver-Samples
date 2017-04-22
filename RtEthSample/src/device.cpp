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

// Forward declarations
static void RtInitializeImr(_In_ RT_INTERRUPT *interrupt);
static USHORT RtCalculateImr(_In_ RT_INTERRUPT *interrupt);

NTSTATUS
RtGetResources(
    _In_ PMP_ADAPTER adapterContext,
    _In_ WDFCMRESLIST resourcesRaw,
    _In_ WDFCMRESLIST resourcesTranslated)
{
    TraceEntry();

    NTSTATUS status = STATUS_SUCCESS;
    ULONG errorCode = 0;
    ULONG errorValue = 0;

    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 buffer[PCI_COMMON_HDR_LENGTH];

    //
    // Make sure the adpater is present
    //

    ULONG bytesWritten = NdisMGetBusData(
        adapterContext->NdisLegacyAdapterHandle,
        PCI_WHICHSPACE_CONFIG,
        FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID),
        buffer,
        PCI_COMMON_HDR_LENGTH);

    PPCI_COMMON_CONFIG pciConfig = reinterpret_cast<PPCI_COMMON_CONFIG>(buffer);

    DBGPRINT(MP_INFO, ("Adapter is found - VendorID/DeviceID=%x/%x\n",
        pciConfig->VendorID, pciConfig->DeviceID));

    // --- HW_START

    USHORT pciCommand = pciConfig->Command;

    // Enable bus matering if it isn't enabled by the BIOS
    if (!(pciCommand & PCI_ENABLE_BUS_MASTER))
    {
        DBGPRINT(MP_WARN, ("Bus master is not enabled by BIOS! pciCommand=%x\n",
            pciCommand));

        NdisStallExecution(1);

        pciCommand |= CMD_BUS_MASTER;
        bytesWritten = NdisMSetBusData(
            adapterContext->NdisLegacyAdapterHandle,
            PCI_WHICHSPACE_CONFIG,
            FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
            &pciCommand,
            sizeof(USHORT));

        NdisStallExecution(1);

        if (bytesWritten != sizeof(USHORT))
        {
            DBGPRINT(MP_ERROR,
                ("NdisMSetBusData (Command) bytesWritten=%d\n", bytesWritten));

            errorCode = NDIS_ERROR_CODE_ADAPTER_NOT_FOUND;
            errorValue = ERRLOG_WRITE_PCI_SLOT_FAILED;

            GOTO_IF_NOT_NT_SUCCESS(Exit, status, STATUS_NDIS_ADAPTER_NOT_FOUND);
        }

        bytesWritten = NdisMGetBusData(
            adapterContext->NdisLegacyAdapterHandle,
            PCI_WHICHSPACE_CONFIG,
            FIELD_OFFSET(PCI_COMMON_CONFIG, Command),
            &pciCommand,
            sizeof(USHORT));

        if (bytesWritten != sizeof(USHORT))
        {
            DBGPRINT(MP_ERROR,
                ("NdisMGetBusData (Command) bytesWritten=%d\n", bytesWritten));

            errorCode = NDIS_ERROR_CODE_ADAPTER_NOT_FOUND;
            errorValue = ERRLOG_READ_PCI_SLOT_FAILED;

            GOTO_IF_NOT_NT_SUCCESS(Exit, status, STATUS_NDIS_ADAPTER_NOT_FOUND);
        }

        if (!(pciCommand & PCI_ENABLE_BUS_MASTER))
        {
            DBGPRINT(MP_ERROR, ("Failed to enable bus master! pciCommand=%x\n",
                pciCommand));

            errorCode = NDIS_ERROR_CODE_ADAPTER_DISABLED;
            errorValue = ERRLOG_BUS_MASTER_DISABLED;

            GOTO_IF_NOT_NT_SUCCESS(Exit, status, STATUS_NDIS_ADAPTER_NOT_FOUND);
        }
    }

    DBGPRINT(MP_INFO, ("Bus master is enabled. pciCommand=%x\n", pciCommand));

    // --- HW_END

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
            //
            // RTL8168D has 2 memory IO regions, first region is MAC regs, second region is MSI-X
            //
            if (memRegCnt == 0)
            {
                NT_ASSERT(rawDescriptor->u.Memory.Length >= NIC_MAP_IOSPACE_LENGTH);

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
        adapterContext->CSRAddress =
        static_cast<PHW_CSR>(
            MmMapIoSpaceEx(
                memAddressTranslated,
                NIC_MAP_IOSPACE_LENGTH,
                PAGE_READWRITE | PAGE_NOCACHE)));

    DBGPRINT(MP_INFO, ("CSRAddress=%p\n", adapterContext->CSRAddress));

Exit:

    if (!NT_SUCCESS(status))
    {
        NdisWriteErrorLogEntry(
            adapterContext->NdisLegacyAdapterHandle,
            errorCode,
            1,
            errorValue);
    }

    TraceExitResult(status);
    return status;
}

NTSTATUS
RtRegisterScatterGatherDma(
    IN PMP_ADAPTER rtAdapter
)
{
    TraceEntryRtAdapter(rtAdapter);

    WdfDeviceSetAlignmentRequirement(rtAdapter->Device, FILE_256_BYTE_ALIGNMENT);

    WDF_DMA_ENABLER_CONFIG dmaEnablerConfig;
    WDF_DMA_ENABLER_CONFIG_INIT(&dmaEnablerConfig, WdfDmaProfileScatterGather64, NIC_MAX_PACKET_SIZE);
    dmaEnablerConfig.Flags |= WDF_DMA_ENABLER_CONFIG_REQUIRE_SINGLE_TRANSFER;
    dmaEnablerConfig.WdmDmaVersionOverride = 3;

    NTSTATUS status = STATUS_SUCCESS;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfDmaEnablerCreate(
            rtAdapter->Device,
            &dmaEnablerConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &rtAdapter->DmaEnabler),
        TraceLoggingRtAdapter(rtAdapter));

Exit:
    if (!NT_SUCCESS(status))
    {
        NdisWriteErrorLogEntry(
            rtAdapter->NdisLegacyAdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            1,
            ERRLOG_OUT_OF_SG_RESOURCES);
    }

    TraceExitResult(status);
    return status;
}

NTSTATUS
RtInitializeHardware(
    _In_ PMP_ADAPTER mpAdapter,
    _In_ WDFCMRESLIST resourcesRaw,
    _In_ WDFCMRESLIST resourcesTranslated)
{
    //
    // Read the registry parameters
    //
    NTSTATUS status = STATUS_SUCCESS;

    mpAdapter->NdisLegacyAdapterHandle = NetAdapterWdmGetNdisHandle(mpAdapter->Adapter);

    GOTO_IF_NOT_NT_SUCCESS(Exit, status, NICReadRegParameters(mpAdapter));

    // Map in phy
    GOTO_IF_NOT_NT_SUCCESS(Exit, status, RtGetResources(mpAdapter, resourcesRaw, resourcesTranslated));

    mpAdapter->Interrupt->IMR = &mpAdapter->CSRAddress->IMR;
    mpAdapter->Interrupt->ISR = &mpAdapter->CSRAddress->ISR;

    if (CheckMacVersion(mpAdapter) == FALSE)
    {
        //
        // Unsupported card
        //
        NdisWriteErrorLogEntry(
            mpAdapter->NdisLegacyAdapterHandle,
            NDIS_ERROR_CODE_ADAPTER_NOT_FOUND,
            0);
        GOTO_IF_NOT_NT_SUCCESS(Exit, status, STATUS_NOT_FOUND);
    }

    SetupHwAfterIdentifyChipType(mpAdapter);

    RtIssueFullReset(mpAdapter);

    PhyPowerUp(mpAdapter);

    RtIssueFullReset(mpAdapter);

    //
    // Read additional info from NIC such as MAC address
    //
    GOTO_IF_NOT_NT_SUCCESS(Exit, status, 
       NICReadAdapterInfo(mpAdapter));

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtRegisterScatterGatherDma(mpAdapter),
        TraceLoggingRtAdapter(mpAdapter));

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtAdapterAllocateTallyBlock(mpAdapter),
        TraceLoggingRtAdapter(mpAdapter));

    //
    // Init the hardware and set up everything
    //
    NICInitializeAdapter(mpAdapter);


    // PHY Version
    MP_WritePhyUshort(mpAdapter, 0x1F, 0x0000);

    mpAdapter->CSRAddress->CPCR = CPCR_INT_MITI_TIMER_UNIT_0;

    // Gathers and indicates current link state to NDIS
    //
    // Normally need to take the adapter lock before updating the NIC's
    // media state, but preparehardware already is serialized against all 
    // other callbacks to the Adapter.

    RtUpdateMediaState(mpAdapter);

    NET_ADAPTER_LINK_STATE linkState;
    RtGetLinkState(mpAdapter, &linkState);

    NetAdapterSetCurrentLinkState(mpAdapter->Adapter, &linkState);

Exit:
    TraceExitResult(status);
    return status;
}

void
RtReleaseHardware(
    _In_ PMP_ADAPTER mpAdapter)
{
    if (mpAdapter->CSRAddress)
    {
        MmUnmapIoSpace(
            mpAdapter->CSRAddress,
            NIC_MAP_IOSPACE_LENGTH);
        mpAdapter->CSRAddress = NULL;
    }
}

_Use_decl_annotations_
NTSTATUS
EvtDevicePrepareHardware(
    _In_ WDFDEVICE device,
    _In_ WDFCMRESLIST resourcesRaw,
    _In_ WDFCMRESLIST resourcesTranslated)
{
    PMP_ADAPTER adapterContext = RtGetDeviceContext(device)->Adapter;

    TraceEntryRtAdapter(adapterContext);

    NTSTATUS status = STATUS_SUCCESS;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status, RtInitializeHardware(adapterContext, resourcesRaw, resourcesTranslated));

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
    PMP_ADAPTER mpAdapter = RtGetDeviceContext(device)->Adapter;

    TraceEntryRtAdapter(mpAdapter);

    RtReleaseHardware(mpAdapter);

    NTSTATUS status = STATUS_SUCCESS;
    TraceExitResult(status);
    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    TraceEntry();

    PMP_ADAPTER mpAdapter = RtGetDeviceContext(Device)->Adapter;

    // Interrupts will be fully enabled in EvtInterruptEnable
    RtInitializeImr(mpAdapter->Interrupt);
    RtAdapterUpdateEnabledChecksumOffloads(mpAdapter);

    if (PreviousState != WdfPowerDeviceD3Final)
    {
        // We're coming back from low power, undo what
        // we did in EvtDeviceD0Exit
        MPSetPowerD0(mpAdapter);
    }

    TraceExit();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
{
    TraceEntry();

    if (TargetState != WdfPowerDeviceD3Final)
    {
        PMP_ADAPTER mpAdapter = RtGetDeviceContext(Device)->Adapter;
        MPSetPowerLow(mpAdapter);
    }

    TraceExit();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtDeviceArmWakeFromSx(
    _In_ WDFDEVICE Device
    )
{
    
    PMP_ADAPTER mpAdapter = RtGetDeviceContext(Device)->Adapter;

    TraceEntryRtAdapter(mpAdapter);

    RtAdapterUpdatePmParameters(mpAdapter);

    TraceExit();
    return STATUS_SUCCESS;
}

VOID
EvtDeviceDisarmWakeFromSx(
    _In_ WDFDEVICE Device
    )
{
    PMP_ADAPTER mpAdapter = RtGetDeviceContext(Device)->Adapter;

    TraceEntryRtAdapter(mpAdapter);

    DisableMagicPacket(mpAdapter);

    TraceExit();
}

void
RtInitializeImr(_In_ RT_INTERRUPT * interrupt)
{
    *interrupt->IMR = 0;
}

USHORT
RtCalculateImr(_In_ RT_INTERRUPT *interrupt)
{
    USHORT imr = RtDefaultInterruptFlags;
    
    if (interrupt->TxNotifyArmed)
    {
        imr |= RtTxInterruptFlags;
    }

    if (interrupt->RxNotifyArmed)
    {
        imr |= RtRxInterruptFlags;
    }

    return imr;
}

void
RtUpdateImr(_In_ RT_INTERRUPT *interrupt)
{
    WdfInterruptAcquireLock(interrupt->Handle); {

        *interrupt->IMR = RtCalculateImr(interrupt);

    } WdfInterruptReleaseLock(interrupt->Handle);
}

_Use_decl_annotations_
NTSTATUS
EvtInterruptEnable(
    _In_ WDFINTERRUPT wdfInterrupt,
    _In_ WDFDEVICE wdfDevice)
{
    UNREFERENCED_PARAMETER((wdfDevice));

    TraceEntry(TraceLoggingPointer(wdfInterrupt));

    // Framework sychronizes EvtInterruptEnable with WdfInterruptAcquireLock
    // so do not grab the lock internally

    RT_INTERRUPT *interrupt = RtGetInterruptContext(wdfInterrupt);
    *interrupt->IMR = RtCalculateImr(interrupt);

    TraceExit();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtInterruptDisable(
    _In_ WDFINTERRUPT wdfInterrupt,
    _In_ WDFDEVICE wdfDevice)
{
    UNREFERENCED_PARAMETER((wdfDevice));

    TraceEntry(TraceLoggingPointer(wdfInterrupt));

    // Framework sychronizes EvtInterruptDisable with WdfInterruptAcquireLock
    // so do not grab the lock internally

    RT_INTERRUPT *interrupt = RtGetInterruptContext(wdfInterrupt);
    *interrupt->IMR = 0;

    TraceExit();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
BOOLEAN
EvtInterruptIsr(
    _In_ WDFINTERRUPT wdfInterrupt,
         ULONG MessageID)
{
    UNREFERENCED_PARAMETER((MessageID));

    RT_INTERRUPT *interrupt = RtGetInterruptContext(wdfInterrupt);

    USHORT interruptStatusRegister = *interrupt->ISR;
    USHORT newWork = interruptStatusRegister & RtExpectedInterruptFlags;

    // Check if our hardware is active
    if (interruptStatusRegister == RtInactiveInterrupt)
    {
        return false;
    }

    // Check if the interrupt was ours
    if (0 == newWork)
    {
        return false;
    }

    // Acknowledge the interrupt
    *interrupt->ISR = interruptStatusRegister;

    // queue up interrupt work
    InterlockedOr16((SHORT volatile *)&interrupt->SavedIsr, newWork);

    // Typically the interrupt lock would be acquired before modifying IMR, but
    // the Interrupt lock is already held for the length of the EvtInterruptIsr.
    USHORT newImr = RtCalculateImr(interrupt);

    // Disable any signals for queued work.
    // The ISR fields that indicate the interrupt reason map directly to the
    // IMR fields that enable them, so the ISR can be simply masked off the IMR
    // to disable those fields that are being serviced.
    newImr &= ~newWork;

    // always re-enable link change notifications
    newImr |= RtDefaultInterruptFlags;

    *interrupt->IMR = newImr;

    if (newWork != 0)
    {
        WdfInterruptQueueDpcForIsr(wdfInterrupt);
    }

    return true;
}

_Use_decl_annotations_
VOID
EvtInterruptDpc(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT AssociatedObject)
{
    UNREFERENCED_PARAMETER(AssociatedObject);

    RT_INTERRUPT *interrupt = RtGetInterruptContext(Interrupt);
    MP_ADAPTER *adapter = interrupt->Adapter;

    USHORT newWork = InterlockedExchange16((SHORT volatile *)&interrupt->SavedIsr, 0);

    if (newWork & RtRxInterruptFlags)
    {
        if (InterlockedExchange(&interrupt->RxNotifyArmed, false))
        {
            NetRxQueueNotifyMoreReceivedPacketsAvailable(adapter->RxQueue);
        }
    }

    if (newWork & RtTxInterruptFlags)
    {
        if (InterlockedExchange(&interrupt->TxNotifyArmed, false))
        {
            NetTxQueueNotifyMoreCompletedPacketsAvailable(adapter->TxQueue);
        }
    }

    if (newWork & ISRIMR_LINK_CHG)
    {
        NET_ADAPTER_LINK_STATE linkState;

        WdfSpinLockAcquire(adapter->Lock); {

            RtUpdateMediaState(adapter);
            RtGetLinkState(adapter, &linkState);

        } WdfSpinLockRelease(adapter->Lock);

        NetAdapterSetCurrentLinkState(adapter->Adapter, &linkState);
    }
}