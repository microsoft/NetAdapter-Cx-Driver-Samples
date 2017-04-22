/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

typedef struct TAG_802_1Q
{
    union
    {

        struct
        {
            USHORT VLanID1 : 4;
            USHORT CFI : 1;
            USHORT Priority: 3;
            USHORT VLanID2 : 8;
        }

        TagHeader;

        USHORT Value;
    };
}

TAG_8021q;

typedef struct _TXDESC
{

    union {

        struct
        {
            unsigned short  length;
            unsigned short  status;
            TAG_8021q VLAN_TAG;
            unsigned short TAGC;
        }

        TxDescData;

        struct
        {
            unsigned short  length;
            unsigned short  status;
            TAG_8021q VLAN_TAG;
            unsigned short OffloadGsoMssTagc;
        }

        TxDescDataIpv6Rss_All;
    };

    NDIS_PHYSICAL_ADDRESS   BufferAddress;
}

TX_DESC;


typedef struct _RX_DESC
{
    union {

        struct
        {
            unsigned short  length : 14;
            unsigned short  CheckSumStatus : 2;
            unsigned short  status;
            TAG_8021q VLAN_TAG;
            unsigned short TAVA;
        }

        RxDescData;

        struct
        {
            unsigned short  length : 14;
            unsigned short  TcpUdpFailure : 2;
            unsigned short  status;
            TAG_8021q VLAN_TAG;
            unsigned short IpRssTava;
        }

        RxDescDataIpv6Rss;
    };

    NDIS_PHYSICAL_ADDRESS   BufferAddress;
}

RX_DESC;

#define RT_MAX_FRAME_SIZE (NIC_MAX_PACKET_SIZE + VLAN_HEADER_SIZE + FRAME_CRC_SIZE)

// move to another space later
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

#define TPPool_NPQ      0x40            // normal priority queue polling

#define CR9346_EEDO     BIT_0
#define CR9346_EEDI     BIT_1
#define CR9346_EESK     BIT_2
#define CR9346_EECS     BIT_3
#define CR9346_EEPROM_FREE     BIT_5
#define CR9346_EEM0     BIT_6            // select 8139 operating mode
#define CR9346_EEM1     BIT_7           // 00: normal

//----------------------------------------------------------------------------
//       8169 PHY Access Register (0x60-63)
//----------------------------------------------------------------------------
#define PHYAR_Flag      0x80000000

#define RXS_OWN      BIT_15
#define RXS_EOR      BIT_14
#define RXS_FS       BIT_13
#define RXS_LS       BIT_12

#define RXS_OWN_HIGHBYTE     BIT_7
#define RXS_FS_HIGHBYTE      BIT_5
#define RXS_LS_HIGHBYTE      BIT_4


// 8169
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
#define RFD_ACT_COUNT_MASK      0x3FFF

#define DTCCR_Cmd BIT_3
#define DTCCR_Clr BIT_0

// 8169

struct GDUMP_TALLY
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
};

typedef enum _NIC_CHIP_TYPE
{
    RTLUNKNOWN,
    RTL8168D
} NIC_CHIP_TYPE, *PNIC_CHIP_TYPE;

//TASK Offload bits
#define TXS_TCPCS       BIT_0
#define TXS_UDPCS       BIT_1
#define TXS_IPCS        BIT_2

#define TXS_IPV6RSS_UDPCS BIT_15
#define TXS_IPV6RSS_TCPCS BIT_14
#define TXS_IPV6RSS_IPV4CS BIT_13
#define TXS_IPV6RSS_IS_IPV6 BIT_12
#define TXS_IPV6RSS_TAGC BIT_1
#define TXS_IPV6RSS_TCPHDR_OFFSET 2
#define TXS_IPV6RSS_MSS_OFFSET 2

#define RXS_IPV6RSS_IS_IPV4 BIT_14

#define NIC_CONFIG0_REG_OFFSET      0x00
#define NIC_CONFIG1_REG_OFFSET      0x01
#define NIC_CONFIG2_REG_OFFSET      0x02
#define NIC_CONFIG3_REG_OFFSET      0x03
#define NIC_CONFIG4_REG_OFFSET      0x04
#define NIC_CONFIG5_REG_OFFSET      0x05

//----------------------------------------------------------------------------
//       8139 (CONFIG5) CONFIG5 register bits(0xd8)
//----------------------------------------------------------------------------
//#define TCR_REG 0x40
#define CR_RST          0x10
#define CR_RE           0x08
#define CR_TE           0x04
#define CR_STOP_REQ     0x80

#define RCR_AAP         0x00000001      // accept all physical address
#define RCR_APM         0x00000002      // accept physical match
#define RCR_AM          0x00000004      // accept multicast
#define RCR_AB          0x00000008      // accept broadcast
#define RCR_AR          0x00000010      // accept runt packet
#define RCR_AER         0x00000020      // accept error packet
#define RCR_9356SEL     BIT_6

