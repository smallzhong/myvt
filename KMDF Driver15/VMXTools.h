#pragma once

#include <ntifs.h>

BOOLEAN VmxIsCheckSupportVTBIOS();

BOOLEAN VmxIsCheckSupportVTCPUID();

BOOLEAN VmxIsCheckSupportVTCr4();

ULONG64 VmxAdjustContorls(ULONG64 value, ULONG64 msr);