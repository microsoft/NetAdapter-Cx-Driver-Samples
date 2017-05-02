/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

void
RtAdapterPowerUpPhy(_In_ RT_ADAPTER *adapter);

void
RtAdapterPushPhySettings(_In_ RT_ADAPTER *adapter);

void
RtAdapterResetPhy(_In_ RT_ADAPTER *adapter);

void
RtAdapterInitializeAutoNegotiation(_In_ RT_ADAPTER *adapter);

void
RtAdapterNotifyLinkChange(_In_ RT_ADAPTER *adapter);

_Requires_lock_held_(adapter->Lock)
void
RtAdapterQueryLinkState(
    _In_    RT_ADAPTER *adapter,
    _Inout_ NET_ADAPTER_LINK_STATE *linkState);

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