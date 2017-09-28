/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

    ===========================================================================

    This file contains definitions for structures, registers, and constants for
    Realtek Gigabit Ethernet network cards. This is all specific to Realtek 
    hardware.

--*/

#pragma once

#pragma region Bit Definitions

#define BIT_0       0x0001
#define BIT_1       0x0002
#define BIT_2       0x0004
#define BIT_3       0x0008
#define BIT_4       0x0010
#define BIT_5       0x0020
#define BIT_6       0x0040
#define BIT_7       0x0080
#define BIT_8       0x0100
#define BIT_9       0x0200
#define BIT_10      0x0400
#define BIT_11      0x0800
#define BIT_12      0x1000
#define BIT_13      0x2000
#define BIT_14      0x4000
#define BIT_15      0x8000
#define BIT_16      0x010000
#define BIT_17      0x020000
#define BIT_18      0x040000
#define BIT_19      0x080000
#define BIT_20      0x100000
#define BIT_21      0x200000
#define BIT_22      0x400000
#define BIT_23      0x800000
#define BIT_24      0x01000000
#define BIT_25      0x02000000
#define BIT_26      0x04000000
#define BIT_27      0x08000000
#define BIT_28      0x10000000
#define BIT_29      0x20000000
#define BIT_30      0x40000000
#define BIT_31      0x80000000

#pragma endregion

#pragma region Standards Limits

#define FRAME_CRC_SIZE 4
#define VLAN_HEADER_SIZE 4
#define RSVD_BUF_SIZE 8

// Ethernet Frame Sizes
#define ETHERNET_ADDRESS_LENGTH         6

#pragma endregion

#pragma region Hardware Limits

// packet and header sizes
#define RT_MAX_PACKET_SIZE (1514)
#define RT_MAX_FRAME_SIZE  (RT_MAX_PACKET_SIZE + VLAN_HEADER_SIZE + FRAME_CRC_SIZE)

// maximum link speed for send and recv in bps
#define RT_MEDIA_MAX_SPEED 1'000'000'000

// Phy related constants
#define PHY_RESET_TIME        (1250 * 4) // (1250 * 4 *100 = 500000) phy reset should complete within 500ms (from spec)

#pragma endregion

#pragma region Software Limits

// max number of physical fragments supported per TCB
#define RT_MAX_PHYS_BUF_COUNT 16

// multicast list size
#define RT_MAX_MCAST_LIST 32

#define RT_MIN_TCB 32
#define RT_MAX_TCB 128

#pragma endregion

#pragma region Version

// Driver version numbers
// update the driver version number every time you release a new driver
// The high word is the major version. The low word is the minor version.
// this should be the same as the version reported in miniport driver characteristics
#define RT_MAJOR_DRIVER_VERSION        1
#define RT_MINOR_DRIVER_VERISON        0
#define RT_VENDOR_DRIVER_VERSION       ((RT_MAJOR_DRIVER_VERSION << 16) | RT_MINOR_DRIVER_VERISON)

#pragma endregion

#pragma region Error Codes

// Error log definitions
#define ERRLOG_OUT_OF_SG_RESOURCES      0x00000409L
#define ERRLOG_NO_MEMORY_RESOURCE       0x00000605L

#pragma endregion

#pragma region Capabilities

// supported filters
#define RT_SUPPORTED_FILTERS (          \
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

#pragma endregion

#pragma region Hardware Memory Descriptors

typedef struct _RT_TAG_802_1Q
{
    union
    {
        struct
        {
            USHORT VLanID1 : 4;
            USHORT CFI : 1;
            USHORT Priority: 3;
            USHORT VLanID2 : 8;
        } TagHeader;

        USHORT Value;
    };
} RT_TAG_802_1Q;

typedef struct _RT_TX_DESC
{
    struct
    {
        unsigned short  length;
        unsigned short  status;
        RT_TAG_802_1Q VLAN_TAG;
        unsigned short OffloadGsoMssTagc;
    } TxDescDataIpv6Rss_All;

    PHYSICAL_ADDRESS BufferAddress;

} RT_TX_DESC;


typedef struct _RT_RX_DESC
{
    struct
    {
        unsigned short  length : 14;
        unsigned short  TcpUdpFailure : 2;
        unsigned short  status;
        RT_TAG_802_1Q VLAN_TAG;
        unsigned short IpRssTava;
    } RxDescDataIpv6Rss;

    PHYSICAL_ADDRESS BufferAddress;
} RT_RX_DESC;

