/*++

Copyright (C) Microsoft Corporation. All rights reserved.

NetAdapterCx NETTXQUEUE DMA Scatter/Gather Framework

    This framework aims to help developers simplify the code they write to
    deal with the DMA engine in a NETTXQUEUE. It creates a queue on behalf
    of the NIC driver and intercepts the EvtTxQueueAdvance callback, breaking
    it down in four function calls the NIC driver should define using macros:

    Macro Name                     | Required   | Type
    -------------------------------|------------|--------------------------------------
    TX_DMA_FX_PROGRAM_DESCRIPTORS  | Yes        | EVT_TX_DMA_QUEUE_PROGRAM_DESCRIPTORS
    TX_DMA_FX_GET_PACKET_STATUS    | Yes        | EVT_TX_DMA_QUEUE_GET_PACKET_STATUS
    TX_DMA_FX_FLUSH_TRANSACTION    | Yes        | EVT_TX_DMA_QUEUE_FLUSH_TRANSACTION
    TX_DMA_FX_BOUNCE_ANALYSIS      | No         | EVT_TX_DMA_QUEUE_BOUNCE_ANALYSIS
    TX_DMA_FX_ALLOC_TAG            | No         | Pool tag to use in internal allocations

    To use this framework the NIC driver should:
        
        - Include txdmafxtypes.h
        - Define at least the required macros
        - Include txdmafx.h

    Example:

        #include "txdmafxtypes.h

        EVT_TX_DMA_QUEUE_PROGRAM_DESCRIPTORS EvtProgramDescriptors;
        EVT_TX_DMA_QUEUE_FLUSH_TRANSACTION EvtFlushTransation;
        EVT_TX_DMA_QUEUE_GET_PACKET_STATUS  EvtGetPacketStatus;

        #define TX_DMA_FX_PROGRAM_DESCRIPTORS EvtProgramDescriptors
        #define TX_DMA_FX_FLUSH_TRANSACTION EvtFlushTransation
        #define TX_DMA_FX_GET_PACKET_STATUS EvtGetPacketStatus

        #define TX_DMA_FX_ALLOC_TAG 'tseT'

        #include "txdmafx.h"


    TX_DMA_FX_PROGRAM_DESCRIPTORS: Called one time for each  NET_PACKET
        that needs to be transmitted. The framework will take care of 
        mapping/unmapping the buffers and will pass in a SCATTER_GATHER_LIST
        along with the packet so that the NIC driver can program the descriptors
        to hardware.

    TX_DMA_FX_GET_PACKET_STATUS: Called one time for each NET_PACKET pending
        transmission. If this function returns STATUS_SUCCESS, the framework will
        release any resources it acquired to map the buffers and return the packet
        to the OS.

        If this function returns STATUS_PENDING the packet is not completed, any
        other status code will cause the packet to be returned to the OS and will
        be counted as a failed completion.

    TX_DMA_FX_FLUSH_TRANSACTION: Called once per EvtTxQueueAdvance callback
        if any new descriptors were programmed to hardware. The NIC should do 
        whatever it is necessary to flush their DMA transaction.

    TX_DMA_FX_BOUNCE_ANALYSIS: If the internal framework bounce analysis is not 
        enough for the NIC driver, it can use this function to implement any checks
        relevant to their hardware. Called once for each NET_PACKET that needs to 
        be transmitted.
        
    To see what kind of bounce analysis the framework does see NET_SCATTER_GATHER_TXQUEUE_CONFIG
    definition or _TxDmaFxBounceAnalysis implementation.

--*/

#ifndef _TXDMAFX_H_
#define _TXDMAFX_H_

#include "txdmafx_details.h"

#ifndef TX_DMA_FX_PROGRAM_DESCRIPTORS
#error To use this framework you need to define TX_DMA_FX_PROGRAM_DESCRIPTORS
#endif

#ifndef TX_DMA_FX_GET_PACKET_STATUS
#error To use this framework you need to define TX_DMA_FX_GET_PACKET_STATUS
#endif

#ifndef TX_DMA_FX_FLUSH_TRANSACTION
#error To use this framework you need to define TX_DMA_FX_FLUSH_TRANSACTION
#endif

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
VOID
_TxDmaFxCompleteTxPackets(
    _In_ TxDmaFx *DmaFx
    )