#define CONFIG3_Magic   0x20            // Wake on Magic packet

enum
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
};

#define PHY_REG_BMSR_AUTO_NEG_COMPLETE BIT_5

#define PHY_STATUS_CABLE_PLUG BIT_7
#define PHY_STATUS_TX_FLOW_CTRL BIT_6
#define PHY_STATUS_RX_FLOW_CTRL BIT_5
#define PHY_STATUS_1000MF BIT_4
#define PHY_STATUS_100M BIT_3
#define PHY_STATUS_10M BIT_2
#define PHY_STATUS_LINK_ON BIT_1
#define PHY_STATUS_FULL_DUPLEX BIT_0

#define ANER_LP_AUTO_NEG_ABLE BIT_0

#define ANAR_10_HALF BIT_5
#define ANAR_10_FULL BIT_6
#define ANAR_100_HALF BIT_7
#define ANAR_100_FULL BIT_8

#define ANAR_MAC_PAUSE BIT_10
#define ANAR_ASYM_PAUSE BIT_11

#define GBCR_1000_FULL BIT_9
#define GBCR_1000_HALF BIT_8

#define MAX_NIC_MULTICAST_REG 8

#define TCR_MXDMA_OFFSET    8
#define TCR_IFG0        0x01000000      // Interframe Gap0
#define TCR_IFG1        0x02000000      // Interframe Gap1
#define TCR_IFG2        0x00080000

#define RCR_MXDMA_OFFSET    8
#define RCR_FIFO_OFFSET     13

// 17-23
#define RCR_Ipv6Rss_RX_ENABLE_FETCH_NUM BIT_14
#define RCR_Ipv6Rss_RX_FETCH_NUM_OFFSET 17

#define PHY_LINK_STATUS BIT_1

#define CPCR_RX_VLAN BIT_6
#define CPCR_RX_CHECKSUM BIT_5
#define CPCR_PCI_DAC BIT_4
#define CPCR_MUL_RW BIT_3
#define CPCR_INT_MITI_TIMER_UNIT_1 BIT_1
#define CPCR_INT_MITI_TIMER_UNIT_0 BIT_0
#define CPCR_DISABLE_INT_MITI_PKT_NUM BIT_7  // 8168B only

#define MIN_RX_DESC 18
#define MAX_RX_DESC 1024

#define MIN_RX_RFD (MIN_RX_DESC * 2)
#define MAX_RX_RFD (MAX_RX_DESC * 2)

#define MIN_TCB 32
#define MAX_TCB 128

#define MAX_SEND_QUEUE_HANDLE_CNT 128

// Recv
#define RX_PKT_ONE_ROUND (32)

#define PHY_LINK_STATUS BIT_1

enum
{
    TCR_RCR_MXDMA_16_BYTES = 0,
    TCR_RCR_MXDMA_32_BYTES,
    TCR_RCR_MXDMA_64_BYTES,
    TCR_RCR_MXDMA_128_BYTES,
    TCR_RCR_MXDMA_256_BYTES,
    TCR_RCR_MXDMA_512_BYTES,
    TCR_RCR_MXDMA_1024_BYTES,
    TCR_RCR_MXDMA_UNLIMITED
};

#define ISRIMR_ROK      BIT_0
#define ISRIMR_RER      BIT_1
#define ISRIMR_TOK      BIT_2
#define ISRIMR_TER      BIT_3
#define ISRIMR_RDU      BIT_4
#define ISRIMR_LINK_CHG BIT_5
#define ISRIMR_RX_FOVW  BIT_6
#define ISRIMR_TDU      BIT_7
#define ISRIMR_SW_INT   BIT_8
//      UNUSED          BIT_9
#define ISRIMR_TDMAOK   BIT_10
//      UNUSED          BIT_11
//      UNUSED          BIT_12
//      UNUSED          BIT_13
#define ISRIMR_TIME_INT BIT_14
#define ISRIMR_SERR     BIT_15

enum
{
    CHKSUM_OFFLOAD_DISABLED = 0,
    CHKSUM_OFFLOAD_TX_ENABLED,
    CHKSUM_OFFLOAD_RX_ENABLED,
    CHKSUM_OFFLOAD_TX_RX_ENABLED
};

#define FRAME_CRC_SIZE 4
#define VLAN_HEADER_SIZE 4
#define RSVD_BUF_SIZE 8

#define SPEED_DUPLUX_MODE_AUTO_NEGOTIATION      (0)
#define SPEED_DUPLUX_MODE_10M_HALF_DUPLEX       (1)
#define SPEED_DUPLUX_MODE_10M_FULL_DUPLEX       (2)
#define SPEED_DUPLUX_MODE_100M_HALF_DUPLEX      (3)
#define SPEED_DUPLUX_MODE_100M_FULL_DUPLEX      (4)
#define SPEED_DUPLUX_MODE_1G_FULL_DUPLEX        (6)
