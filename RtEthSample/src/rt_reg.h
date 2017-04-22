/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#ifndef _RT_REG_H
#define _RT_REG_H

#pragma pack(1)

//-------------------------------------------------------------------------
// Ethernet Frame Structure
//-------------------------------------------------------------------------
//- Ethernet 6-byte Address

typedef struct _ETH_ADDRESS_STRUC
{
    UCHAR EthNodeAddress[ETHERNET_ADDRESS_LENGTH];
}

ETH_ADDRESS_STRUC, *PETH_ADDRESS_STRUC;


//- Ethernet 14-byte Header

typedef struct _ETH_HEADER_STRUC
{
    UCHAR Destination[ETHERNET_ADDRESS_LENGTH];
    UCHAR Source[ETHERNET_ADDRESS_LENGTH];
    USHORT TypeLength;
}

ETH_HEADER_STRUC, *PETH_HEADER_STRUC;

typedef struct _ETH_RX_BUFFER_STRUC
{
    ETH_HEADER_STRUC RxMacHeader;
    UCHAR RxBufferData[(RCB_BUFFER_SIZE - sizeof(ETH_HEADER_STRUC))];
}

ETH_RX_BUFFER_STRUC, *PETH_RX_BUFFER_STRUC;


//-------------------------------------------------------------------------
// Control/Status Registers (CSR)
//-------------------------------------------------------------------------

typedef struct _CSR_STRUC
{
    UCHAR ID0;
    UCHAR ID1;
    UCHAR ID2;
    UCHAR ID3;
    UCHAR ID4;
    UCHAR ID5;
    USHORT RESV_06_07;
    UCHAR MulticastReg0;
    UCHAR MulticastReg1;
    UCHAR MulticastReg2;
    UCHAR MulticastReg3;
    UCHAR MulticastReg4;
    UCHAR MulticastReg5;
    UCHAR MulticastReg6;
    UCHAR MulticastReg7;
    // 0x10
    ULONG DTCCRLow;
    ULONG DTCCRHigh;

    //0x18~0x1B
    ULONG REG_18_1B;

    // 0x1C
    ULONG REG_1C_1F;

    // 0x20
    ULONG TNPDSLow;
    ULONG TNPDSHigh;

    ULONG THPDSLow;
    ULONG THPDSHigh;

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
    ULONG  TCR;
    ULONG  RCR;
    ULONG  TimerCTR;
    ULONG REG_4C_4F;
    // 0x50
    UCHAR CR9346;
    // 0x51
    UCHAR CONFIG0;
    // 0x52
    UCHAR CONFIG1;
    // 0x53
    UCHAR CONFIG2;
    // 0x54
    UCHAR CONFIG3;
    UCHAR CONFIG4;
    UCHAR CONFIG5;

    // Transmit Descriptor Fetch Number - controls
    // the transmit descriptor fetch number
    UCHAR TDFNR;
    ULONG Timer;
    ULONG REG_5C_5F;
    ULONG PhyAccessReg;
    ULONG REG_64_67;
    ULONG REG_68_6B;
    UCHAR PhyStatus;
    UCHAR REG_6D;
    UCHAR REG_6E;
    UCHAR REG_6F;

    // 0x70

    ULONG REG_70_73;
    ULONG REG_74_77;
    ULONG REG_78_7B;
    ULONG REG_7C_7F;

    ULONG REG_80_83;
    ULONG REG_84_87;
    ULONG REG_88_8B;
    ULONG REG_8C_8F;

    ULONG REG_90_93;
    ULONG REG_94_97;
    ULONG REG_98_9B;
    ULONG REG_9C_9F;

    ULONG REG_A0_A3;
    ULONG REG_A4_A7;
    ULONG REG_A8_AB;
    ULONG REG_AC_AF;

    ULONG REG_B0_B3;
    ULONG REG_B4_B7;
    ULONG REG_B8_BB;
    ULONG REG_BC_BF;

    ULONG REG_C0_C3;
    ULONG REG_C4_C7;
    ULONG REG_C8_CB;
    ULONG REG_CC_CF;
    // D0

    union
    {
        struct
        {
            UCHAR   RESV_D0;
            UCHAR   RESV_D1;
            UCHAR   RESV_D2;
            UCHAR   RESV_D3;
        }

        REG_D0_D3;

    };
    ULONG REG_D4_D7;
    USHORT RESV_D8_D9;
    USHORT RMS;
    ULONG REG_DC_DF;


    USHORT  CPCR;

    union
    {
        struct
        {
            UCHAR RxTimerNum;
            UCHAR TxTimerNum;
        }
        IntMiti;
    };

    ULONG   RDSARLow;
    ULONG   RDSARHigh;


    // EC


    union
    {

        struct
        {
            UCHAR   MTPS;
            UCHAR   RESV_ED;
            UCHAR   RESV_EE;
            UCHAR   RESV_EF;
        }
        MtpsReg;

    };

    union
    {

        struct
        {
            UCHAR RESV_F0;
            UCHAR RESV_F1;
            UCHAR RESV_F2;
            UCHAR RESV_F3;
        }

        REG_F0_F3;

    };
    ULONG REG_F4_F7;
    ULONG REG_F8_FB;
    ULONG REG_FC_FF;

}

CSR_STRUC, *PCSR_STRUC;

#pragma pack()

// MDI Control register bit definitions
#define MDI_CR_1000                 BIT_6       // 
#define MDI_CR_COLL_TEST_ENABLE     BIT_7       // Collision test enable
#define MDI_CR_FULL_HALF            BIT_8       // FDX =1, half duplex =0
#define MDI_CR_RESTART_AUTO_NEG     BIT_9       // Restart auto negotiation
#define MDI_CR_ISOLATE              BIT_10      // Isolate PHY from MII
#define MDI_CR_POWER_DOWN           BIT_11      // Power down
#define MDI_CR_AUTO_SELECT          BIT_12      // Auto speed select enable
#define MDI_CR_10_100               BIT_13      // 0 = 10Mbs, 1 = 100Mbs
#define MDI_CR_LOOPBACK             BIT_14      // 0 = normal, 1 = loopback
#define MDI_CR_RESET                BIT_15      // 0 = normal, 1 = PHY reset

// MDI Status register bit definitions
#define MDI_SR_EXT_REG_CAPABLE      BIT_0       // Extended register capabilities
#define MDI_SR_JABBER_DETECT        BIT_1       // Jabber detected
#define MDI_SR_LINK_STATUS          BIT_2       // Link Status -- 1 = link
#define MDI_SR_AUTO_SELECT_CAPABLE  BIT_3       // Auto speed select capable
#define MDI_SR_REMOTE_FAULT_DETECT  BIT_4       // Remote fault detect
#define MDI_SR_AUTO_NEG_COMPLETE    BIT_5       // Auto negotiation complete
#define MDI_SR_10T_HALF_DPX         BIT_11      // 10BaseT Half Duplex capable
#define MDI_SR_10T_FULL_DPX         BIT_12      // 10BaseT full duplex capable
#define MDI_SR_TX_HALF_DPX          BIT_13      // TX Half Duplex capable
#define MDI_SR_TX_FULL_DPX          BIT_14      // TX full duplex capable
#define MDI_SR_T4_CAPABLE           BIT_15      // T4 capable

#endif  // _RT_REG_H