/*

Description:

    Iterates over the packets in the ring buffer that belong to the NIC driver and
    were already programmed to hardware. It then calls TX_DMA_FX_GET_PACKET_STATUS
    to let the NIC driver check if the DMA transfer is complete or not. If it is, any
    resources associated with the transfer are returned and the packet is marked
    as complete.

*/
{
    NET_RING_BUFFER *ringBuffer = DmaFx->RingBuffer;

    while (ringBuffer->BeginIndex != ringBuffer->NextIndex)
    {
        NET_PACKET *packet = NetRingBufferGetPacketAtIndex(ringBuffer, ringBuffer->BeginIndex);

        // If the packet is already marked as completed it is because we
        // failed to program its descriptors and we should just drop it
        if (!packet->Data.Completed)
        {
            NTSTATUS packetStatus = 
                TX_DMA_FX_GET_PACKET_STATUS(
                    DmaFx->QueueHandle,
                    packet);
            
            // We need to complete packets in order, if the current returned
            // pending there is no point in keep trying
            if (packetStatus == STATUS_PENDING)
                break;

            if (packetStatus != STATUS_SUCCESS)
                DmaFx->Statistics.Packet.CompletedWithError += 1;

            if (!DmaFx->DmaBypass)
            {
                // If we are using DMA APIs make sure we return the resources we
                // acquired
                TX_DMA_FX_PACKET_CONTEXT *fxPacketContext = _TxDmaFxGetPacketContext(packet, DmaFx);

                // Even when using DMA APIs, we might still have a NULL SGL if
                // the packet was bounced
                if (fxPacketContext->ScatterGatherList != NULL)
                {
                    DmaFx->DmaAdapter->DmaOperations->PutScatterGatherList(
                        DmaFx->DmaAdapter,
                        fxPacketContext->ScatterGatherList,
                        TRUE);

                    fxPacketContext->ScatterGatherList = NULL;
                }
            }            

            if (_TX_DMA_FX_IS_PACKET_BOUNCED(packet))
            {
                DmaFx->BounceFreeIndex += 1;
                _TX_DMA_FX_PACKET_CLEAR_BOUNCED_FLAG(packet);
            }
        }

        ringBuffer->BeginIndex = NetRingBufferIncrementIndex(ringBuffer, ringBuffer->BeginIndex);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
ULONG
_TxDmaFxTransmitPackets(
    _In_ TxDmaFx *DmaFx
    )
/*

Description:
    
    This function iterates over the packets in the ring buffer
    owned by the NIC but not yet programmed to hardware. It then
    performs an internal bounce analysis to decide if the Tx buffer
    needs to be bounced before transmiting or not. Lastly it calls a
    function to handle the transmit operation.

    The NIC driver can optionally register a TX_DMA_FX_BOUNCE_ANALYSIS
    callback in which it can analyze the Tx buffers and decide if it they
    need to be bounced or not.

*/
{
    ULONG numPacketsProgrammed = 0;
    NET_PACKET *netPacket;

    NET_RING_BUFFER *ringBuffer = DmaFx->RingBuffer;

    while (NULL != (netPacket = NetRingBufferGetNextPacket(ringBuffer)))
    {
        NTSTATUS status = STATUS_SUCCESS;

        if (netPacket->IgnoreThisPacket)
        {
            DmaFx->Statistics.Packet.Skipped += 1;
            status = STATUS_UNSUCCESSFUL;
        }
        else
        {
            TX_DMA_BOUNCE_ANALYSIS bounceAnalysis = _TxDmaFxBounceAnalysis(DmaFx, netPacket);

            if (bounceAnalysis == TxDmaTransmitInPlace)
            {
                #ifdef TX_DMA_FX_BOUNCE_ANALYSIS
                bounceAnalysis = TX_DMA_FX_BOUNCE_ANALYSIS(DmaFx->QueueHandle, netPacket);
                #endif
            }

            switch (bounceAnalysis)
            {
                case TxDmaTransmitInPlace:
                {
                    status = _TxDmaFxMapAndTransmitPacket(DmaFx, netPacket);
                    break;
                }
                case TxDmaTransmitAfterBouncing:
                {
                    status = _TxDmaFxBounceAndTransmitPacket(DmaFx, netPacket);
                    break;
                }
                case TxDmaCannotTransmit:
                    DmaFx->Statistics.Packet.CannotTransmit += 1;
                    __fallthrough;
                default:
                    status = STATUS_UNSUCCESSFUL;
                    break;
            }
        }

        // If DMA doesn't have enough buffers for us, 
        // or if we didn't have enough bounce buffers
        // give up now and try again when resources
        // are available.
        if (status == STATUS_INSUFFICIENT_RESOURCES)
            break;

        if (status == STATUS_SUCCESS)
            numPacketsProgrammed++;
        else
            netPacket->Data.Completed = TRUE;

        NetRingBufferAdvanceNextPacket(ringBuffer);
    }

    return numPacketsProgrammed;
}

_Use_decl_annotations_
__inline
VOID
_TxDmaFxAdvance(
    _In_ NETTXQUEUE TxQueue
    )
/*

Description:
    
    This function handles the NETTXQUEUE EvtTxQueueAdvance on behalf of the
    NIC driver. If any number of packets are programmed to hardware during
    this callback we call TX_DMA_FX_FLUSH_TRANSACTION to let the consumer
    of this framework do whatever is necessary to flush their DMA transaction.

Arguments:

    TxQueue - NETTXQUEUE handle

*/
{
    TxDmaFx *TxDmaFx = _TxDmaFxGetContext(TxQueue);

    ULONG numPacketsProgrammed = _TxDmaFxTransmitPackets(TxDmaFx);

    if (numPacketsProgrammed > 0)
        TX_DMA_FX_FLUSH_TRANSACTION(TxDmaFx->QueueHandle);

    _TxDmaFxCompleteTxPackets(TxDmaFx);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
__inline
NetTxDmaQueueCreate(
    _Inout_ PNETTXQUEUE_INIT NetTxQueueInit,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES TxQueueAttributes,
    _In_ PNET_TX_DMA_QUEUE_CONFIG Configuration,
    _Out_ NETTXQUEUE* TxQueue
    )
/*

Description:

    This function will create a NETTXQUEUE on behalf of the caller and set up
    the necessary state to intercept incoming NET_PACKETs and map the buffers
    to use in DMA transactions.

Arguments:

    NetTxQueueInit    - Opaque handle containing information about the queue the OS
                        is asking us to create

    TxQueueAttributes - Object attributes the NIC driver wants in the NETTXQUEUE

    Configuration     - Contains configuration this framework will use to make decisions
                        about what to do with incoming NET_PACKETs as well as the
                        callback functions needed to operate this queue

    TxQueue           - Handle to the created NETTXQUEUE

*/
{
    *TxQueue = NULL;

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(_TxDmaFxValidateConfig(Configuration));
    
    BOOLEAN dmaBypass;

    if (Configuration->AllowDmaBypass)
    {
        _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
            _TxDmaFxCheckDmaBypass(
                Configuration->DmaEnabler,
                &dmaBypass));
    }
    else
    {
        dmaBypass = FALSE;
    }

    // The framework only intercepts the Advance callback (to map and unmap the 
    // incoming packets), the others go directly to the ones the NIC driver provides
    NET_TXQUEUE_CONFIG txConfig;
    NET_TXQUEUE_CONFIG_INIT(
        &txConfig,
        _TxDmaFxAdvance,
        Configuration->EvtTxQueueSetNotificationEnabled,
        Configuration->EvtTxQueueCancel);

    ULONG_PTR fxPacketContextOffset = 0;
    // We only need a packet context if using DMA APIs
    if (!dmaBypass)
    {
        ULONG_PTR alignedClientContextSize = ALIGN_UP_BY(Configuration->ContextTypeInfo->ContextSize, __alignof(_TX_DMA_FX_PACKET_CONTEXT));
        fxPacketContextOffset = alignedClientContextSize;

        ULONG_PTR newContextSize = alignedClientContextSize + sizeof(_TX_DMA_FX_PACKET_CONTEXT);

        WDF_OBJECT_CONTEXT_TYPE_INFO *typeInfo = (WDF_OBJECT_CONTEXT_TYPE_INFO *)Configuration->ContextTypeInfo;
        typeInfo->ContextSize = newContextSize;
    }

    txConfig.ContextTypeInfo = Configuration->ContextTypeInfo;

    // Configure a private Tx Queue context to hold DMA information, the NIC
    // is not allowed to modify the data stored in it
    WDF_OBJECT_ATTRIBUTES privateAttribs;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&privateAttribs, TxDmaFx);

    NETTXQUEUE txQueue;
    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        NetTxQueueCreate(
            NetTxQueueInit,
            &privateAttribs,
            &txConfig,
            &txQueue));

    // Initialize TxDmaFx object
    TxDmaFx *dmaFx = _TxDmaFxGetContext(txQueue);

    dmaFx->QueueHandle = txQueue;
    dmaFx->RingBuffer = NetTxQueueGetRingBuffer(txQueue);
    dmaFx->Config = *Configuration;

    if (!dmaBypass)
    {
        dmaFx->FxPacketContextOffset = fxPacketContextOffset;
        dmaFx->FxPacketContextTypeInfo = Configuration->ContextTypeInfo;
    }

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        _TxDmaFxInitialize(
            dmaFx, 
            dmaBypass));

    // Now allocate space for the NIC context (if any)
    if (TxQueueAttributes != NULL)
    {
        _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
            WdfObjectAllocateContext(
                txQueue,
                TxQueueAttributes,
                NULL));
    }

    *TxQueue = txQueue;

    return STATUS_SUCCESS;
}

#endif