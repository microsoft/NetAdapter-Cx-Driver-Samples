/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

--*/

#include "precomp.h"
#include <ntintsafe.h>

#include "trace.h"
#include "device.h"

#if DBG
#define _FILENUMBER     'TINI'
#endif

typedef struct _MP_REG_ENTRY
{
    NDIS_STRING RegName;                // variable name text
    UINT FieldOffset;            // offset to MP_ADAPTER field
    UINT FieldSize;              // size (in bytes) of the field
    UINT Default;                // default value to use
    UINT Min;                    // minimum value allowed
    UINT Max;                    // maximum value allowed
}

MP_REG_ENTRY, *PMP_REG_ENTRY;

#define MP_OFFSET(field)   ((UINT)FIELD_OFFSET(MP_ADAPTER,field))
#define MP_SIZE(field)     sizeof(((PMP_ADAPTER)0)->field)

MP_REG_ENTRY NICRegTable[] =
{
    // reg value name                           Offset in MP_ADAPTER            Field size                  Default Value           Min             Max
    {NDIS_STRING_CONST("*UDPChecksumOffloadIPv4"), MP_OFFSET(UDPChksumOffv4), MP_SIZE(UDPChksumOffv4), 0, 0, 3},
    {NDIS_STRING_CONST("*SpeedDuplex"), MP_OFFSET(SpeedDuplex), MP_SIZE(SpeedDuplex), 0, 0, 6},
    {NDIS_STRING_CONST("*ReceiveBuffers"), MP_OFFSET(ReceiveBuffers), MP_SIZE(ReceiveBuffers), 64, 1, 512},
    {NDIS_STRING_CONST("*TransmitBuffers"), MP_OFFSET(TransmitBuffers), MP_SIZE(TransmitBuffers), 16, 1, 128},
    //
    {NDIS_STRING_CONST("*UDPChecksumOffloadIPv6"), MP_OFFSET(UDPChksumOffv6), MP_SIZE(UDPChksumOffv6), 0, 0, 3},
    {NDIS_STRING_CONST("*IPChecksumOffloadIPv4"), MP_OFFSET(IPChksumOffv4), MP_SIZE(IPChksumOffv4), 0, 0, 3},
    {NDIS_STRING_CONST("*TCPChecksumOffloadIPv4"), MP_OFFSET(TCPChksumOffv4), MP_SIZE(TCPChksumOffv4), 0, 0, 3},
    {NDIS_STRING_CONST("*TCPChecksumOffloadIPv6"), MP_OFFSET(TCPChksumOffv6), MP_SIZE(TCPChksumOffv6), 0, 0, 3},
    {NDIS_STRING_CONST("*WakeOnMagicPacket"), MP_OFFSET(WakeOnMagicPacketEnabled), MP_SIZE(WakeOnMagicPacketEnabled), 0, 0, 1 },
};

_Must_inspect_result_
__drv_allocatesMem(mem)
_IRQL_requires_max_(DISPATCH_LEVEL)
PMDL
RtAllocateMdl(
    _In_reads_bytes_(Length) PVOID VirtualAddress,
    _In_  UINT Length)
{
    if (PMDL Mdl = IoAllocateMdl(VirtualAddress, Length, FALSE, FALSE, NULL))
    {
        MmBuildMdlForNonPagedPool(Mdl);
        Mdl->Next = NULL;
        return Mdl;
    }

    return NULL;
}

