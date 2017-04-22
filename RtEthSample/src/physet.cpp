/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"
#pragma hdrstop
#pragma warning (disable: 4514)


//
// first time PHY try to link
//
NDIS_STATUS PhyDetect(IN PMP_ADAPTER Adapter)
{
    SetupPhyAutoNeg(Adapter);
    PhyRestartNway(Adapter);

    return NDIS_STATUS_SUCCESS;
}

void
ScanAndSetupPhy(IN PMP_ADAPTER Adapter)
{
    USHORT PhyRegDataANER;
    USHORT PhyRegDataBMSR;
    BOOLEAN LinkStsOn;

    MP_WritePhyUshort(Adapter, 0x1F, 0x0000);

    //
    // wait for auto-negotiation to complete
    //

    LinkStsOn = FALSE;
    if (Adapter->CSRAddress->PhyStatus & PHY_STATUS_LINK_ON)
    {
        LinkStsOn = TRUE;
    }

    //
    // If there is a valid link
    //
    if (LinkStsOn)
    {
        PhyRegDataANER = MP_ReadPhyUshort(Adapter, PHY_REG_ANER);
        PhyRegDataBMSR = MP_ReadPhyUshort(Adapter, PHY_REG_BMSR);

        if ((PhyRegDataANER & ANER_LP_AUTO_NEG_ABLE) &&
                (PhyRegDataBMSR & MDI_SR_AUTO_NEG_COMPLETE))
        {
            DBGPRINT(MP_WARN, ("   Detecting Speed/Dpx from NWAY connection\n"));
            Adapter->LinkAutoNeg = TRUE;
        }

        FindPhySpeedAndDpx(Adapter, 0);
    }
    else
    {
        Adapter->LinkAutoNeg = FALSE;
        Adapter->usLinkSpeed = 10;
        Adapter->usDuplexMode = 1;


        DBGPRINT(MP_WARN, ("No Valid Link\n"));
    }
}

void SetupPhyAutoNeg(IN PMP_ADAPTER Adapter)
{
    BOOLEAN ForcePhySetting = FALSE;
    USHORT PhyRegDataANAR;
    USHORT PhyRegDataGBCR;
    USHORT PhyRegDataBMCR;

    DBGPRINT(MP_WARN, ("AiTempSpeed %d SpeedDuplex %d \n", Adapter->AiTempSpeed, Adapter->SpeedDuplex));
    DBGPRINT(MP_WARN, ("AiForceDpx %d \n", Adapter->AiForceDpx));

    MP_WritePhyUshort(Adapter, 0x1f, 0x0000);

    PhyRegDataGBCR = MP_ReadPhyUshort(Adapter, PHY_REG_GBCR);
    // clear 1000 capability
    PhyRegDataGBCR &= ~(GBCR_1000_FULL | GBCR_1000_HALF);

    //
    // Find out what kind of technology this Phy is capable of.
    //
    PhyRegDataANAR = MP_ReadPhyUshort(Adapter, PHY_REG_ANAR);

    PhyRegDataANAR &= ~(ANAR_10_HALF | ANAR_10_FULL | ANAR_100_HALF | ANAR_100_FULL | ANAR_MAC_PAUSE | ANAR_ASYM_PAUSE);

    PhyRegDataBMCR = 0;

    if (((Adapter->AiTempSpeed) || (Adapter->AiForceDpx)))
    {
        if (Adapter->AiTempSpeed == 10)
        {
            // If half duplex is forced

            if (Adapter->AiForceDpx == 1)
            {
                DBGPRINT(MP_INFO, ("   Forcing 10mb 1/2 duplex\n"));
                PhyRegDataANAR |= ANAR_10_HALF;
                ForcePhySetting = TRUE;
            }

            // If full duplex is forced
            else if (Adapter->AiForceDpx == 2)
            {
                DBGPRINT(MP_INFO, ("   Forcing 10mb full duplex\n"));

                PhyRegDataANAR |= (ANAR_10_FULL | ANAR_10_HALF);

                ForcePhySetting = TRUE;
            }
        }

        //
        // If speed is forced to 100mb
        //
        else if (Adapter->AiTempSpeed == 100)
        {
            // If half duplex is forced

            if (Adapter->AiForceDpx == 1)
            {
                DBGPRINT(MP_INFO, ("   Forcing 100mb half duplex\n"));

                PhyRegDataANAR |= (ANAR_100_HALF | ANAR_10_FULL | ANAR_10_HALF);

                ForcePhySetting = TRUE;
            }

            // If full duplex is forced
            else if (Adapter->AiForceDpx == 2)
            {
                DBGPRINT(MP_INFO, ("   Forcing 100mb full duplex\n"));

                PhyRegDataANAR |= (ANAR_100_FULL | ANAR_100_HALF | ANAR_10_FULL | ANAR_10_HALF);

                ForcePhySetting = TRUE;
            }
        }

        //
        // If speed is forced to 1000mb
        //
        else if (Adapter->AiTempSpeed == 1000)
        {
            DBGPRINT(MP_INFO, ("   Forcing 1000mb auto duplex\n"));

            PhyRegDataGBCR |= (GBCR_1000_FULL | GBCR_1000_HALF);

            ForcePhySetting = TRUE;
        }
    }
    else
    {
        //Auto Negotiation
        PhyRegDataANAR |= (ANAR_100_FULL | ANAR_100_HALF | ANAR_10_FULL | ANAR_10_HALF);
        PhyRegDataGBCR |= (GBCR_1000_FULL | GBCR_1000_HALF);

        ForcePhySetting = TRUE;
    }

    //
    //10/100 +++
    //
    MP_WritePhyUshort(Adapter, PHY_REG_ANAR, PhyRegDataANAR);

    //
    //10/100 ---
    //


    //
    //1000 +++
    //
    MP_WritePhyUshort(Adapter, PHY_REG_GBCR, PhyRegDataGBCR);
    //
    //1000 ---
    //
}

