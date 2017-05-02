/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

typedef struct _RT_INTERRUPT
{
    RT_ADAPTER *Adapter;
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

    // Statistical counters, for diagnostics only
    ULONG64 NumInterrupts;
    ULONG64 NumInterruptsNotOurs;
    ULONG64 NumInterruptsDisabled;
    ULONG64 NumRxInterrupts;
    ULONG64 NumTxInterrupts;
} RT_INTERRUPT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RT_INTERRUPT, RtGetInterruptContext);

static const USHORT RtTxInterruptFlags = ISRIMR_TOK | ISRIMR_TER;
static const USHORT RtRxInterruptFlags = ISRIMR_ROK | ISRIMR_RER | ISRIMR_RDU;
static const USHORT RtDefaultInterruptFlags = ISRIMR_LINK_CHG;
static const USHORT RtExpectedInterruptFlags = (RtTxInterruptFlags | RtRxInterruptFlags | RtDefaultInterruptFlags | ISRIMR_RX_FOVW);
static const USHORT RtInactiveInterrupt = 0xFFFF;

NTSTATUS
RtInterruptCreate(
    _In_ WDFDEVICE wdfDevice,
    _In_ RT_ADAPTER *adapter,
    _Out_ RT_INTERRUPT **interrupt);

void RtInterruptInitialize(_In_ RT_INTERRUPT *interrupt);
void RtUpdateImr(_In_ RT_INTERRUPT *interrupt);

EVT_WDF_INTERRUPT_ISR EvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC EvtInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE EvtInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE EvtInterruptDisable;
