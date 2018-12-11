/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "trace.h"
#include "power.h"
#include "device.h"
#include "adapter.h"
#include "link.h"
#include "phy.h"
#include "interrupt.h"

void
RtAdapterEnableMagicPacket(_In_ RT_ADAPTER *adapter)
{
    // enable cr9346 before writing config registers
    RtAdapterEnableCR9346Write(adapter); {

        adapter->CSRAddress->CONFIG3 |= CONFIG3_Magic;

    } RtAdapterDisableCR9346Write(adapter);
}

void
RtAdapterDisableMagicPacket(_In_ RT_ADAPTER *adapter)
{
    // enable cr9346 before writing config registers
    RtAdapterEnableCR9346Write(adapter); {

        adapter->CSRAddress->CONFIG3 &= ~CONFIG3_Magic;

    } RtAdapterDisableCR9346Write(adapter);
}


void
RtAdapterRaiseToD0(_In_ RT_ADAPTER *adapter)
{
    RtAdapterSetupHardware(adapter);

    RtAdapterIssueFullReset(adapter);

    RtAdapterPowerUpPhy(adapter);

    RtAdapterResetPhy(adapter);

    RtAdapterIssueFullReset(adapter);

    WdfSpinLockAcquire(adapter->Lock); {

        RtAdapterSetupCurrentAddress(adapter);

    } WdfSpinLockRelease(adapter->Lock);

    RtAdapterPushPhySettings(adapter);

    RtAdapterInitializeAutoNegotiation(adapter);
}

_Use_decl_annotations_
NTSTATUS
EvtDeviceD0Entry(
    _In_ WDFDEVICE wdfDevice,
         WDF_POWER_DEVICE_STATE previousState)
{
    RT_ADAPTER *adapter = RtGetDeviceContext(wdfDevice)->Adapter;

    TraceEntryRtAdapter(
        adapter,
        TraceLoggingUInt32(previousState, "PreviousState"));

    // Interrupts will be fully enabled in EvtInterruptEnable
    RtInterruptInitialize(adapter->Interrupt);
    RtAdapterUpdateHardwareChecksum(adapter);
    RtAdapterUpdateInterruptModeration(adapter);

    if (previousState != WdfPowerDeviceD3Final)
    {
        // We're coming back from low power, undo what
        // we did in EvtDeviceD0Exit
        RtAdapterRaiseToD0(adapter);

        // Set up the multicast list address
        // return to D0, WOL no more require to RX all multicast packets
        RtAdapterPushMulticastList(adapter);

        // Update link state
        // Lock not required because of serialized power transition.
        NET_ADAPTER_LINK_STATE linkState;
        RtAdapterQueryLinkState(adapter, &linkState);

        NetAdapterSetLinkState(adapter->NetAdapter, &linkState);
    }

    adapter->CSRAddress->RMS = RT_MAX_FRAME_SIZE;

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
    RT_ADAPTER *adapter = RtGetDeviceContext(Device)->Adapter;

    TraceEntry();

    if (TargetState != WdfPowerDeviceD3Final)
    {
        NET_ADAPTER_LINK_STATE linkState;
        NET_ADAPTER_LINK_STATE_INIT(
            &linkState,
            NDIS_LINK_SPEED_UNKNOWN,
            MediaConnectStateUnknown,
            MediaDuplexStateUnknown,
            NetAdapterPauseFunctionsUnknown,
            NET_ADAPTER_AUTO_NEGOTIATION_NO_FLAGS);

        NetAdapterSetLinkState(adapter->NetAdapter, &linkState);

        // acknowledge interrupt
        USHORT isr = adapter->CSRAddress->ISR0;
        adapter->CSRAddress->ISR0 = isr;
    }

    TraceExit();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtDeviceArmWakeFromSx(
    _In_ WDFDEVICE wdfDevice)
{
    RT_ADAPTER *adapter = RtGetDeviceContext(wdfDevice)->Adapter;

    TraceEntryRtAdapter(adapter);

    // Use NETPOWERSETTINGS to check if we should enable wake from magic packet
    NETPOWERSETTINGS powerSettings = NetAdapterGetPowerSettings(adapter->NetAdapter);
    ULONG enabledWakePatterns = NetPowerSettingsGetEnabledWakePatternFlags(powerSettings);

    if (enabledWakePatterns & NET_ADAPTER_WAKE_MAGIC_PACKET)
    {
        RtAdapterEnableMagicPacket(adapter);
    }

    TraceExit();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
EvtDeviceDisarmWakeFromSx(
    _In_ WDFDEVICE wdfDevice)
{
    RT_ADAPTER *adapter = RtGetDeviceContext(wdfDevice)->Adapter;

    TraceEntryRtAdapter(adapter);

    RtAdapterDisableMagicPacket(adapter);

    TraceExit();
}
