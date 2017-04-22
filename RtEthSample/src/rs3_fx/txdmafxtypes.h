// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef _TXDMAFXTYPES_H_
#define _TXDMAFXTYPES_H_

#define _TX_DMA_FX_RETURN_IF_NTSTATUS_FAILED(status)   { NTSTATUS __statusRet = (status); if (__statusRet < 0) { return __statusRet;} }
#define _TX_DMA_FX_IS_POWER_OF_TWO(_n) (((_n) != 0) && !((_n) & ((_n) - 1)))

#define _TX_DMA_FX_PACKET_SET_BOUNCED_FLAG(_packet)   (_packet)->Data.Scratch = TRUE
#define _TX_DMA_FX_PACKET_CLEAR_BOUNCED_FLAG(_packet) (_packet)->Data.Scratch = FALSE
#define _TX_DMA_FX_IS_PACKET_BOUNCED(_packet)         (_packet)->Data.Scratch

#define _TX_DMA_FX_NUM_BOUNCE_BUFFERS ( 1 << 4 )
#define _TX_DMA_FX_DEFAULT_SCATTER_GATHER_ELEMENTS 16

#ifndef TX_DMA_FX_ALLOC_TAG
#pragma message(": warning: It is a good practice to define TX_DMA_FX_ALLOC_TAG. Defaulting to WdfDriverGlobals->DriverTag.")
#define TX_DMA_FX_ALLOC_TAG WdfDriverGlobals->DriverTag
#endif

typedef enum _TX_DMA_BOUNCE_ANALYSIS
{
    TxDmaTransmitInPlace,
    TxDmaTransmitAfterBouncing,
    TxDmaCannotTransmit,
} TX_DMA_BOUNCE_ANALYSIS;

typedef
_Function_class_(EVT_TX_DMA_QUEUE_BOUNCE_ANALYSIS)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
TX_DMA_BOUNCE_ANALYSIS
EVT_TX_DMA_QUEUE_BOUNCE_ANALYSIS(
    _In_ NETTXQUEUE TxQueue,
    _In_ NET_PACKET *NetPacket
);

typedef EVT_TX_DMA_QUEUE_BOUNCE_ANALYSIS *PFN_SG_TXQUEUE_BOUNCE_ANALYSIS;

typedef
_Function_class_(EVT_TX_DMA_QUEUE_PROGRAM_DESCRIPTORS)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
EVT_TX_DMA_QUEUE_PROGRAM_DESCRIPTORS(
    _In_      NETTXQUEUE TxQueue,
    _In_      NET_PACKET *NetPacket,
    _In_      SCATTER_GATHER_LIST *Sgl
);

typedef EVT_TX_DMA_QUEUE_PROGRAM_DESCRIPTORS *PFN_SG_TXQUEUE_PROGRAM_DESCRIPTORS;

typedef
_Function_class_(EVT_TX_DMA_QUEUE_GET_PACKET_STATUS)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
EVT_TX_DMA_QUEUE_GET_PACKET_STATUS(
    _In_     NETTXQUEUE TxQueue,
    _In_     NET_PACKET *NetPacket
);

typedef EVT_TX_DMA_QUEUE_GET_PACKET_STATUS *PFN_SG_TXQUEUE_GET_PACKET_STATUS;

typedef
_Function_class_(EVT_TX_DMA_QUEUE_FLUSH_TRANSACTION)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
EVT_TX_DMA_QUEUE_FLUSH_TRANSACTION(
    _In_ NETTXQUEUE TxQueue
);

typedef EVT_TX_DMA_QUEUE_FLUSH_TRANSACTION *PFN_SG_TXQUEUE_FLUSH_TRANSACTION;

typedef struct _NET_TX_DMA_QUEUE_CONFIG
{
    //
    // Has information needed by DMA APIs
    //
    WDFDEVICE Device;

    //
    // DMA enabler to use when mapping/unmapping buffers
    //
    WDFDMAENABLER DmaEnabler;

    //
    // In certain conditions the framework might be able
    // to bypass DMA APIs and map buffers directly, set
    // this to TRUE if you want to allow this behavior
    //
    BOOLEAN AllowDmaBypass;

    //
    // 0 for no limit
    //
    ULONG MaximumScatterGatherElements;

    //
    // Maximum packet size, in bytes, the Tx Queue can transmit
    //
    ULONG MaximumPacketSize;

    //
    // FILE_XXX_ALIGNMENT, or -1 to determine automatically
    //
    ULONG AlignmentRequirement;

    //
    // 2 ^ AddressWidth gives the maximum physical address supported by the hardware
    //
    ULONG AddressWidth;

    //
    // Mandatory Callback - Normal EvtSetNotificationEnabled callback
    //
    PFN_TXQUEUE_SET_NOTIFICATION_ENABLED EvtTxQueueSetNotificationEnabled;

    //
    // Mandatory Callback - Normal EvtCancel callback
    //
    PFN_TXQUEUE_CANCEL EvtTxQueueCancel;

} NET_TX_DMA_QUEUE_CONFIG, *PNET_TX_DMA_QUEUE_CONFIG;

