/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"
#pragma hdrstop
#pragma warning (disable: 4514 4706)

void
RtIssueFullReset(
    MP_ADAPTER *adapter
)
{
    adapter->CSRAddress->CmdReg = CR_RST;

    for (UINT i = 0; i < 20; i++)
    {
        NdisStallExecution(20);

        if (!(adapter->CSRAddress->CmdReg & CR_RST))
        {
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// Procedure: GetConnectionStatus
//
// Description: This function returns the connection status that is
//              a required indication for PC 97 specification from MS
//              the value we are looking for is if there is link to the
//              wire or not.
//
// Arguments: IN Adapter structure pointer
//
// Returns:   NdisMediaStateConnected
//            NdisMediaStateDisconnected
//-----------------------------------------------------------------------------
NDIS_MEDIA_CONNECT_STATE
NICGetMediaState(
    IN PMP_ADAPTER Adapter
)
{
    UCHAR Status = Adapter->CSRAddress->PhyStatus;

    if (Status & PHY_LINK_STATUS)
    {
        return MediaConnectStateConnected;
    }
    else
    {
        return MediaConnectStateDisconnected;
    }
}