NDIS_STATUS NICReadAdapterInfo(
    IN PMP_ADAPTER Adapter
)
/*++
Routine Description:

    Read the mac addresss from the adapter

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_INVALID_ADDRESS

--*/
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    USHORT usValue = 0;
    int i;
    UCHAR addrValue;

    DBGPRINT(MP_TRACE, ("--> NICReadAdapterInfo\n"));

    DBGPRINT(MP_INFO, ("HW Permanent Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                       Adapter->CSRAddress->ID0, Adapter->CSRAddress->ID1,
                       Adapter->CSRAddress->ID2, Adapter->CSRAddress->ID3,
                       Adapter->CSRAddress->ID4, Adapter->CSRAddress->ID5));

    for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i += 2)
    {
        if (i == 0)
        {
            addrValue = (Adapter->CSRAddress->ID1);
            usValue = (USHORT) addrValue << 8;
            addrValue = (Adapter->CSRAddress->ID0);
            usValue = usValue | (USHORT) addrValue;
        }
        else if (i == 2)
        {
            addrValue = (Adapter->CSRAddress->ID3);
            usValue = (USHORT) addrValue << 8;
            addrValue = (Adapter->CSRAddress->ID2);
            usValue = usValue | (USHORT) addrValue;
        }
        else if (i == 4)
        {
            addrValue = (Adapter->CSRAddress->ID5);
            usValue = (USHORT) addrValue << 8;
            addrValue = (Adapter->CSRAddress->ID4);
            usValue = usValue | (USHORT) addrValue;
        }

        *((PUSHORT) (&Adapter->PermanentAddress[i])) = usValue;
    }

    DBGPRINT(MP_INFO, ("Permanent Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                       Adapter->PermanentAddress[0], Adapter->PermanentAddress[1],
                       Adapter->PermanentAddress[2], Adapter->PermanentAddress[3],
                       Adapter->PermanentAddress[4], Adapter->PermanentAddress[5]));

    if (ETH_IS_MULTICAST(Adapter->PermanentAddress) ||
            ETH_IS_BROADCAST(Adapter->PermanentAddress))
    {
        DBGPRINT(MP_ERROR, ("Permanent address is invalid\n"));
        NdisWriteErrorLogEntry(
            Adapter->NdisLegacyAdapterHandle,
            NDIS_ERROR_CODE_NETWORK_ADDRESS,
            0);
        Status = NDIS_STATUS_INVALID_ADDRESS;
    }
    else
    {
        if (!Adapter->OverrideAddress)
        {
            ETH_COPY_NETWORK_ADDRESS(Adapter->CurrentAddress, Adapter->PermanentAddress);
        }

        DBGPRINT(MP_INFO, ("Current Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
                           Adapter->CurrentAddress[0], Adapter->CurrentAddress[1],
                           Adapter->CurrentAddress[2], Adapter->CurrentAddress[3],
                           Adapter->CurrentAddress[4], Adapter->CurrentAddress[5]));
    }

    DBGPRINT_S(Status, ("<-- NICReadAdapterInfo, Status=%x\n", Status));

    return Status;
}

