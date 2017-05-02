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
#include "power.h"
#include "interrupt.h"

// {5D364AAF-5B49-41A0-9E03-D3CB2AA2E03E}
TRACELOGGING_DEFINE_PROVIDER(
    RealtekTraceProvider,
    "Realtek.Trace.Provider",
    (0x5d364aaf, 0x5b49, 0x41a0, 0x9e, 0x3, 0xd3, 0xcb, 0x2a, 0xa2, 0xe0, 0x3e));

EXTERN_C __declspec(code_seg("INIT")) DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_UNLOAD EvtDriverUnload;
EVT_WDF_DRIVER_DEVICE_ADD EvtDriverDeviceAdd;

_Use_decl_annotations_
__declspec(code_seg("INIT"))
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT driverObject,
    _In_ PUNICODE_STRING registryPath)
{
    BOOLEAN traceLoggingRegistered = FALSE;
    NTSTATUS status = STATUS_SUCCESS;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        TraceLoggingRegister(RealtekTraceProvider));

    traceLoggingRegistered = TRUE;

    TraceEntry(TraceLoggingUnicodeString(registryPath));

    WDF_DRIVER_CONFIG driverConfig;
    WDF_DRIVER_CONFIG_INIT(&driverConfig, EvtDriverDeviceAdd);

    driverConfig.DriverPoolTag = 'RTEK';

    driverConfig.EvtDriverUnload = EvtDriverUnload;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &driverConfig, NULL));

Exit:

    if (traceLoggingRegistered)
    {
        TraceExitResult(status);

        if (!NT_SUCCESS(status))
        {
            TraceLoggingUnregister(RealtekTraceProvider);
        }
    }

    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtDriverDeviceAdd(
    _In_ WDFDRIVER driver,
    _Inout_ PWDFDEVICE_INIT deviceInit)
{
    UNREFERENCED_PARAMETER((driver));

    TraceEntry();

    NTSTATUS status = STATUS_SUCCESS;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetAdapterDeviceInitConfig(deviceInit));

    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, RT_DEVICE);

    {
        WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
        pnpPowerCallbacks.EvtDevicePrepareHardware = EvtDevicePrepareHardware;
        pnpPowerCallbacks.EvtDeviceReleaseHardware = EvtDeviceReleaseHardware;
        pnpPowerCallbacks.EvtDeviceD0Entry = EvtDeviceD0Entry;
        pnpPowerCallbacks.EvtDeviceD0Exit = EvtDeviceD0Exit;

        WdfDeviceInitSetPnpPowerEventCallbacks(deviceInit, &pnpPowerCallbacks);
    }

    {
        WDF_POWER_POLICY_EVENT_CALLBACKS powerPolicyCallbacks;
        WDF_POWER_POLICY_EVENT_CALLBACKS_INIT(&powerPolicyCallbacks);

        powerPolicyCallbacks.EvtDeviceArmWakeFromSx = EvtDeviceArmWakeFromSx;
        powerPolicyCallbacks.EvtDeviceDisarmWakeFromSx = EvtDeviceDisarmWakeFromSx;

        WdfDeviceInitSetPowerPolicyEventCallbacks(deviceInit, &powerPolicyCallbacks);
    }
    
    WDFDEVICE wdfDevice;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfDeviceCreate(&deviceInit, &deviceAttributes, &wdfDevice));

    // Default wake settings is good enough
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfDeviceAssignSxWakeSettings(wdfDevice, &wakeSettings));

    NET_ADAPTER_CONFIG adapterConfig;
    NET_ADAPTER_CONFIG_INIT(
        &adapterConfig,
        EvtAdapterSetCapabilities,
        EvtAdapterCreateTxQueue,
        EvtAdapterCreateRxQueue);

    WDF_OBJECT_ATTRIBUTES adapterAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&adapterAttributes, RT_ADAPTER);

    NETADAPTER netAdapter;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        NetAdapterCreate(wdfDevice, &adapterAttributes, &adapterConfig, &netAdapter));

    RT_ADAPTER *adapter = RtGetAdapterContext(netAdapter);
    RT_DEVICE *device = RtGetDeviceContext(wdfDevice);

    device->Adapter = adapter;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtInitializeAdapterContext(adapter, wdfDevice, netAdapter));

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtInitializeAdapterRequestQueue(adapter));

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        RtInterruptCreate(wdfDevice, adapter, &adapter->Interrupt));

Exit:
    TraceExitResult(status);

    return status;
}

_Use_decl_annotations_
VOID
EvtDriverUnload(
    _In_ WDFDRIVER driver)
{
    UNREFERENCED_PARAMETER((driver));

    TraceEntry();

    TraceLoggingUnregister(RealtekTraceProvider);
}