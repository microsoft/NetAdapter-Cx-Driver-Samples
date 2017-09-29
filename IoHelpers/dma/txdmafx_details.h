// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef _TXDMAFX_DETAILS_H_
#define _TXDMAFX_DETAILS_H_

_IRQL_requires_max_(PASSIVE_LEVEL)
__inline
NTSTATUS
_TxDmaFxCheckDmaBypass(
    _In_ WDFDMAENABLER Enabler,
    _Out_ BOOLEAN *DmaBypass
    )
{
    DMA_ADAPTER *dmaAdapter = WdfDmaEnablerWdmGetDmaAdapter(Enabler, WdfDmaDirectionWriteToDevice);

    DMA_ADAPTER_INFO adapterInfo = { DMA_ADAPTER_INFO_VERSION1 };

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        dmaAdapter->DmaOperations->GetDmaAdapterInfo(
            dmaAdapter,
            &adapterInfo));

    *DmaBypass = (0 != (adapterInfo.V1.Flags & ADAPTER_INFO_API_BYPASS));

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
__inline
NTSTATUS
_TxDmaFxValidateConfig(
    _In_ NET_TX_DMA_QUEUE_CONFIG *Config
    )
/*

Description:

    Validades the configuration object provided by
    the NIC driver.

*/
{
    // Check mandatory event callbacks
    if (Config->EvtTxQueueSetNotificationEnabled == NULL ||
        Config->EvtTxQueueCancel == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // We need a WDFDEVICE and WDFDMAENABLER to use DMA APIs, check they
    // are present
    if (Config->Device == NULL || Config->DmaEnabler == NULL)
        return STATUS_INVALID_PARAMETER;

    // Check if AlignmentRequirement is a 2^N - 1 number
    if (Config->AlignmentRequirement != -1 && (Config->AlignmentRequirement & (Config->AlignmentRequirement + 1)) != 0)
        return STATUS_INVALID_PARAMETER;

    // Check if a maximum packet size is given, the framework needs this information
    // to make internal allocations
    if (Config->MaximumPacketSize == 0)
        return STATUS_INVALID_PARAMETER;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
__inline
NTSTATUS
_TxDmaFxInitializeForDirectMapping(
    _In_ TxDmaFx *DmaFx
    )
/*

Description:

    Initializes TxDmaFx with the necessary resources
    to do direct mapping of buffers.

*/
{
    DmaFx->DmaBypass = TRUE;

    size_t sglAllocationSize;

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        RtlSizeTMult(
            sizeof(SCATTER_GATHER_ELEMENT),
            DmaFx->Config.MaximumScatterGatherElements,
            &sglAllocationSize));

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        RtlSizeTAdd(
            sglAllocationSize,
            FIELD_OFFSET(SCATTER_GATHER_LIST, Elements),
            &sglAllocationSize));

    // Parent the memory allocation to the NETTXQUEUE
    WDF_OBJECT_ATTRIBUTES memoryAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
    memoryAttributes.ParentObject = DmaFx->QueueHandle;

    WDFMEMORY sglAllocation;
    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        WdfMemoryCreate(
            &memoryAttributes,
            NonPagedPoolNx,
            TX_DMA_FX_ALLOC_TAG,
            sglAllocationSize,
            &sglAllocation,
            (void**)&DmaFx->SpareSgl));

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
__inline
NTSTATUS
_TxDmaFxInitializeForDma(
    _In_ TxDmaFx *DmaFx
    )