//-----------------------------------------------------------------------------
// Procedure:   FindPhySpeedAndDpx
//
// Description: This routine will figure out what line speed and duplex mode
//              the PHY is currently using.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      PhyId - The ID of the PHY in question.
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------

VOID FindPhySpeedAndDpx(
    IN PMP_ADAPTER Adapter,
    IN UINT PhyId
)
{
    UCHAR PhyStatus;

    UNREFERENCED_PARAMETER(PhyId);

    Adapter->usLinkSpeed = Adapter->AiTempSpeed;
    Adapter->usDuplexMode = (USHORT) Adapter->AiForceDpx;


    PhyStatus = Adapter->CSRAddress->PhyStatus;

    if (PhyStatus & PHY_STATUS_1000MF) Adapter->usLinkSpeed = 1000;
    else if (PhyStatus & PHY_STATUS_100M) Adapter->usLinkSpeed = 100;
    else if (PhyStatus & PHY_STATUS_10M) Adapter->usLinkSpeed = 10;


    //
    // Get current duplex setting -- if bit is set then FDX is enabled
    //
    if (PhyStatus & PHY_STATUS_FULL_DUPLEX)
    {
        Adapter->usDuplexMode = 2;
    }
    else
    {
        Adapter->usDuplexMode = 1;
    }

    Adapter->RxFlowCtrl = FALSE;
    Adapter->TxFlowCtrl = FALSE;

    if (PhyStatus & PHY_STATUS_RX_FLOW_CTRL) Adapter->RxFlowCtrl = TRUE;
    if (PhyStatus & PHY_STATUS_TX_FLOW_CTRL) Adapter->TxFlowCtrl = TRUE;
}


//-----------------------------------------------------------------------------
// Procedure:   ResetPhy
//
// Description: This routine will reset the PHY that the adapter is currently
//              configured to use.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//
// Returns:
//      NOTHING
//-----------------------------------------------------------------------------

VOID
ResetPhy(
    IN PMP_ADAPTER Adapter
)
{
    USHORT PhyRegData = 0;
    UINT i;

    MP_WritePhyUshort(Adapter, 0x1f, 0x0000);

    //
    // Reset the Phy
    //
    MP_WritePhyUshort(Adapter, PHY_REG_BMCR, (MDI_CR_RESET | MDI_CR_AUTO_SELECT));


    for (i = PHY_RESET_TIME; i != 0; i--)
    {
        PhyRegData = MP_ReadPhyUshort(Adapter, PHY_REG_BMCR);

        if ((PhyRegData & 0x8000) == 0)
            break;

        NdisStallExecution(20);
    }
}

VOID PhyRestartNway(IN PMP_ADAPTER Adapter)
{
    USHORT PhyRegDataBMCR;

    MP_WritePhyUshort(Adapter, 0x1f, 0x0000);
    PhyRegDataBMCR = MDI_CR_AUTO_SELECT | MDI_CR_RESTART_AUTO_NEG;
    MP_WritePhyUshort(Adapter, PHY_REG_BMCR, PhyRegDataBMCR);
}
