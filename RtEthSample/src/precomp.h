/*++

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
    ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
    PARTICULAR PURPOSE.

    Copyright (c) Microsoft Corporation. All rights reserved

--*/

#pragma warning(disable:4214)   // bit field types other than int

#pragma warning(disable:4201)   // nameless struct/union
#pragma warning(disable:4115)   // named type definition in parentheses
#pragma warning(disable:4127)   // conditional expression is constant
#pragma warning(disable:4054)   // cast of function pointer to PVOID
#pragma warning(disable:4206)   // translation unit is empty


#include <ntddk.h>
#include <wdf.h>
#include <netadaptercx.h>
#include <ntintsafe.h>

#include "rt_def.h"

#include "rt_equ.h"
#include "rt_reg.h"

#include "mp_dbg.h"
#include "mp_def.h"
#include "mp.h"
#include "mp_nic.h"

#include "rt_sup.h"

#include "rt_func.h"