typedef struct _RT_TALLY
{
    ULONGLONG TxOK;
    ULONGLONG RxOK;
    ULONGLONG TxERR;
    ULONG RxERR;
    USHORT MissPkt;
    USHORT FAE;
    ULONG Tx1Col;
    ULONG TxMCol;
    ULONGLONG RxOKPhy;
    ULONGLONG RxOKBrd;
    ULONG RxOKMul;
    USHORT TxAbt;
    USHORT TxUndrn; //only possible on jumbo frames
} RT_TALLY;

#pragma endregion

#pragma region Control Block

#pragma pack(1)

//-------------------------------------------------------------------------
// Control/Status Registers (CSR)
//-------------------------------------------------------------------------

typedef struct _RT_MAC
{
    // 00
    UCHAR ID[6];
    USHORT RESV_06_07;
    UCHAR MulticastReg7;
    UCHAR MulticastReg6;
    UCHAR MulticastReg5;
    UCHAR MulticastReg4;
    UCHAR MulticastReg3;
    UCHAR MulticastReg2;
    UCHAR MulticastReg1;
    UCHAR MulticastReg0;
    
    // 10
    ULONG DTCCRLow;
    ULONG DTCCRHigh;

    ULONG REG_18_1B;
    ULONG REG_1C_1F;

    // 20
    ULONG TNPDSLow;
    ULONG TNPDSHigh;

    ULONG THPDSLow;
    ULONG THPDSHigh;

    // 30
    USHORT ResendIntrTmr;
    USHORT ResendIntrTmrCnt;

    USHORT RTRxDMABC;
    UCHAR ERSR;
    UCHAR CmdReg;
    UCHAR TPPoll;
    UCHAR RESV_39;
    UCHAR RESV_3A;
    UCHAR RESV_3B;
    USHORT IMR;
    USHORT  ISR;

    // 40
    ULONG  TCR;
    ULONG  RCR;
    ULONG  TimerCTR;
    ULONG REG_4C_4F;
    
    // 50
    UCHAR CR9346;
    UCHAR CONFIG0;
    UCHAR CONFIG1;
    UCHAR CONFIG2;
    UCHAR CONFIG3;
    UCHAR CONFIG4;
    UCHAR CONFIG5;

    // Transmit Descriptor Fetch Number - controls
    // the transmit descriptor fetch number
    UCHAR TDFNR;
    ULONG Timer;
    ULONG REG_5C_5F;

    // 60
    ULONG PhyAccessReg;
    ULONG REG_64_67;
    ULONG REG_68_6B;
    UCHAR PhyStatus;
    UCHAR REG_6D;
    UCHAR REG_6E;
    UCHAR REG_6F;

    // 70
    ULONG REG_70_73;
    ULONG REG_74_77;
    ULONG REG_78_7B;
    ULONG REG_7C_7F;

    // 80
    ULONG REG_80_83;
    ULONG REG_84_87;
    ULONG REG_88_8B;
    ULONG REG_8C_8F;

    // 90
    ULONG REG_90_93;
    ULONG REG_94_97;
    ULONG REG_98_9B;
    ULONG REG_9C_9F;

    // A0
    ULONG REG_A0_A3;
    ULONG REG_A4_A7;
    ULONG REG_A8_AB;
    ULONG REG_AC_AF;

    // B0
    ULONG REG_B0_B3;
    ULONG REG_B4_B7;
    ULONG REG_B8_BB;
    ULONG REG_BC_BF;

    // C0
    ULONG REG_C0_C3;
    ULONG REG_C4_C7;
    ULONG REG_C8_CB;
    ULONG REG_CC_CF;

    // D0
    struct
    {
        UCHAR   RESV_D0;
        UCHAR   RESV_D1;
        UCHAR   RESV_D2;
        UCHAR   RESV_D3;
    } REG_D0_D3;

    ULONG REG_D4_D7;
    USHORT RESV_D8_D9;
    USHORT RMS;
    ULONG REG_DC_DF;

    // E0
    USHORT  CPCR;

    struct
    {
        UCHAR RxTimerNum;
        UCHAR TxTimerNum;
    } IntMiti;

    ULONG   RDSARLow;
    ULONG   RDSARHigh;

    struct
    {
        UCHAR   MTPS;
        UCHAR   RESV_ED;
        UCHAR   RESV_EE;
        UCHAR   RESV_EF;
    } MtpsReg;

    // F0
    struct
    {
        UCHAR RESV_F0;
        UCHAR RESV_F1;
        UCHAR RESV_F2;
        UCHAR RESV_F3;
    } REG_F0_F3;

    ULONG REG_F4_F7;
    ULONG REG_F8_FB;
    ULONG REG_FC_FF;
} RT_MAC;