/*

Description:

    Initializes TxDmaFx with the necessary resources
    to use with DMA APIs when mapping buffers.

*/
{
    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        DmaFx->DmaAdapter->DmaOperations->CalculateScatterGatherList(
            DmaFx->DmaAdapter,
            NULL,
            ULongToPtr(PAGE_SIZE - 1),
            DmaFx->Config.MaximumPacketSize,
            &DmaFx->ScatterGatherListSize,
            NULL));

    //
    // Allocate memory for scatter-gather list
    //
    NET_RING_BUFFER *ringBuffer = DmaFx->RingBuffer;
    size_t memSize;

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        RtlSizeTMult(
            ringBuffer->NumberOfElements,
            DmaFx->ScatterGatherListSize,
            &memSize));

    // Parent the memory allocation to the NETTXQUEUE
    WDF_OBJECT_ATTRIBUTES memoryAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
    memoryAttributes.ParentObject = DmaFx->QueueHandle;

    WDFMEMORY memory;
    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        WdfMemoryCreate(
            &memoryAttributes,
            NonPagedPoolNx,
            TX_DMA_FX_ALLOC_TAG,
            memSize,
            &memory,
            &DmaFx->SgListMem));

    RtlZeroMemory(DmaFx->SgListMem, memSize);

    size_t dmaAllocationSize;

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        RtlSizeTMult(
            DMA_TRANSFER_CONTEXT_SIZE_V1,
            ringBuffer->NumberOfElements,
            &dmaAllocationSize));

    WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
    memoryAttributes.ParentObject = DmaFx->QueueHandle;

    void *dmaArray;
    WDFMEMORY dmaAllocation;
    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        WdfMemoryCreate(
            &memoryAttributes,
            NonPagedPoolNx,
            TX_DMA_FX_ALLOC_TAG,
            dmaAllocationSize,
            &dmaAllocation,
            &dmaArray));

    // Initialize the private packet context for each packet in the ring buffer
    for (UINT32 i = 0; i < ringBuffer->NumberOfElements; i++)
    {
        NET_PACKET *packet = NetRingBufferGetPacketAtIndex(ringBuffer, i);
        TX_DMA_FX_PACKET_CONTEXT *fxPacketContext = _TxDmaFxGetPacketContextFromToken(packet, DmaFx->ContextToken);

        fxPacketContext->ScatterGatherBuffer = (PUCHAR)DmaFx->SgListMem + i * DmaFx->ScatterGatherListSize;
        fxPacketContext->DmaTransferContext = (UCHAR*)dmaArray + i * DMA_TRANSFER_CONTEXT_SIZE_V1;
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
__inline
NTSTATUS
_TxDmaFxInitializeBounceBuffers(
    _In_ TxDmaFx *DmaFx
    )
/*

Description:

    Allocates the buffers used to bounce packets.

*/
{
    // Make sure the bounce buffer size is aligned to something, if
    // the NIC driver provided an alignment requirement, use that value,
    // otherwise use MEMORY_ALLOCATION_ALIGNMENT
    ULONG_PTR alignUpBy = DmaFx->Config.AlignmentRequirement != -1 
        ? DmaFx->Config.AlignmentRequirement + 1
        : MEMORY_ALLOCATION_ALIGNMENT;

    ULONG_PTR bounceBufferSize = ALIGN_UP_BY(DmaFx->Config.MaximumPacketSize, alignUpBy);

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        RtlULongPtrToULong(
            bounceBufferSize, 
            &DmaFx->BounceBufferSize));

    // Allocate bounce buffers
    size_t bounceSize;

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        RtlSizeTMult(
            DmaFx->BounceBufferSize,
            DmaFx->NumBounceBuffers,
            &bounceSize));

    WDFCOMMONBUFFER commonBuffer;
    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        WdfCommonBufferCreate(
            DmaFx->Config.DmaEnabler,
            bounceSize,
            WDF_NO_OBJECT_ATTRIBUTES,
            &commonBuffer));

    DmaFx->BounceBasePA = WdfCommonBufferGetAlignedLogicalAddress(commonBuffer);
    DmaFx->BounceBaseVA = WdfCommonBufferGetAlignedVirtualAddress(commonBuffer);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
__inline
NTSTATUS
_TxDmaFxInitialize(
    _In_ TxDmaFx *DmaFx,
    _In_ BOOLEAN BypassDma
    )
