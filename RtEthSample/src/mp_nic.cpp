/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"
#include <xfilter.h>
#if DBG
#define _FILENUMBER     'CINM'
#endif

#include "adapter.h"
#include "device.h"
#include "oid.h"
#include "trace.h"

#pragma warning(disable:28167)


// Detects the NIC's current media state and updates the adapter context
// to reflect that updated media state of the hardware. The driver must serialize access
// to MP_ADAPTER.
void
RtUpdateMediaState(
    _In_    MP_ADAPTER *Adapter)
{
    //
    // Handle the link negotiation.
    //
    ScanAndSetupPhy(Adapter);

    //
    // NDIS 6.0 miniports are required to report their link speed, link state and
    // duplex state as soon as they figure it out. NDIS does not make any assumption
    // that they are connected, etc.
    //
    Adapter->MediaState = NICGetMediaState(Adapter);

    DBGPRINT(MP_WARN, ("Media state changed to %s\n",
                       ((Adapter->MediaState == MediaConnectStateConnected) ?
                        "Connected" : "Disconnected")));

    DBGPRINT(MP_WARN, ("Adapter->LinkAutoNeg %d \n", Adapter->LinkAutoNeg));
    DBGPRINT(MP_WARN, ("RtUpdateMediaState - negotiation done\n"));
}

void
RtGetLinkState(
    _In_    MP_ADAPTER *Adapter,
    _Inout_ NET_ADAPTER_LINK_STATE *LinkState
)
/*++
Routine Description:
This routine sends a NDIS_STATUS_LINK_STATE status up to NDIS

Arguments:

Adapter         Pointer to our adapter

Return Value:

None

NOTE: called at DISPATCH level
--*/

{
    TraceEntryRtAdapter(Adapter,
        TraceLoggingString((Adapter->MediaState == MediaConnectStateConnected) ?
                "Connected" : "Disconnected", "MediaState"));

    NET_ADAPTER_AUTO_NEGOTIATION_FLAGS AutoNegotiationFlags = NET_ADAPTER_AUTO_NEGOTIATION_NO_FLAGS;

    if (Adapter->MediaState == MediaConnectStateConnected)
    {
        if (Adapter->usDuplexMode == 1)
        {
            Adapter->MediaDuplexState = MediaDuplexStateHalf;
        }
        else if (Adapter->usDuplexMode == 2)
        {
            Adapter->MediaDuplexState = MediaDuplexStateFull;
        }
        else
        {
            Adapter->MediaDuplexState = MediaDuplexStateUnknown;
        }

        //
        // NDIS 6.0 miniports report xmit and recv link speeds in bps
        //
        Adapter->LinkSpeed = Adapter->usLinkSpeed * 1000000;

        if (Adapter->LinkAutoNeg)
        {
            AutoNegotiationFlags = NET_ADAPTER_LINK_STATE_XMIT_LINK_SPEED_AUTO_NEGOTIATED |
                                   NET_ADAPTER_LINK_STATE_RCV_LINK_SPEED_AUTO_NEGOTIATED |
                                   NET_ADAPTER_LINK_STATE_DUPLEX_AUTO_NEGOTIATED;
        }
    }
    else
    {
        Adapter->MediaState = MediaConnectStateDisconnected;
        Adapter->MediaDuplexState = MediaDuplexStateUnknown;
        Adapter->LinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
    }

    NET_ADAPTER_LINK_STATE_INIT(LinkState, Adapter->LinkSpeed, Adapter->MediaState, Adapter->MediaDuplexState, NetAdapterPauseFunctionsUnknown, AutoNegotiationFlags);

    TraceExit();
}

#pragma warning(default:28167)
