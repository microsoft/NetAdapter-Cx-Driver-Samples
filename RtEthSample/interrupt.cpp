/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "trace.h"
#include "interrupt.h"
#include "adapter.h"
#include "link.h"

static
UINT32
RtInterruptIsrGet(
    _In_ RT_INTERRUPT *interrupt,
    _In_ ULONG queueId)
{
    UINT32 isr = 0;

    switch (queueId)
    {

    case 0:
        isr = *interrupt->Isr[queueId].Address16;
        break;

    default:
        if (interrupt->Adapter->RxQueues[queueId])
        {
            isr = *interrupt->Isr[queueId].Address8;
        }
        break;

    }

    return isr;
}

static
VOID
RtInterruptImrPut(
    _In_ RT_INTERRUPT *interrupt,
    _In_ ULONG queueId,
    _In_ UINT32 value)
{
    switch (queueId)
    {
    case 0:
        *interrupt->Imr[queueId].Address16 = (UINT16)value;
        return;
    }

    *interrupt->Imr[queueId].Address8 = (UINT8)value;
}

static
VOID
RtInterruptIsrPut(
    _In_ RT_INTERRUPT *interrupt,
    _In_ ULONG queueId,
    _In_ UINT32 value)
{
    if (value == 0)
    {
        return;
    }

    switch (queueId)
    {
    case 0:
        *interrupt->Isr[queueId].Address16 = (UINT16)value;
        return;
    }

    *interrupt->Isr[queueId].Address8 = (UINT8)value;
}

static
VOID
RtInterruptSecondaryImrUpdate(
    _In_ RT_INTERRUPT *interrupt,
    _In_ ULONG queueId,
    _In_ UINT32 isr,
    _In_ UINT8 imr)
{
    if (interrupt->Adapter->RxQueues[queueId])
    {
        RtInterruptImrPut(interrupt, queueId, imr);
        if (isr)
        {
            interrupt->NumRxInterrupts[queueId]++;
        }
    }
}

NTSTATUS
RtInterruptCreate(
    _In_ WDFDEVICE wdfDevice,
    _In_ RT_ADAPTER *adapter,
    _Out_ RT_INTERRUPT **interrupt)
{
    TraceEntryRtAdapter(adapter);

    *interrupt = nullptr;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, RT_INTERRUPT);

    WDF_INTERRUPT_CONFIG config;
    WDF_INTERRUPT_CONFIG_INIT(&config, EvtInterruptIsr, EvtInterruptDpc);

    config.EvtInterruptEnable = EvtInterruptEnable;
    config.EvtInterruptDisable = EvtInterruptDisable;

    NTSTATUS status = STATUS_SUCCESS;

    WDFINTERRUPT wdfInterrupt;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfInterruptCreate(wdfDevice, &config, &attributes, &wdfInterrupt));

    *interrupt = RtGetInterruptContext(wdfInterrupt);

    (*interrupt)->Adapter = adapter;
    (*interrupt)->Handle = wdfInterrupt;

Exit:

    TraceExitResult(status);
    return status;
}

void
RtInterruptInitialize(_In_ RT_INTERRUPT *interrupt)
{
    RtInterruptImrPut(interrupt, 0, 0);
    RtInterruptImrPut(interrupt, 1, 0);
    RtInterruptImrPut(interrupt, 2, 0);
    RtInterruptImrPut(interrupt, 3, 0);
}

static
UINT16
RtCalculateImr(
    _In_ RT_INTERRUPT *interrupt,
    _In_ ULONG queueId)
{
    UINT16 imr = 0;

    if (queueId == 0)
    {
        imr = RtDefaultInterruptFlags;

        if (interrupt->TxNotifyArmed)
        {
            imr |= RtTxInterruptFlags;
        }

        if (interrupt->RxNotifyArmed[0])
        {
            imr |= RtRxInterruptFlags;
        }
    }
    else if (interrupt->RxNotifyArmed[queueId])
    {
        imr = RtRxInterruptSecondaryFlags;
    }

    return imr;
}

