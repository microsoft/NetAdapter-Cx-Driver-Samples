/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

Module Name:
   mp_nic.h

Abstract:
   Function prototypes for mp_nic.c, mp_init.c and mp_req.c

Revision History:
   Who         When        What
   --------    --------    ----------------------------------------------
   DChen       11-01-99    created

Notes:

--*/

#ifndef _MP_NIC_H
#define _MP_NIC_H

void
RtUpdateMediaState(
    _In_    MP_ADAPTER *Adapter);

void
RtGetLinkState(
    _In_    MP_ADAPTER *Adapter,
    _Inout_ NET_ADAPTER_LINK_STATE *LinkState
);

//
// MP_INIT.C
//

NDIS_STATUS NICReadAdapterInfo(
    IN PMP_ADAPTER Adapter);

NTSTATUS NICReadRegParameters(
    IN PMP_ADAPTER Adapter);

NTSTATUS 
RtAdapterAllocateTallyBlock(
    _In_ PMP_ADAPTER Adapter);

void
NICInitializeAdapter(
    _In_ MP_ADAPTER *adapter);

void
HwConfigure(
    _In_ MP_ADAPTER *adapter);

void
HwSetupIAAddress(
    _In_ MP_ADAPTER *adapter);

void
HwClearAllCounters(
    _In_ MP_ADAPTER *adapter);

//
// MP_REQ.C
//

NDIS_STATUS NICGetStatsCounters(
    IN PMP_ADAPTER Adapter,
    IN NDIS_OID Oid,
    OUT PULONG64 pCounter);

void NICSetPacketFilter(
    IN PMP_ADAPTER Adapter,
    IN ULONG PacketFilter);

VOID NICSetMulticastReg(
    IN PMP_ADAPTER Adapter,
    IN ULONG MultiCast_03,
    IN ULONG MultiCast_47
);

void NICSetMulticastList(
    IN PMP_ADAPTER Adapter
);

VOID
MPFillOffloadCaps (
    IN PMP_ADAPTER pAdapter,
    IN OUT PNDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES pOffload_Capabilities,
    IN OUT PNDIS_STATUS pStatus,
    IN OUT PULONG pulInfoLen
);

VOID RtAdapterSetOffloadParameters(
    IN PMP_ADAPTER pAdapter,
    IN PNDIS_OFFLOAD_PARAMETERS pOffloadParaSet,
    OUT PNDIS_OFFLOAD pNdisOffload
);

#endif  // MP_NIC_H

