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
#include <netadaptercx.h>
#include <netiodef.h>

// Avoid putting user headers into the precomp header.
// Exceptions here include:
// 1. Constant definitions
// 2. Definitions for hardware structures that cannot change
// 3. Forward declarations

#include "forward.h"
#include "rt_def.h"

