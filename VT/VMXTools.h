#pragma once

#include <ntifs.h>

BOOLEAN VmxIsCheckSupportVTBIOS();

BOOLEAN VmxIsCheckSupportVTCPUID();

BOOLEAN VmxIsCheckSupportVTCr4();

ULONG64 VmxAdjustContorls(ULONG64 value, ULONG64 msr);

BOOLEAN VmxSetReadMsrBitMap(PUCHAR msrBitMap, ULONG64 msrAddrIndex,BOOLEAN isEnable);

BOOLEAN VmxSetWriteMsrBitMap(PUCHAR msrBitMap, ULONG64 msrAddrIndex, BOOLEAN isEnable);

VOID VmxEanbleMTF(BOOLEAN isEanble);