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

    interrupt->NumInterrupts++;

    USHORT interruptStatusRegister = *interrupt->ISR;
    USHORT newWork = interruptStatusRegister & RtExpectedInterruptFlags;

    // Check if our hardware is active
    if (interruptStatusRegister == RtInactiveInterrupt)
    {
        interrupt->NumInterruptsDisabled++;
        return false;
    }

    // Check if the interrupt was ours
    if (0 == newWork)
    {
        interrupt->NumInterruptsNotOurs++;
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
        if (newWork & RtTxInterruptFlags)
            interrupt->NumTxInterrupts++;
        if (newWork & RtRxInterruptFlags)
            interrupt->NumRxInterrupts++;

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
    RT_ADAPTER *adapter = interrupt->Adapter;

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
        RtAdapterNotifyLinkChange(adapter);
    }
}