NTSTATUS
NICReadRegParameters(
    IN PMP_ADAPTER Adapter
)
/*++
Routine Description:

    Read the following from the registry
    1. All the parameters
    2. NetworkAddres

Arguments:

    Adapter                         Pointer to our adapter
    MiniportAdapterHandle           For use by NdisOpenConfiguration

Return Value:

    STATUS_SUCCESS
    STATUS_INSUFFICIENT_RESOURCES

--*/
{
    TraceEntryRtAdapter(Adapter);

    NTSTATUS status = STATUS_SUCCESS;
    
    NETCONFIGURATION configuration;
    GOTO_IF_NOT_NT_SUCCESS(Exit, status, 
        NetAdapterOpenConfiguration(Adapter->Adapter, WDF_NO_OBJECT_ATTRIBUTES, &configuration));

    // read all the registry values
    for (UINT i = 0; i < ARRAYSIZE(NICRegTable); i++)
    {
        PMP_REG_ENTRY pRegEntry = &NICRegTable[i];

        //
        // Driver should NOT fail the initialization only because it can not
        // read the registry
        //
        PUCHAR pointer = (PUCHAR) Adapter + pRegEntry->FieldOffset;

        // Get the configuration value for a specific parameter.  Under NT the
        // parameters are all read in as DWORDs.
        ULONG value = 0;
        status = NetConfigurationQueryUlong(
            configuration,
            NET_CONFIGURATION_QUERY_ULONG_NO_FLAGS,
            &pRegEntry->RegName,
            &value);

        // If the parameter was present, then check its value for validity.

        if (NT_SUCCESS(status))
        {
            // Check that param value is not too small or too large

            if (value < pRegEntry->Min ||
                value > pRegEntry->Max)
            {
                value = pRegEntry->Default;
            }
        }
        else
        {
            value = pRegEntry->Default;
            status = STATUS_SUCCESS;
        }

        TraceLoggingWrite(
            RealtekTraceProvider,
            "ReadConfiguration",
            TraceLoggingRtAdapter(Adapter),
            TraceLoggingUnicodeString(&pRegEntry->RegName, "Key"),
            TraceLoggingUInt32(value, "Value"));

        //
        // Store the value in the adapter structure.
        //
        switch (pRegEntry->FieldSize)
        {
        case 1:
            *((PUCHAR) pointer) = (UCHAR) value;
            break;

        case 2:
            *((PUSHORT) pointer) = (USHORT) value;
            break;

        case 4:
            *((PULONG) pointer) = (ULONG) value;
            break;

        default:
            DBGPRINT(MP_ERROR, ("Bogus field size %d\n", pRegEntry->FieldSize));
            break;
        }
    }

    // Read NetworkAddress registry value
    // Use it as the current address if any
    UCHAR NetworkAddress[ETH_LENGTH_OF_ADDRESS];
    ULONG OutputSize;

    status = NetConfigurationQueryNetworkAddress(
        configuration,
        ARRAYSIZE(NetworkAddress),
        NetworkAddress,
        &OutputSize);

    if ((status == STATUS_SUCCESS) && (OutputSize == ETH_LENGTH_OF_ADDRESS))
    {
        if ((ETH_IS_MULTICAST(NetworkAddress) || ETH_IS_BROADCAST(NetworkAddress)))
        {
            DBGPRINT(MP_ERROR,
                ("Overriding NetworkAddress is invalid - %02x-%02x-%02x-%02x-%02x-%02x\n",
                    NetworkAddress[0], NetworkAddress[1], NetworkAddress[2],
                    NetworkAddress[3], NetworkAddress[4], NetworkAddress[5]));
        }
        else
        {
            ETH_COPY_NETWORK_ADDRESS(Adapter->CurrentAddress, NetworkAddress);
            Adapter->OverrideAddress = TRUE;
        }
    }

    status = STATUS_SUCCESS;

    // Decode SpeedDuplex
    if (Adapter->SpeedDuplex)
    {
        switch (Adapter->SpeedDuplex)
        {
        case SPEED_DUPLUX_MODE_10M_HALF_DUPLEX:
            Adapter->AiTempSpeed = 10;
            Adapter->AiForceDpx = 1;
            break;

        case SPEED_DUPLUX_MODE_10M_FULL_DUPLEX:
            Adapter->AiTempSpeed = 10;
            Adapter->AiForceDpx = 2;
            break;

        case SPEED_DUPLUX_MODE_100M_HALF_DUPLEX:
            Adapter->AiTempSpeed = 100;
            Adapter->AiForceDpx = 1;
            break;

        case SPEED_DUPLUX_MODE_100M_FULL_DUPLEX:
            Adapter->AiTempSpeed = 100;
            Adapter->AiForceDpx = 2;
            break;

        case SPEED_DUPLUX_MODE_1G_FULL_DUPLEX:
            Adapter->AiTempSpeed = 1000;
            Adapter->AiForceDpx = 2;
            break;
        default:
            Adapter->SpeedDuplex = SPEED_DUPLUX_MODE_AUTO_NEGOTIATION;
            Adapter->AiTempSpeed = 0;
            Adapter->AiForceDpx = 0;
            break;
        }
    }

    //
    // initial number of TX and RX
    //
    Adapter->NumTcb = Adapter->TransmitBuffers;

    if (Adapter->NumTcb > MAX_TCB) Adapter->NumTcb = MAX_TCB;

    if (Adapter->NumTcb < MIN_TCB) Adapter->NumTcb = MIN_TCB;

Exit:
    if (configuration)
    {
        NetConfigurationClose(configuration);
    }

    TraceExitResult(status);
    return status;
}

