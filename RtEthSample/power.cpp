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
            NetAdapterPauseFunctionTypeUnknown,
            NetAdapterAutoNegotiationFlagNone);

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

    // Iterate over the wake source list and look for the wake on magic packet
    // entry. If the device supports more power capabilities it can expect to
    // find other types of wake sources.

    NET_WAKE_SOURCE_LIST wakeSourceList;
    NET_WAKE_SOURCE_LIST_INIT(&wakeSourceList);

    NetDeviceGetWakeSourceList(wdfDevice, &wakeSourceList);

    for (SIZE_T i = 0; i < NetWakeSourceListGetCount(&wakeSourceList); i++)
    {
        NETWAKESOURCE wakeSource = NetWakeSourceListGetElement(&wakeSourceList, i);
        NET_WAKE_SOURCE_TYPE const wakeSourceType = NetWakeSourceGetType(wakeSource);

        if (wakeSourceType == NetWakeSourceTypeMagicPacket)
        {
            RtAdapterEnableMagicPacket(adapter);
        }
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
