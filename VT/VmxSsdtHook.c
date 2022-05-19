#include "VmxSsdtHook.h"
#include "ssdt\ssdt.h"
#include "ssdt\SearchCode.h"
#include <intrin.h>
#include "VMXDefine.h"

ULONG64 g_oldSysCallEntry;
ULONG64 g_oldSyxxxxx;

PUCHAR gSsdtHookBitMap = NULL;
ULONG gSsdtFunction[0x1000] = {0};

SsdtArray gSsdtSeviceArray;
PSsdtArray pgSsdtSeviceArray =NULL;


ULONG64 KiSaveDebugRegisterState = 0;
ULONG64 KiUmsCallEntry = 0;
ULONG64 gSsdtRet1 = 0;
ULONG64 gSsdtRet2 = 0;

ULONG64 VmxGetOldSysCallEntry()
{
	return g_oldSysCallEntry;
}

void VmxInitSsdtHook()
{
	g_oldSysCallEntry = __readmsr(IA32_MSR_SYSENTRY);

	SSDTStruct * ssdt = SSDTfind();
	gSsdtSeviceArray.ssdt.number = ssdt->NumberOfServices;
	gSsdtSeviceArray.ssdt.paramTable = ssdt->pArgumentTable;
	gSsdtSeviceArray.ssdt.tatalCount = ssdt->pCounterTable;

	gSsdtSeviceArray.ssdt.funcTable = gSsdtFunction;

	memcpy(gSsdtSeviceArray.ssdt.funcTable, ssdt->pServiceTable, ssdt->NumberOfServices * 4);

	gSsdtHookBitMap = ExAllocatePool(NonPagedPool, PAGE_SIZE);

	pgSsdtSeviceArray = &gSsdtSeviceArray;

	memset(gSsdtHookBitMap, 0, PAGE_SIZE);

	gSsdtRet1 = g_oldSysCallEntry + 0x15;
	gSsdtRet2 = SearchNtCodeHead("F783********4D0F45D3423B***0F*****4E8B1417", 0);
	KiSaveDebugRegisterState = SearchNtCodeHead("65********0F**0F**4889**4889**0F**0F**4889**4889**0F**0F**4889**4889*****33C00F**65********74*66F7", 0);
	KiUmsCallEntry = SearchNtCodeHead("48******0F29******0F29******440F*******440F*******440F*******440F*******440F*******440F*******440F", 0);

	
}

void VmxDestorySsdtHook()
{
	
	__writemsr(IA32_MSR_SYSENTRY, g_oldSysCallEntry);
	ExFreePool(gSsdtHookBitMap);
}

void VmxSetNewSysCallEntry(ULONG64 newFuncAddr)
{
	__writemsr(IA32_MSR_SYSENTRY, newFuncAddr);
}

ULONG64 VmxSsdtSetHook(ULONG index, ULONG64 newFunc)
{
	SSDTStruct * ssdt = SSDTfind();

	LONG offset = ssdt->pServiceTable[index];
	LONG paramCount = offset & 0xf;
	
	LONG64 xoffset =  offset >> 4;

	LONG newOffset = newFunc - (ULONG64)gSsdtSeviceArray.ssdt.funcTable;

	newOffset = (newOffset << 4) | paramCount;

	gSsdtSeviceArray.ssdt.funcTable[index] = newOffset;

	gSsdtHookBitMap[index] = 1;

	return (ULONG64)ssdt->pServiceTable + xoffset;
}


VOID VmxSsdTUnHook(ULONG index)
{
	gSsdtHookBitMap[index] = 0;
}