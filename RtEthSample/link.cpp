/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"

#include "trace.h"
#include "link.h"
#include "phy.h"
#include "adapter.h"

NET_IF_MEDIA_DUPLEX_STATE
RtAdapterGetDuplexSetting(_In_ RT_ADAPTER const *adapter)
{
    switch (adapter->SpeedDuplex)
    {
    case RtSpeedDuplexMode10MHalfDuplex:
    case RtSpeedDuplexMode100MHalfDuplex:
        return MediaDuplexStateHalf;

    case RtSpeedDuplexMode10MFullDuplex:
    case RtSpeedDuplexMode100MFullDuplex:
    case RtSpeedDuplexMode1GFullDuplex:
        return MediaDuplexStateFull;

    default:
        return MediaDuplexStateUnknown;
    }
}

USHORT
RtAdapterGetLinkSpeedSetting(_In_ RT_ADAPTER const *adapter)
{
    switch (adapter->SpeedDuplex)
    {
    case RtSpeedDuplexMode10MHalfDuplex:
    case RtSpeedDuplexMode10MFullDuplex:
        return 10;

    case RtSpeedDuplexMode100MHalfDuplex:
    case RtSpeedDuplexMode100MFullDuplex:
        return 100;

    case RtSpeedDuplexMode1GFullDuplex:
        return 1000;

    default:
        return 0;
    }
}

void
RtAdapterPowerUpPhy(_In_ RT_ADAPTER *adapter)
{
    RtAdapterWritePhyUint16(adapter, 0x1F, 0x0000);
    RtAdapterWritePhyUint16(adapter, 0x0e, 0x0000);
    RtAdapterWritePhyUint16(adapter, PHY_REG_BMCR, MDI_CR_AUTO_SELECT);
}

void
RtAdapterPushPhySettings(_In_ RT_ADAPTER *adapter)
{
    RtAdapterWritePhyUint16(adapter, 0x1f, 0x0000);

    USHORT gbcr = RtAdapterReadPhyUint16(adapter, PHY_REG_GBCR);
    // clear 1000 capability
    gbcr &= ~(GBCR_1000_FULL | GBCR_1000_HALF);

    // Find out what kind of technology this Phy is capable of.
    USHORT anar = RtAdapterReadPhyUint16(adapter, PHY_REG_ANAR);

    anar &= ~(ANAR_10_HALF | ANAR_10_FULL | ANAR_100_HALF | ANAR_100_FULL | ANAR_MAC_PAUSE | ANAR_ASYM_PAUSE);

    switch (adapter->SpeedDuplex)
    {
    case RtSpeedDuplexMode10MHalfDuplex:
        anar |= ANAR_10_HALF;
        break;
    case RtSpeedDuplexMode10MFullDuplex:
        anar |= ANAR_10_FULL;
        break;
    case RtSpeedDuplexMode100MHalfDuplex:
        anar |= ANAR_100_HALF;
        break;
    case RtSpeedDuplexMode100MFullDuplex:
        anar |= ANAR_100_FULL;
        break;
    case RtSpeedDuplexMode1GFullDuplex:
        gbcr |= GBCR_1000_FULL;
        break;
    case RtSpeedDuplexModeAutoNegotiation:
        anar |= (ANAR_100_FULL | ANAR_100_HALF | ANAR_10_FULL | ANAR_10_HALF);
        gbcr |= (GBCR_1000_FULL | GBCR_1000_HALF);
        break;
    }

    switch (adapter->FlowControl)
    {
    case RtFlowControlTxEnabled:
        anar |= ANAR_ASYM_PAUSE;
        break;
    case RtFlowControlRxEnabled:
        anar |= ANAR_MAC_PAUSE;
        break;
    case RtFlowControlTxRxEnabled:
        anar |= (ANAR_ASYM_PAUSE | ANAR_MAC_PAUSE);
        break;
    }

    // update 10/100 register
    RtAdapterWritePhyUint16(adapter, PHY_REG_ANAR, anar);

    // update 1000 register
    RtAdapterWritePhyUint16(adapter, PHY_REG_GBCR, gbcr);
}

void
RtAdapterResetPhy(RT_ADAPTER *adapter)
{
    RtAdapterWritePhyUint16(adapter, 0x1f, 0x0000);

    // Reset the Phy
    RtAdapterWritePhyUint16(adapter, PHY_REG_BMCR, MDI_CR_RESET | MDI_CR_AUTO_SELECT);

    for (UINT i = PHY_RESET_TIME; i != 0; i--)
    {
        USHORT PhyRegData = RtAdapterReadPhyUint16(adapter, PHY_REG_BMCR);

        if ((PhyRegData & 0x8000) == 0)
            break;

        KeStallExecutionProcessor(20);
    }
}

void
RtAdapterInitializeAutoNegotiation(_In_ RT_ADAPTER *adapter)
{
    RtAdapterWritePhyUint16(adapter, 0x1f, 0x0000);

    RtAdapterWritePhyUint16(
        adapter, PHY_REG_BMCR, MDI_CR_AUTO_SELECT | MDI_CR_RESTART_AUTO_NEG);
}