static_assert(sizeof(RT_MAC) == 0x100, "Size of RT_MAC is specified by hardware");

#pragma pack()

#pragma endregion

#pragma region Register Constants

// MulticastReg7: 0x8
#define MAX_NIC_MULTICAST_REG 8

// DTCCR: 0x10
#define DTCCR_Cmd BIT_3
#define DTCCR_Clr BIT_0

// CmdReg: 0x37
#define CR_RST          0x10
#define CR_RE           0x08
#define CR_TE           0x04
#define CR_STOP_REQ     0x80

// TPPoll: 0x38
#define TPPoll_NPQ 0x40 // normal priority queue polling

// IMR: 0x3C, ISR: 0x3E
#define ISRIMR_ROK      BIT_0
#define ISRIMR_RER      BIT_1
#define ISRIMR_TOK      BIT_2
#define ISRIMR_TER      BIT_3
#define ISRIMR_RDU      BIT_4
#define ISRIMR_LINK_CHG BIT_5
#define ISRIMR_RX_FOVW  BIT_6
#define ISRIMR_TDU      BIT_7
#define ISRIMR_SW_INT   BIT_8
#define ISRIMR_TDMAOK   BIT_10
#define ISRIMR_TIME_INT BIT_14
#define ISRIMR_SERR     BIT_15

// TCR: 0x40
#define TCR_MXDMA_OFFSET 8
#define TCR_IFG2         BIT_19
#define TCR_IFG0         BIT_24      // Interframe Gap0
#define TCR_IFG1         BIT_25      // Interframe Gap1

// RCR: 0x44
#define RCR_AAP     BIT_0 // accept all physical address
#define RCR_APM     BIT_1 // accept physical match
#define RCR_AM      BIT_2 // accept multicast
#define RCR_AB      BIT_3 // accept broadcast
#define RCR_AR      BIT_4 // accept runt packet
#define RCR_AER     BIT_5 // accept error packet
#define RCR_9356SEL BIT_6 // EEPROM type

#define RCR_MXDMA_OFFSET    8
#define RCR_FIFO_OFFSET     13

#define RCR_Ipv6Rss_RX_ENABLE_FETCH_NUM BIT_14
#define RCR_Ipv6Rss_RX_FETCH_NUM_OFFSET 17

typedef enum _RT_TCR_RCR_MXDMA : UCHAR
{
    TCR_RCR_MXDMA_16_BYTES = 0,
    TCR_RCR_MXDMA_32_BYTES,
    TCR_RCR_MXDMA_64_BYTES,
    TCR_RCR_MXDMA_128_BYTES,
    TCR_RCR_MXDMA_256_BYTES,
    TCR_RCR_MXDMA_512_BYTES,
    TCR_RCR_MXDMA_1024_BYTES,
    TCR_RCR_MXDMA_UNLIMITED
} RT_TCR_RCR_MXDMA;

// CR9346: 0x50
#define CR9346_EEDO        BIT_0
#define CR9346_EEDI        BIT_1
#define CR9346_EESK        BIT_2
#define CR9346_EECS        BIT_3
#define CR9346_EEPROM_FREE BIT_5
#define CR9346_EEM0        BIT_6 // select 8139 operating mode
#define CR9346_EEM1        BIT_7 // 00: normal

// CONFIG3: 0x54
#define CONFIG3_Magic   0x20            // Wake on Magic packet

// PhyAccessReg: 0x60
#define PHYAR_Flag      BIT_31

#define PHY_REG_BMSR_AUTO_NEG_COMPLETE BIT_5

typedef enum _RT_PHY_REG
{
    PHY_REG_BMCR = 0,
    PHY_REG_BMSR,
    PHY_REG_PHYAD1,
    PHY_REG_PHYAD2,
    PHY_REG_ANAR,
    PHY_REG_ANLPAR,
    PHY_REG_ANER,
    PHY_REG_ANNPTR,
    PHY_REG_ANNRPR,
    PHY_REG_GBCR,
    PHY_REG_GBSR,
    PHY_REG_RESV_11,
    PHY_REG_RESV_12,
    PHY_REG_RESV_13,
    PHY_REG_RESV_14,
    PHY_REG_GBESR
} RT_PHY_REG;