VOID
FORCEINLINE
NET_TX_DMA_QUEUE_CONFIG_INIT(
    _Out_ PNET_TX_DMA_QUEUE_CONFIG NetTxQueueConfig,
    _In_  WDFDEVICE Device,
    _In_  WDFDMAENABLER DmaEnabler,
    _In_  ULONG MaximumPacketSize,
    _In_  PFN_TXQUEUE_SET_NOTIFICATION_ENABLED EvtTxQueueSetNotificationEnabled,
    _In_  PFN_TXQUEUE_CANCEL EvtTxQueueCancel
)
{
    RtlZeroMemory(NetTxQueueConfig, sizeof(*NetTxQueueConfig));
    NetTxQueueConfig->Device = Device;
    NetTxQueueConfig->DmaEnabler = DmaEnabler;
    NetTxQueueConfig->EvtTxQueueSetNotificationEnabled = EvtTxQueueSetNotificationEnabled;
    NetTxQueueConfig->EvtTxQueueCancel = EvtTxQueueCancel;
    NetTxQueueConfig->MaximumPacketSize = MaximumPacketSize;

    NetTxQueueConfig->AlignmentRequirement = (ULONG)-1;
}

typedef struct _TxDmaFxStats
{
    struct _Packet
    {
        // How many NET_PACKETs with IgnoreThisPacket flag set
        ULONG Skipped;

        // Number of times we had to bounce a packet and succeeded
        ULONG BounceSuccess;

        // Number of times we had to bounce a packet but failed
        ULONG BounceFailure;

        // Number of times the bounce analysis returned TxDmaCannotTransmit
        ULONG CannotTransmit;

        // Number of times TX_DMA_FX_GET_PACKET_STATUS returned an error
        ULONG CompletedWithError;
    } Packet;

    struct _DMA
    {
        // Number of times an attempt to transmit a packet using DMA APIs
        // returned STATUS_INSUFFICIENT_RESOURCES
        ULONG InsufficientResourcesCount;

        // Counts other errors from DMA APIs
        ULONG OtherErrors;
    } DMA;

} TxDmaFxStats;

typedef struct _TxDmaFx
{
    // Queue handle, created by the framework
    NETTXQUEUE QueueHandle;

    // Copy of the configuration provided by
    // the NIC driver
    NET_TX_DMA_QUEUE_CONFIG Config;

    // Cache of the NET_RING_BUFFER pointer
    NET_RING_BUFFER *RingBuffer;

    // If the NIC driver provided the AddressWidth
    // parameter in the configuration, this stores
    // the maximum physical address supported by
    // the hardware
    ULONGLONG MaximumAddress;

    // Used to retrieve the framework's packet
    // context from a NET_PACKET
    PNET_PACKET_CONTEXT_TOKEN ContextToken;

    // WDM objects
    DEVICE_OBJECT *DeviceObject;
    DMA_ADAPTER *DmaAdapter;

    // If TRUE, we can bypass HAL's DMA APIs
    BOOLEAN DmaBypass;

    // Used when we can do direct mapping
    SCATTER_GATHER_LIST *SpareSgl;

    // Used when we need to use HAL APIs
    VOID *SgListMem;
    ULONG ScatterGatherListSize;

    // Bounce buffer management
    PHYSICAL_ADDRESS BounceBasePA;
    VOID *BounceBaseVA;
    ULONG BounceBufferSize;

    ULONG NumBounceBuffers;
    ULONG BounceFreeIndex;
    ULONG BounceBusyIndex;

    TxDmaFxStats Statistics;

} TxDmaFx;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TxDmaFx, _TxDmaFxGetContext);

// Used only with HAL APIs
typedef struct _TX_DMA_FX_PACKET_CONTEXT
{
    SCATTER_GATHER_LIST *ScatterGatherList;
    VOID *ScatterGatherBuffer;
    VOID *DmaTransferContext;
} TX_DMA_FX_PACKET_CONTEXT;
NET_PACKET_DECLARE_CONTEXT_TYPE_WITH_NAME(TX_DMA_FX_PACKET_CONTEXT, _TxDmaFxGetPacketContext);

EVT_TXQUEUE_ADVANCE _TxDmaFxAdvance;

#endif