void
RtAdapterUpdateLinkStatus(
    _In_ RT_ADAPTER *adapter)
{
    // deafult to user setting
    adapter->LinkSpeed = RtAdapterGetLinkSpeedSetting(adapter);
    adapter->DuplexMode = RtAdapterGetDuplexSetting(adapter);

    ULONG const PhyStatus = adapter->CSRAddress->PhyStatus;

    if (PhyStatus & PHY_STATUS_1000MF) adapter->LinkSpeed = 1000;
    else if (PhyStatus & PHY_STATUS_100M) adapter->LinkSpeed = 100;
    else if (PhyStatus & PHY_STATUS_10M) adapter->LinkSpeed = 10;


    // Get current duplex setting -- if bit is set then FDX is enabled
    if (PhyStatus & PHY_STATUS_FULL_DUPLEX)
    {
        adapter->DuplexMode = MediaDuplexStateFull;
    }
    else
    {
        adapter->DuplexMode = MediaDuplexStateHalf;
    }
}

void
RtAdapterCompleteAutoNegotiation(
    _In_ RT_ADAPTER *adapter)
{
    RtAdapterWritePhyUint16(adapter, 0x1F, 0x0000);
    
    // If there is a valid link
    if (adapter->CSRAddress->PhyStatus & PHY_STATUS_LINK_ON)
    {
        USHORT const PhyRegDataANER =
            RtAdapterReadPhyUint16(adapter, PHY_REG_ANER);

        USHORT const PhyRegDataBMSR =
            RtAdapterReadPhyUint16(adapter, PHY_REG_BMSR);

        // If both 1) the partner supports link auto-negotiation, and 2) auto-
        // negotiation is complete, then the Link is auto negotiated.
        adapter->LinkAutoNeg =
            (PhyRegDataANER & ANER_LP_AUTO_NEG_ABLE) &&
            (PhyRegDataBMSR & MDI_SR_AUTO_NEG_COMPLETE);

        // Ensure that context link status is up to date.
        RtAdapterUpdateLinkStatus(adapter);
    }
    else
    {
        adapter->LinkAutoNeg = false;
    }
}

void
RtAdapterNotifyLinkChange(RT_ADAPTER *adapter)
{
    NET_ADAPTER_LINK_STATE linkState;

    WdfSpinLockAcquire(adapter->Lock); {

        RtAdapterQueryLinkState(adapter, &linkState);

    } WdfSpinLockRelease(adapter->Lock);

    NetAdapterSetLinkState(adapter->NetAdapter, &linkState);
}


// Detects the NIC's current media state and updates the adapter context
// to reflect that updated media state of the hardware. The driver must serialize access
// to RT_ADAPTER.
NDIS_MEDIA_CONNECT_STATE
RtAdapterQueryMediaState(
    _In_ RT_ADAPTER *adapter)
{
    // Detect if auto-negotiation is complete and update adapter state with
    // link information.
    RtAdapterCompleteAutoNegotiation(adapter);

    UCHAR const phyStatus = adapter->CSRAddress->PhyStatus;
    if (phyStatus & PHY_LINK_STATUS)
    {
        return MediaConnectStateConnected;
    }
    else
    {
        return MediaConnectStateDisconnected;
    }
}

_Use_decl_annotations_
void
RtAdapterQueryLinkState(
    _In_    RT_ADAPTER *adapter,
    _Inout_ NET_ADAPTER_LINK_STATE *linkState
)
/*++
Routine Description:
    This routine sends a NDIS_STATUS_LINK_STATE status up to NDIS

Arguments:

    adapter         Pointer to our adapter

Return Value:

    None
--*/

{
    TraceEntryRtAdapter(adapter);

    NET_ADAPTER_AUTO_NEGOTIATION_FLAGS autoNegotiationFlags = NET_ADAPTER_AUTO_NEGOTIATION_NO_FLAGS;

    if (adapter->LinkAutoNeg)
    {
        autoNegotiationFlags |=
            NET_ADAPTER_LINK_STATE_XMIT_LINK_SPEED_AUTO_NEGOTIATED |
            NET_ADAPTER_LINK_STATE_RCV_LINK_SPEED_AUTO_NEGOTIATED |
            NET_ADAPTER_LINK_STATE_DUPLEX_AUTO_NEGOTIATED;
    }

    if (adapter->FlowControl != RtFlowControlDisabled)
    {
        autoNegotiationFlags |=
            NET_ADAPTER_LINK_STATE_PAUSE_FUNCTIONS_AUTO_NEGOTIATED;
    }

    NET_ADAPTER_PAUSE_FUNCTIONS pauseFunctions = NetAdapterPauseFunctionsUnknown;

    switch (adapter->FlowControl)
    {
    case RtFlowControlDisabled:
        pauseFunctions = NetAdapterPauseFunctionsUnsupported;
        break;
    case RtFlowControlRxEnabled:
        pauseFunctions = NetAdapterPauseFunctionsReceiveOnly;
        break;
    case RtFlowControlTxEnabled:
        pauseFunctions = NetAdapterPauseFunctionsSendOnly;
        break;
    case RtFlowControlTxRxEnabled:
        pauseFunctions = NetAdapterPauseFunctionsSendAndReceive;
        break;
    }

    NET_ADAPTER_LINK_STATE_INIT(
        linkState,
        adapter->LinkSpeed * 1'000'000,
        RtAdapterQueryMediaState(adapter),
        adapter->DuplexMode,
        pauseFunctions,
        autoNegotiationFlags);

    TraceExit();
}
