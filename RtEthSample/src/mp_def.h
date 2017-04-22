/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

--*/


#ifndef _MP_DEF_H
#define _MP_DEF_H

// memory tag for this driver
#define NIC_TAG                         ((ULONG)'RTEK')
#define NIC_DBG_STRING                  ("**RTL**")

// packet and header sizes
#define NIC_MAX_PACKET_SIZE             (1514)
#define NIC_MIN_PACKET_SIZE             (60)
#define NIC_HEADER_SIZE                 14

// multicast list size
#define NIC_MAX_MCAST_LIST              32

#define NIC_MAJOR_DRIVER_VERSION        9
#define NIC_MINOR_DRIVER_VERISON        1

#define NIC_DRIVER_VERISON_GLOBAL_EXPORT ((NIC_MAJOR_DRIVER_VERSION * 1000) | NIC_MINOR_DRIVER_VERISON)

// update the driver version number every time you release a new driver
// The high word is the major version. The low word is the minor version.
// this should be the same as the version reported in miniport driver characteristics

#define NIC_VENDOR_DRIVER_VERSION       ((NIC_MAJOR_DRIVER_VERSION << 16) | NIC_MINOR_DRIVER_VERISON)

// NDIS version in use by the NIC driver.
// The high byte is the major version. The low byte is the minor version.
#define NIC_DRIVER_VERSION              0x0600


// media type, we use ethernet, change if necessary
#define NIC_MEDIA_TYPE                  NdisMedium802_3


//
// maximum link speed for send dna recv in bps
//
#define NIC_MEDIA_MAX_SPEED             1000000000

// interface type, we use PCI
#define NIC_INTERFACE_TYPE              NdisInterfacePci

// buffer size passed in NdisMQueryAdapterResources
// We should only need three adapter resources (IO, interrupt and memory),
// Some devices get extra resources, so have room for 10 resources
#define NIC_RESOURCE_BUF_SIZE           (sizeof(NDIS_RESOURCE_LIST) + \
                                        (10*sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)))

// IO space length
#define NIC_MAP_IOSPACE_LENGTH          sizeof(CSR_STRUC)

// hardware CSR (Control Status Register) structure

typedef PCSR_STRUC PHW_CSR;

// change to your company name instead of using Microsoft
#define NIC_VENDOR_DESC                 "Realtek"


// max number of physical fragments supported per TCB
#define NIC_MAX_PHYS_BUF_COUNT          (16)

// How many intervals before the RFD list is shrinked?
#define NIC_RFD_SHRINK_THRESHOLD        10

// local data buffer size (to copy send packet data into a local buffer)
#define NIC_BUFFER_SIZE                 1520

// max lookahead size
#define NIC_MAX_LOOKAHEAD               (NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE)

// max number of send packets the MiniportSendPackets function can accept
#define NIC_MAX_SEND_PACKETS            (10)

// supported filters
#define NIC_SUPPORTED_FILTERS (          \
    NET_PACKET_FILTER_TYPE_DIRECTED    | \
    NET_PACKET_FILTER_TYPE_MULTICAST   | \
    NET_PACKET_FILTER_TYPE_BROADCAST   | \
    NET_PACKET_FILTER_TYPE_PROMISCUOUS | \
    NET_PACKET_FILTER_TYPE_ALL_MULTICAST)

#define NIC_SUPPORTED_STATISTICS (                 \
    NET_ADAPTER_STATISTICS_XMIT_OK               | \
    NET_ADAPTER_STATISTICS_RCV_OK                | \
    NET_ADAPTER_STATISTICS_XMIT_ERROR            | \
    NET_ADAPTER_STATISTICS_RCV_ERROR             | \
    NET_ADAPTER_STATISTICS_RCV_CRC_ERROR         | \
    NET_ADAPTER_STATISTICS_RCV_NO_BUFFER         | \
    NET_ADAPTER_STATISTICS_TRANSMIT_QUEUE_LENGTH | \
    NET_ADAPTER_STATISTICS_GEN_STATISTICS)

// Threshold for a remove
#define NIC_HARDWARE_ERROR_THRESHOLD    1

// The CheckForHang intervals before we decide the send is stuck
#define NIC_SEND_HANG_THRESHOLD         3

// NDIS_ERROR_CODE_ADAPTER_NOT_FOUND
#define ERRLOG_READ_PCI_SLOT_FAILED     0x00000101L
#define ERRLOG_WRITE_PCI_SLOT_FAILED    0x00000102L
#define ERRLOG_VENDOR_DEVICE_NOMATCH    0x00000103L

// NDIS_ERROR_CODE_ADAPTER_DISABLED
#define ERRLOG_BUS_MASTER_DISABLED      0x00000201L

// NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION
#define ERRLOG_INVALID_SPEED_DUPLEX     0x00000301L
#define ERRLOG_SET_SECONDARY_FAILED     0x00000302L

// NDIS_ERROR_CODE_OUT_OF_RESOURCES
#define ERRLOG_OUT_OF_MEMORY            0x00000401L
#define ERRLOG_OUT_OF_SHARED_MEMORY     0x00000402L
#define ERRLOG_OUT_OF_BUFFER_POOL       0x00000404L
#define ERRLOG_OUT_OF_NDIS_BUFFER       0x00000405L
#define ERRLOG_OUT_OF_PACKET_POOL       0x00000406L
#define ERRLOG_OUT_OF_NDIS_PACKET       0x00000407L
#define ERRLOG_OUT_OF_LOOKASIDE_MEMORY  0x00000408L
#define ERRLOG_OUT_OF_SG_RESOURCES      0x00000409L

// NDIS_ERROR_CODE_HARDWARE_FAILURE
#define ERRLOG_SELFTEST_FAILED          0x00000501L
#define ERRLOG_INITIALIZE_ADAPTER       0x00000502L
#define ERRLOG_REMOVE_MINIPORT          0x00000503L

// NDIS_ERROR_CODE_RESOURCE_CONFLICT
#define ERRLOG_MAP_IO_SPACE             0x00000601L
#define ERRLOG_QUERY_ADAPTER_RESOURCES  0x00000602L
#define ERRLOG_NO_IO_RESOURCE           0x00000603L
#define ERRLOG_NO_INTERRUPT_RESOURCE    0x00000604L
#define ERRLOG_NO_MEMORY_RESOURCE       0x00000605L


#endif  // _MP_DEF_H

