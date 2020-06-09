/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <ntintsafe.h>
#include <preview/netadaptercx.h>
#include <preview/netadapter.h>
#include <netiodef.h>

#include <net/checksum.h>
#include <net/logicaladdress.h>
#include <net/lso.h>
#include <net/virtualaddress.h>
#include <net\databuffer.h>
#include <net/ieee8021q.h>

// Avoid putting user headers into the precomp header.
// Exceptions here include:
// 1. Constant definitions
// 2. Definitions for hardware structures that cannot change
// 3. Forward declarations

#include "forward.h"
#include "rt_def.h"