void
RtUpdateImr(
    _In_ RT_INTERRUPT *interrupt,
    _In_ ULONG queueId)
{
    WdfInterruptAcquireLock(interrupt->Handle);

    RtInterruptImrPut(interrupt, queueId, RtCalculateImr(interrupt, queueId));

    WdfInterruptReleaseLock(interrupt->Handle);
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
    RtInterruptImrPut(interrupt, 0, RtCalculateImr(interrupt, 0));
    RtInterruptImrPut(interrupt, 1, RtCalculateImr(interrupt, 1));
    RtInterruptImrPut(interrupt, 2, RtCalculateImr(interrupt, 2));
    RtInterruptImrPut(interrupt, 3, RtCalculateImr(interrupt, 3));

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

    RtInterruptInitialize(RtGetInterruptContext(wdfInterrupt));

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

    interrupt->NumInterrupts++;


    // Check if our hardware is active
    UINT16 isr0 = (UINT16)RtInterruptIsrGet(interrupt, 0);
    if (isr0 == RtInactiveInterrupt)
    {
        interrupt->NumInterruptsDisabled++;
        return false;
    }

    // Always returns 0 if ! RxQueues[N]
    UINT32 isr1 = RtInterruptIsrGet(interrupt, 1);
    UINT32 isr2 = RtInterruptIsrGet(interrupt, 2);
    UINT32 isr3 = RtInterruptIsrGet(interrupt, 3);

    // Acknowledge the interrupt, noop if value == 0
    RtInterruptIsrPut(interrupt, 0, isr0);
    RtInterruptIsrPut(interrupt, 1, isr1);
    RtInterruptIsrPut(interrupt, 2, isr2);
    RtInterruptIsrPut(interrupt, 3, isr3);

    // Discard interrupt status we don't expect
    isr0 &= RtExpectedInterruptFlags;
    isr1 &= RtRxInterruptSecondaryFlags;
    isr2 &= RtRxInterruptSecondaryFlags;
    isr3 &= RtRxInterruptSecondaryFlags;

    // Compress isr bits into a single integer
    UINT32 isrPacked = ISR_PACK(isr0, isr1, isr2, isr3);

    // Check if the interrupt was ours
    if (isrPacked == 0)
    {
        interrupt->NumInterruptsNotOurs++;
        return false;
    }

    // Queue up interrupt work
    InterlockedOr((LONG volatile *)&interrupt->SavedIsr, isrPacked);

    // Typically the interrupt lock would be acquired before modifying IMR, but
    // the Interrupt lock is already held for the length of the EvtInterruptIsr.
    UINT16 imr0 = RtCalculateImr(interrupt, 0);
    UINT8 imr1 = (UINT8)RtCalculateImr(interrupt, 1);
    UINT8 imr2 = (UINT8)RtCalculateImr(interrupt, 2);
    UINT8 imr3 = (UINT8)RtCalculateImr(interrupt, 3);

    // Disable any signals for queued work.
    // The ISR fields that indicate the interrupt reason map directly to the
    // IMR fields that enable them, so the ISR can be simply masked off the IMR
    // to disable those fields that are being serviced.
    imr0 &= ~isr0;
    imr1 &= ~(UINT8)isr1;
    imr2 &= ~(UINT8)isr2;
    imr3 &= ~(UINT8)isr3;

    // always re-enable link change notifications
    imr0 |= RtDefaultInterruptFlags;

    RtInterruptImrPut(interrupt, 0, imr0);
    RtInterruptSecondaryImrUpdate(interrupt, 1, isr1, imr1);
    RtInterruptSecondaryImrUpdate(interrupt, 2, isr2, imr2);
    RtInterruptSecondaryImrUpdate(interrupt, 3, isr3, imr3);

    if (isr0 & RtTxInterruptFlags)
        interrupt->NumTxInterrupts++;
    if (isr0 & RtRxInterruptFlags)
        interrupt->NumRxInterrupts[0]++;

    WdfInterruptQueueDpcForIsr(wdfInterrupt);

    return true;
}

static
void
RtRxNotify(
    _In_ RT_INTERRUPT *interrupt,
    _In_ ULONG queueId
    )
{
    if (InterlockedExchange(&interrupt->RxNotifyArmed[queueId], false))
    {
        NetRxQueueNotifyMoreReceivedPacketsAvailable(
            interrupt->Adapter->RxQueues[queueId]);
    }
}

_Use_decl_annotations_
VOID
EvtInterruptDpc(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT AssociatedObject)
{
    UNREFERENCED_PARAMETER(AssociatedObject);

    RT_INTERRUPT *interrupt = RtGetInterruptContext(Interrupt);
    RT_ADAPTER *adapter = interrupt->Adapter;

    UINT32 isrPacked = InterlockedExchange((LONG volatile *)&interrupt->SavedIsr, 0);
    UINT16 isr0;
    UINT8 isr1, isr2, isr3;
    ISR_UNPACK(isrPacked, isr0, isr1, isr2, isr3);

    if (isr0 & RtRxInterruptFlags)
    {
        RtRxNotify(interrupt, 0);
    }

    if (isr1 & RtRxInterruptSecondaryFlags)
    {
        RtRxNotify(interrupt, 1);
    }

    if (isr2 & RtRxInterruptSecondaryFlags)
    {
        RtRxNotify(interrupt, 2);
    }

    if (isr3 & RtRxInterruptSecondaryFlags)
    {
        RtRxNotify(interrupt, 3);
    }

    if (isr0 & RtTxInterruptFlags)
    {
        if (InterlockedExchange(&interrupt->TxNotifyArmed, false))
        {
            NetTxQueueNotifyMoreCompletedPacketsAvailable(adapter->TxQueues[0]);
        }
    }

    if (isr0 & ISRIMR_LINK_CHG)
    {
        RtAdapterNotifyLinkChange(adapter);
    }
}