/*

Description:

    This function will initialze the framework to properly map/unmap buffers
    using DMA. It will decide if direct mapping can be done or not, and
    allocate the appropriate resources.

*/
{
    if (DmaFx->Config.MaximumScatterGatherElements == 0)
        DmaFx->Config.MaximumScatterGatherElements = _TX_DMA_FX_DEFAULT_SCATTER_GATHER_ELEMENTS;

    if (DmaFx->Config.AddressWidth > 0)
        DmaFx->MaximumAddress = 1ull << DmaFx->Config.AddressWidth;

    C_ASSERT(_TX_DMA_FX_IS_POWER_OF_TWO(_TX_DMA_FX_NUM_BOUNCE_BUFFERS));

    DmaFx->NumBounceBuffers = _TX_DMA_FX_NUM_BOUNCE_BUFFERS;
    DmaFx->DmaAdapter = WdfDmaEnablerWdmGetDmaAdapter(
        DmaFx->Config.DmaEnabler,
        WdfDmaDirectionWriteToDevice);
    DmaFx->DeviceObject = WdfDeviceWdmGetDeviceObject(DmaFx->Config.Device);

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        _TxDmaFxInitializeBounceBuffers(DmaFx));

    if (BypassDma)
    {
        // If ADAPTER_INFO_API_BYPASS flag is set, it means we can map
        // the buffers without using the DMA HAL APIs, which requires less
        // resource allocation and less complicated calls
        _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
            _TxDmaFxInitializeForDirectMapping(DmaFx));
    }
    else
    {
        _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
            _TxDmaFxInitializeForDma(DmaFx));
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
ULONG
_TxDmaFxCopyPacketToBuffer(
    _In_ NET_PACKET *packet,
    _Out_writes_bytes_(bufferSize) VOID *buffer,
    _In_ ULONG bufferSize
    )
