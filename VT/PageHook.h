#pragma once
#include <ntifs.h>

#define _EPT_PAGE_HOOK_MAX 20

typedef struct _PageHookContext 
{
	LIST_ENTRY list;

	PVOID NewAddr[_EPT_PAGE_HOOK_MAX];

	PVOID OldFunAddr[_EPT_PAGE_HOOK_MAX];

	ULONG64 HookCount;

	ULONG64 insLen;

	PUCHAR NewPageStartPage;

	PUCHAR OldPageStartPage;

	ULONG64 NewAddrPageNumber;

	ULONG64 OldFunAddrNumber;

	ULONG64 KernelCr3;

	ULONG64 UserCr3;

	ULONG64 isHook;
}PageHookContext,*PPageHookContext;

PPageHookContext EptGetPageHookContext(ULONG64 funAddrStartPage, ULONG64 kernelCr3, ULONG64 userCr3);

BOOLEAN EptPageHook(PVOID funAddr, PVOID newAddr);