NTSTATUS
RtAdapterAllocateTallyBlock(
    _In_ PMP_ADAPTER Adapter
)
/*++
Routine Description:

    Allocate all the memory blocks for send, receive and others

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    STATUS_SUCCESS
    STATUS_FAILURE
    STATUS_INSUFFICIENT_RESOURCES

--*/
{
    TraceEntryRtAdapter(Adapter);

    NTSTATUS status = STATUS_SUCCESS;

    //
    // Allocate memory for Tally counter.
    //
    WDFCOMMONBUFFER HwTallyMemAlloc = WDF_NO_HANDLE;

    GOTO_IF_NOT_NT_SUCCESS(Exit, status,
        WdfCommonBufferCreate(
            Adapter->DmaEnabler,
            sizeof(GDUMP_TALLY),
            WDF_NO_OBJECT_ATTRIBUTES,
            &HwTallyMemAlloc));

    Adapter->GTally = static_cast<GDUMP_TALLY*>(WdfCommonBufferGetAlignedVirtualAddress(HwTallyMemAlloc));
    Adapter->TallyPhy = WdfCommonBufferGetAlignedLogicalAddress(HwTallyMemAlloc);

    RtlZeroMemory(Adapter->GTally, sizeof(*Adapter->GTally));

Exit:
    TraceExitResult(status);
    return status;
}

void
NICInitializeAdapter(
    _In_ MP_ADAPTER *adapter)
/*++
Routine Description:

    Initialize the adapter and set up everything

Arguments:

    adapter     Pointer to our adapter

--*/
{
    // set up our link indication variable
    // it doesn't matter what this is right now because it will be
    // set correctly if link fails
    adapter->MediaState = MediaConnectStateUnknown;
    adapter->MediaDuplexState = MediaDuplexStateUnknown;
    adapter->LinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
    
    // Configure the adapter
    HwConfigure(adapter);

    // Program current address
    HwSetupIAAddress(adapter);

    // Clear the internal counters
    HwClearAllCounters(adapter);
}

void
HwConfigure(
    _In_ MP_ADAPTER *adapter)
/*++
Routine Description:

    Configure the hardware

Arguments:

    adapter     Pointer to our adapter

--*/
{
    UCHAR TmpUchar = adapter->CSRAddress->REG_F0_F3.RESV_F3;
    TmpUchar |= BIT_2;
    adapter->CSRAddress->REG_F0_F3.RESV_F3 = TmpUchar;

    //
    // NIC hardware register to default
    //
    RtIssueFullReset(adapter);

    adapter->CSRAddress->DTCCRHigh = NdisGetPhysicalAddressHigh(adapter->TallyPhy);
    adapter->CSRAddress->DTCCRLow = NdisGetPhysicalAddressLow(adapter->TallyPhy);
}


void
HwSetupIAAddress(
    _In_ MP_ADAPTER *adapter)
/*++
Routine Description:

    Set up the individual MAC address

Arguments:

    adapter     Pointer to our adapter

--*/
{
    EnableCR9346Write(adapter); {

        ULONG TempUlong = adapter->CurrentAddress[3];
        TempUlong = TempUlong << 8;
        TempUlong = TempUlong + adapter->CurrentAddress[2];
        TempUlong = TempUlong << 8;
        TempUlong = TempUlong + adapter->CurrentAddress[1];
        TempUlong = TempUlong << 8;
        TempUlong = TempUlong + adapter->CurrentAddress[0];

        PULONG idReg = (PULONG)& adapter->CSRAddress->ID0;
        *idReg = TempUlong;

        TempUlong = adapter->CurrentAddress[5];
        TempUlong = TempUlong << 8;
        TempUlong = TempUlong + adapter->CurrentAddress[4];

        idReg = (PULONG)&adapter->CSRAddress->ID4;
        *idReg = TempUlong;

    } DisableCR9346Write(adapter);
}


void
HwClearAllCounters(
    _In_ MP_ADAPTER *adapter)
/*++
Routine Description:

    This routine will clear the hardware error statistic counters

Arguments:

    Adapter     Pointer to our adapter
    
--*/
{
    adapter->CSRAddress->DTCCRLow = NdisGetPhysicalAddressLow(adapter->TallyPhy) | DTCCR_Clr;
}
