#pragma once
#include <ntifs.h>

VOID KeGenericCallDpc(__in PKDEFERRED_ROUTINE Routine, __in_opt PVOID Context);

VOID KeSignalCallDpcDone(__in PVOID SystemArgument1);

LOGICAL KeSignalCallDpcSynchronize(__in PVOID SystemArgument2);