#define ANER_LP_AUTO_NEG_ABLE BIT_0

#define ANAR_10_HALF BIT_5
#define ANAR_10_FULL BIT_6
#define ANAR_100_HALF BIT_7
#define ANAR_100_FULL BIT_8

#define ANAR_MAC_PAUSE BIT_10
#define ANAR_ASYM_PAUSE BIT_11

#define GBCR_1000_FULL BIT_9
#define GBCR_1000_HALF BIT_8

// PhyStatus: 0x6C
#define PHY_STATUS_CABLE_PLUG BIT_7
#define PHY_STATUS_TX_FLOW_CTRL BIT_6
#define PHY_STATUS_RX_FLOW_CTRL BIT_5
#define PHY_STATUS_1000MF BIT_4
#define PHY_STATUS_100M BIT_3
#define PHY_STATUS_10M BIT_2
#define PHY_STATUS_LINK_ON BIT_1
#define PHY_STATUS_FULL_DUPLEX BIT_0

// PhyStatus: 0x6C
#define PHY_LINK_STATUS BIT_1

// CPCR: 0xE0
#define CPCR_RX_VLAN BIT_6
#define CPCR_RX_CHECKSUM BIT_5
#define CPCR_PCI_DAC BIT_4
#define CPCR_MUL_RW BIT_3
#define CPCR_INT_MITI_TIMER_UNIT_1 BIT_1
#define CPCR_INT_MITI_TIMER_UNIT_0 BIT_0
#define CPCR_DISABLE_INT_MITI_PKT_NUM BIT_7

#pragma endregion

#pragma region Descriptor Constants

// Receive status
#define RXS_OWN      BIT_15
#define RXS_EOR      BIT_14
#define RXS_FS       BIT_13
#define RXS_LS       BIT_12

#define RXS_OWN_HIGHBYTE     BIT_7
#define RXS_FS_HIGHBYTE      BIT_5
#define RXS_LS_HIGHBYTE      BIT_4

#define RXS_MAR      BIT_11
#define RXS_PAM      BIT_10
#define RXS_BAR      BIT_9
#define RXS_BOVF     BIT_8
#define RXS_FOVF     BIT_7
#define RXS_RWT      BIT_6
#define RXS_RES      BIT_5
#define RXS_RUNT     BIT_4
#define RXS_CRC      BIT_3
#define RXS_UDPIP_PACKET     BIT_2
#define RXS_TCPIP_PACKET     BIT_1
#define RXS_IP_PACKET BIT_2|BIT_1
#define RXS_IPF      BIT_0
#define RXS_UDPF     BIT_1
#define RXS_TCPF     BIT_0

// receive extended status
#define RXS_IPV6RSS_IS_IPV6 BIT_15
#define RXS_IPV6RSS_IS_IPV4 BIT_14
#define RXS_IPV6RSS_UDPF BIT_10
#define RXS_IPV6RSS_TCPF BIT_9

// Transmit status
#define TXS_CC3_0       BIT_0|BIT_1|BIT_2|BIT_3
#define TXS_EXC         BIT_4
#define TXS_LNKF        BIT_5
#define TXS_OWC         BIT_6
#define TXS_TES         BIT_7
#define TXS_UNF         BIT_9
#define TXS_LGSEN       BIT_11
#define TXS_LS          BIT_12
#define TXS_FS          BIT_13
#define TXS_EOR         BIT_14
#define TXS_OWN         BIT_15

#define TXS_OWN_HIGHBYTE BIT_7

#define TXS_TCPCS       BIT_0
#define TXS_UDPCS       BIT_1
#define TXS_IPCS        BIT_2

// transmit extended status
#define TXS_IPV6RSS_UDPCS BIT_15
#define TXS_IPV6RSS_TCPCS BIT_14
#define TXS_IPV6RSS_IPV4CS BIT_13
#define TXS_IPV6RSS_IS_IPV6 BIT_12
#define TXS_IPV6RSS_TAGC BIT_1
#define TXS_IPV6RSS_TCPHDR_OFFSET 2
#define TXS_IPV6RSS_MSS_OFFSET 2

#pragma endregion
