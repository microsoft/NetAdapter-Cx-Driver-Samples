/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/



#ifndef _MP_DBG_H
#define _MP_DBG_H

//
// Message verbosity: lower values indicate higher urgency
//
#define MP_OFF          0
#define MP_ERROR        1
#define MP_WARN         2
#define MP_TRACE        3
#define MP_INFO         4
#define MP_LOUD         5

#if DBG

__declspec(selectany) ULONG MPDebugLevel = MP_WARN;

#define DBGPRINT(Level, Fmt) \
{ \
    if (Level <= MPDebugLevel) \
    { \
        DbgPrint(NIC_DBG_STRING); \
        DbgPrint Fmt; \
    } \
}

#define DBGPRINT_RAW(Level, Fmt) \
{ \
    if (Level <= MPDebugLevel) \
    { \
      DbgPrint Fmt; \
    } \
}

#define DBGPRINT_S(Status, Fmt) \
{ \
    ULONG dbglevel; \
    if (Status == NDIS_STATUS_SUCCESS || Status == NDIS_STATUS_PENDING) dbglevel = MP_TRACE; \
    else dbglevel = MP_ERROR; \
    DBGPRINT(dbglevel, Fmt); \
}

#define DBGPRINT_UNICODE(Level, UString) \
{ \
    if (Level <= MPDebugLevel) \
    { \
        DbgPrint(NIC_DBG_STRING); \
      mpDbgPrintUnicodeString(UString); \
   } \
}

#else   // !DBG

#define DBGPRINT(Level, Fmt)
#define DBGPRINT_RAW(Level, Fmt)
#define DBGPRINT_S(Status, Fmt)
#define DBGPRINT_UNICODE(Level, UString)

#endif  // DBG

#endif  // _MP_DBG_H

