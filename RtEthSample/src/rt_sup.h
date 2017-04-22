/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#ifndef _RT_SUP_H
#define _RT_SUP_H

NDIS_MEDIA_CONNECT_STATE
NICGetMediaState(
    IN PMP_ADAPTER Adapter
);

void
RtIssueFullReset(
    _In_ MP_ADAPTER *adapter
);

// physet.c

VOID
ResetPhy(
    IN PMP_ADAPTER Adapter
);

VOID PhyRestartNway(
    IN PMP_ADAPTER Adapter
);

NDIS_STATUS
PhyDetect(
    IN PMP_ADAPTER Adapter
);

void
ScanAndSetupPhy(
    IN PMP_ADAPTER Adapter
);

VOID
FindPhySpeedAndDpx(
    IN PMP_ADAPTER Adapter,
    IN UINT PhyId
);


#endif //_RT_SUP_H

