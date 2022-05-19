#pragma once
#include <ntifs.h>

typedef struct _SsdtItem
{
	PULONG funcTable;
	ULONG64 tatalCount;
	ULONG64 number;
	PUCHAR paramTable;

}SsdtItem,*PSsdtItem;

typedef struct _SsdtArray
{
	SsdtItem ssdt;
	SsdtItem ssdt1;
	SsdtItem ssdt2;
	SsdtItem ssdt3;
}SsdtArray, *PSsdtArray;

void VmxDestorySsdtHook();

void VmxInitSsdtHook();

ULONG64 VmxGetOldSysCallEntry();

void VmxSetNewSysCallEntry(ULONG64 newFuncAddr);

ULONG64 VmxSsdtSetHook(ULONG index, ULONG64 newFunc);

VOID VmxSsdTUnHook(ULONG index);