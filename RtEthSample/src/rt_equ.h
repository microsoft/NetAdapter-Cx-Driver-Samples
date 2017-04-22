/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#ifndef _RT_EQU_H
#define _RT_EQU_H

//-------------------------------------------------------------------------
// Phy related constants
//-------------------------------------------------------------------------
#define PHY_RESET_TIME        (1250 * 4) // (1250 * 4 *100 = 500000) phy reset should complete within 500ms (from spec)


//-------------------------------------------------------------------------
// Ethernet Frame Sizes
//-------------------------------------------------------------------------
#define ETHERNET_ADDRESS_LENGTH         6
#define ETHERNET_HEADER_SIZE            14
#define MINIMUM_ETHERNET_PACKET_SIZE    60
#define MAXIMUM_ETHERNET_PACKET_SIZE    1514

#define MAX_MULTICAST_ADDRESSES         32

// Make receive area 1520 for 16 bit alignment.
#define RCB_BUFFER_SIZE                 (1526) // 0x5F6 //1514+4(CRC)+4(VLAN)+4(RcvSideScaleResult)

//-------------------------------------------------------------------------
// Bit Mask definitions
//-------------------------------------------------------------------------
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

#define BIT_0_2     0x0007
#define BIT_0_3     0x000F
#define BIT_0_4     0x001F
#define BIT_0_5     0x003F
#define BIT_0_6     0x007F
#define BIT_0_7     0x00FF
#define BIT_0_8     0x01FF
#define BIT_0_13    0x3FFF
#define BIT_0_15    0xFFFF
#define BIT_1_2     0x0006
#define BIT_1_3     0x000E
#define BIT_2_5     0x003C
#define BIT_3_4     0x0018
#define BIT_4_5     0x0030
#define BIT_4_6     0x0070
#define BIT_4_7     0x00F0
#define BIT_5_7     0x00E0
#define BIT_5_9     0x03E0
#define BIT_5_12    0x1FE0
#define BIT_5_15    0xFFE0
#define BIT_6_7     0x00c0
#define BIT_7_11    0x0F80
#define BIT_8_10    0x0700
#define BIT_9_13    0x3E00
#define BIT_12_15   0xF000
#define BIT_8_15    0xFF00

#define BIT_16_20   0x001F0000
#define BIT_21_25   0x03E00000
#define BIT_26_27   0x0C000000

#define CMD_BUS_MASTER              BIT_2

#endif  // _RT_EQU_H