/*

Description:

    Copy the contents of a NET_PACKET to a continuous buffer.

*/
{
    UCHAR *p = (UCHAR*)buffer;
    ULONG bytesRemaining = bufferSize;

    NET_PACKET_FRAGMENT *fragment;

    for (fragment = &packet->Data; ; fragment = NET_PACKET_FRAGMENT_GET_NEXT(fragment))
    {
        if (!NT_VERIFY(bytesRemaining >= fragment->ValidLength))
            break;

        RtlCopyMemory(p, (UCHAR*)fragment->VirtualAddress + fragment->Offset, (size_t)fragment->ValidLength);
        p += fragment->ValidLength;
        bytesRemaining -= (ULONG)fragment->ValidLength;

        if (fragment->LastFragmentOfFrame)
            break;
    }

    return (ULONG)(p - (UCHAR*)buffer);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
NTSTATUS
_TxDmaFxTransmitPacketViaDirectMapping(
    _In_ TxDmaFx *DmaFx,
    _In_ NET_PACKET *NetPacket
    )
/*

Description:

    Maps the fragments of a NET_PACKET using direct mapping and then
    calls EvtSgProgramDescriptors to let the NIC driver program the
    buffers to hardware.

*/
{
    SCATTER_GATHER_LIST *sgl = DmaFx->SpareSgl;
    NET_PACKET_FRAGMENT *fragment;

    sgl->NumberOfElements = 0;

    for (fragment = &NetPacket->Data; ; fragment = NET_PACKET_FRAGMENT_GET_NEXT(fragment))
    {
        ULONG_PTR vaStart = (ULONG_PTR)fragment->VirtualAddress + (ULONG)fragment->Offset;
        ULONG_PTR vaEnd = vaStart + (ULONG)fragment->ValidLength;

        if (vaStart == vaEnd)
            continue;

        for (ULONG_PTR va = vaStart; va < vaEnd; va = (ULONG_PTR)(PAGE_ALIGN(va)) + PAGE_SIZE)
        {
            NT_ASSERT(sgl->NumberOfElements < DmaFx->Config.MaximumScatterGatherElements);

            SCATTER_GATHER_ELEMENT *sgElement = &sgl->Elements[sgl->NumberOfElements];

            // Performance can be optimized by coalescing adjacent SGEs

            sgElement->Address = MmGetPhysicalAddress((PVOID)va);

            if (PAGE_ALIGN(va) != PAGE_ALIGN(vaEnd))
                sgElement->Length = PAGE_SIZE - BYTE_OFFSET(va);
            else
                sgElement->Length = (ULONG)(vaEnd - va);

            sgl->NumberOfElements += 1;
        }

        if (fragment->LastFragmentOfFrame)
            break;
    }

    NT_ASSERT(sgl->NumberOfElements > 0);

    TX_DMA_FX_PROGRAM_DESCRIPTORS(
        DmaFx->QueueHandle,
        NetPacket,
        sgl);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
NTSTATUS
_TxDmaFxTransmitPacketViaDma(
    _In_ TxDmaFx *DmaFx,
    _In_ NET_PACKET *NetPacket
    )
/*

Description:

    Maps the fragments of a NET_PACKET using DMA HAL APIs and then
    calls EvtSgProgramDescriptors to let the NIC driver program the
    buffers to hardware.

*/
{
    ULONG totalFrameLength = 0;
    ULONG mdlChainOffset = 0;

    NET_PACKET_FRAGMENT *fragment;

    for (fragment = &NetPacket->Data; ; fragment = NET_PACKET_FRAGMENT_GET_NEXT(fragment))
    {
        if (fragment->Offset > 0)
        {
            if (totalFrameLength == 0)
            {
                mdlChainOffset += (ULONG)fragment->Offset;
            }
        }

        totalFrameLength += (ULONG)fragment->ValidLength;

        if (fragment->LastFragmentOfFrame)
            break;
    }

    DMA_ADAPTER *dmaAdapter = DmaFx->DmaAdapter;
    TX_DMA_FX_PACKET_CONTEXT *fxPacketContext = _TxDmaFxGetPacketContextFromToken(NetPacket, DmaFx->ContextToken);

    _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(
        dmaAdapter->DmaOperations->InitializeDmaTransferContext(
            dmaAdapter,
            fxPacketContext->DmaTransferContext));

    MDL *firstMdl = NetPacket->Data.Mapping.Mdl;

    NTSTATUS buildSGLStatus = dmaAdapter->DmaOperations->BuildScatterGatherListEx(
        dmaAdapter,
        DmaFx->DeviceObject,
        fxPacketContext->DmaTransferContext,
        firstMdl,
        mdlChainOffset,
        totalFrameLength,
        DMA_SYNCHRONOUS_CALLBACK,
        NULL, // ExecutionRoutine
        NULL, // Context
        TRUE, // WriteToDevice
        fxPacketContext->ScatterGatherBuffer,
        DmaFx->ScatterGatherListSize,
        NULL, // DmaCompletionRoutine,
        NULL, // CompletionContext,
        &fxPacketContext->ScatterGatherList);

    if (buildSGLStatus == STATUS_SUCCESS)
    {
        TX_DMA_FX_PROGRAM_DESCRIPTORS(
            DmaFx->QueueHandle,
            NetPacket,
            fxPacketContext->ScatterGatherList);
    }

    dmaAdapter->DmaOperations->FreeAdapterObject(
        dmaAdapter,
        DeallocateObjectKeepRegisters);

    return buildSGLStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
NTSTATUS
_TxDmaFxMapAndTransmitPacket(
    _In_ TxDmaFx *DmaFx,
    _Inout_ NET_PACKET *NetPacket
    )
/*

Description:

    Calls the appropriate transmit function based on if we
    can bypass DMA APIs or not

*/
{
    if (DmaFx->DmaBypass)
    {
        return _TxDmaFxTransmitPacketViaDirectMapping(DmaFx, NetPacket);
    }
    else
    {
        NTSTATUS dmaStatus = _TxDmaFxTransmitPacketViaDma(DmaFx, NetPacket);

        if (dmaStatus != STATUS_SUCCESS)
        {
            switch (dmaStatus)
            {
                case STATUS_INSUFFICIENT_RESOURCES:
                    DmaFx->Statistics.DMA.InsufficientResourcesCount += 1;
                    break;
                default:
                    DmaFx->Statistics.DMA.OtherErrors += 1;
                    break;
            }
        }

        return dmaStatus;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
NTSTATUS
_TxDmaFxBounceAndTransmitPacket(
    _In_ TxDmaFx *DmaFx,
    _In_ NET_PACKET *NetPacket
    )
/*

Description:

    This function tries to bounce the buffers in a NET_PACKET and build the
    necessary SCATTER_GATHER_LIST to map the new buffer. If it succeeds it
    calls EvtSgProgramDescriptors to let the NIC driver do whatever is needed
    to program the buffer to hardware.

*/
{
    union
    {
        SCATTER_GATHER_LIST sgl;
        UCHAR sglStorage[FIELD_OFFSET(SCATTER_GATHER_LIST, Elements) + sizeof(SCATTER_GATHER_ELEMENT)];
    } u;

    // Is there a free bounce buffer?
    if (DmaFx->BounceBusyIndex - DmaFx->BounceFreeIndex == DmaFx->NumBounceBuffers)
    {
        DmaFx->Statistics.Packet.BounceFailure += 1;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ULONG bounce = DmaFx->BounceBusyIndex % DmaFx->NumBounceBuffers;
    
    PVOID buffer = (UCHAR*)DmaFx->BounceBaseVA + bounce * DmaFx->BounceBufferSize;
    ULONG packetLength = _TxDmaFxCopyPacketToBuffer(NetPacket, buffer, DmaFx->BounceBufferSize);

    u.sgl.NumberOfElements = 1;
    u.sgl.Elements[0].Length = packetLength;
    u.sgl.Elements[0].Address.QuadPart = DmaFx->BounceBasePA.QuadPart + bounce * DmaFx->BounceBufferSize;

    TX_DMA_FX_PROGRAM_DESCRIPTORS(
        DmaFx->QueueHandle,
        NetPacket,
        &u.sgl);
    
    _TX_DMA_FX_PACKET_SET_BOUNCED_FLAG(NetPacket);

    DmaFx->Statistics.Packet.BounceSuccess += 1;
    DmaFx->BounceBusyIndex += 1;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
TX_DMA_BOUNCE_ANALYSIS
_TxDmaFxBounceAnalysis(
    _In_ TxDmaFx *DmaFx,
    _In_ NET_PACKET *NetPacket
    )
{
    ULONG alignmentRequirement = DmaFx->Config.AlignmentRequirement;
    ULONG maximumScatterGatherElements = DmaFx->Config.MaximumScatterGatherElements;
    ULONG maximumPacketSize = DmaFx->Config.MaximumPacketSize;
    BOOLEAN checkAddrWidth = DmaFx->Config.AddressWidth != 0;

    ULONG numDescriptorsRequired = 0;
    ULONGLONG totalPacketSize = 0;
    BOOLEAN bounce = FALSE;

    for (NET_PACKET_FRAGMENT *fragment = &NetPacket->Data;
        fragment;
        fragment = NET_PACKET_FRAGMENT_GET_NEXT(fragment))
    {
        // If a fragment other than the first one has an offset, the DMA
        // APIs won't be able to properly map the buffers.
        if (fragment->Offset > 0 && fragment != &NetPacket->Data)
            bounce = TRUE;
        
        MDL *mdl = (MDL*)fragment->Mapping.Mdl;

        // If a fragment other than the last one does not completly fill
        // the memory described by the MDL, the DMA APIs won't be able to
        // properly map the buffers
        if (fragment->Offset + fragment->ValidLength < MmGetMdlByteCount(mdl) && !fragment->LastFragmentOfFrame)
            bounce = TRUE;

        // Calculate how many Scatter/Gather elements we need to transmit
        // this fragment.
        // This is overly pessimistic if the physical pages are contiguous.
        ULONG_PTR va = (ULONG_PTR)fragment->VirtualAddress + (ULONG)fragment->Offset;
        numDescriptorsRequired += (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(va, fragment->ValidLength);

        totalPacketSize += fragment->ValidLength;

        // If the configuration has an alignment requirement, check if the virtual
        // address meets the requirement. We're assuming the alignment requirement
        // is less than 4096. 
        if ((alignmentRequirement != -1) && (va & alignmentRequirement) != 0)
            bounce = TRUE;

        if (checkAddrWidth)
        {
            PHYSICAL_ADDRESS pa = MmGetPhysicalAddress((VOID*)va);

            if ((ULONGLONG)pa.QuadPart > DmaFx->MaximumAddress)
                bounce = TRUE;
        }

        if (fragment->LastFragmentOfFrame)
            break;
    }

    // First check if we can transmit the buffers at all
    if (maximumPacketSize != 0 && totalPacketSize > maximumPacketSize)
        return TxDmaCannotTransmit;

    // Then check if we detected any condition that requires
    // buffer bouncing
    if (bounce)
        return TxDmaTransmitAfterBouncing;

    // Lastly check if we can DMA the number of required descriptors
    if (numDescriptorsRequired > maximumScatterGatherElements)
        return TxDmaTransmitAfterBouncing;

    return TxDmaTransmitInPlace;
}

#endif