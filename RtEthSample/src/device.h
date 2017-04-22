/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

typedef struct _RT_DEVICE
{
    PMP_ADAPTER Adapter;
} RT_DEVICE, *PRT_DEVICE;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_DEVICE, RtGetDeviceContext);

typedef struct _RT_INTERRUPT
{
    PMP_ADAPTER Adapter;
    WDFINTERRUPT Handle;

    // Armed Notifications
    LONG RxNotifyArmed;
    LONG TxNotifyArmed;

    // Fired Notificiations
    // Tracks un-served ISR interrupt fields. Masks in only
    // the RtExpectedInterruptFlags
    USHORT SavedIsr;

    USHORT volatile * IMR;
    USHORT volatile * ISR;
} RT_INTERRUPT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_INTERRUPT, RtGetInterruptContext);

EVT_WDF_DEVICE_PREPARE_HARDWARE     EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE     EvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY             EvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT              EvtDeviceD0Exit;

EVT_WDF_DEVICE_ARM_WAKE_FROM_SX     EvtDeviceArmWakeFromSx;
EVT_WDF_DEVICE_DISARM_WAKE_FROM_SX  EvtDeviceDisarmWakeFromSx;

static const USHORT RtTxInterruptFlags  = ISRIMR_TOK | ISRIMR_TER;
static const USHORT RtRxInterruptFlags  = ISRIMR_ROK | ISRIMR_RER | ISRIMR_RDU;
static const USHORT RtDefaultInterruptFlags = ISRIMR_LINK_CHG;
static const USHORT RtExpectedInterruptFlags = (RtTxInterruptFlags | RtRxInterruptFlags | RtDefaultInterruptFlags | ISRIMR_RX_FOVW);
static const USHORT RtInactiveInterrupt = 0xFFFF;

void RtUpdateImr(_In_ RT_INTERRUPT *interrupt);

EVT_WDF_INTERRUPT_ISR EvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC EvtInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE EvtInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE EvtInterruptDisable;
