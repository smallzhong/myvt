#include "PageHook.h"
#include <intrin.h>
#include "ssdt\AsmCode.h"
#include "vmxs.h"
#include "VMXDefine.h"
#include "ExportApi.h"
#include "ssdt\SearchCode.h"

PageHookContext gPageHookContext = {0};

PVOID PsGetProcessWow64Process(PEPROCESS Process);

BOOLEAN IsCurrentProcessX64()
{
	PEPROCESS Process = PsGetCurrentProcess();

	return PsGetProcessWow64Process(Process) == NULL;
}

int GetHookLen(ULONG64 Addr, ULONG64 size,BOOLEAN isX64)
{
	PUCHAR tempAddr = Addr;
	int totalSize = 0;
	int len = 0;

	if (isX64)
	{
	
		do
		{
			len = insn_len_x86_64((ULONG64)tempAddr);

			tempAddr = tempAddr + len;

			totalSize += len;

		} while (totalSize < size);
	}
	else 
	{
		do
		{
			len = insn_len_x86_32((ULONG64)tempAddr);

			tempAddr = tempAddr + len;

			totalSize += len;

		} while (totalSize < size);
	}
	
	return totalSize;
}


ULONG64 GetCurrentProcessUserCr3()
{
	PEPROCESS Process = PsGetCurrentProcess();
	
	ULONG number = GetWindowsVersionNumber();
	ULONG64 offset = 0;
	switch (number)
	{
	case 7:
		offset = 0x110;
		break;
	case 8:
	case 1507:
	case 1511:
	case 1607:
	case 1703:
	case 1709:
		offset = 0x278;
		break;
	case 1803:
	case 1809:
		offset = 0x280;
		break;
	case 1903:
	case 1909:
		offset = 0x280;
		break;
	case 2004:
	case 2009:
	case 2011:
	case 2012:
	case 2013:
		offset = 0x388;
		break;
	default:
		offset = 0x388;
		break;
	}

	ULONG64 userCr3 = *(PULONG64)((ULONG_PTR)Process + offset);

	if (userCr3 & 1 == 0)
	{
		userCr3 = 1;
	}

	return userCr3;
}

VOID EptInitPageHookContext(PPageHookContext context)
{
	memset(context, 0, sizeof(PageHookContext));

	ULONG64 kernelCr3 = __readcr3();

	ULONG64 userCr3 = GetCurrentProcessUserCr3();
	
	context->isHook = FALSE;

	context->KernelCr3 = kernelCr3;
	
	context->UserCr3 = userCr3;

	InitializeListHead(&context->list);
}

PPageHookContext EptGetPageHookContext(ULONG64 funAddrStartPage, ULONG64 kernelCr3, ULONG64 userCr3)
{
	if (funAddrStartPage == 0) return NULL;

	PPageHookContext headList = (PPageHookContext)&gPageHookContext.list;

	PPageHookContext next = headList;

	PPageHookContext findContext = NULL;

	if (IsListEmpty(headList)) return NULL;
	
	do 
	{

		if (next->OldPageStartPage == funAddrStartPage &&
			(next->KernelCr3 == kernelCr3 || (userCr3 != 1 && next->UserCr3 == userCr3)))
		{
			findContext = next;
			break;
		}
	
		next = next->list.Flink;

	} while (next != headList);

	return findContext;
}

VOID EptPageHookVmCallDpc(_In_ struct _KDPC *Dpc, _In_opt_ PVOID DeferredContext, _In_opt_ PVOID SystemArgument1, _In_opt_ PVOID SystemArgument2)
{
	PPageHookContext phContext = (PPageHookContext)DeferredContext;
	
	ULONG64 RetIsHook = 0;

	AsmVmCall(__EPT_PAGE_HOOK, phContext->KernelCr3, phContext->NewAddrPageNumber, phContext->OldFunAddrNumber, &RetIsHook);

	phContext->isHook = TRUE;

	DbgPrintEx(77, 0, "[db]:vt cpu number = %d hook %llx,isHook %lld\r\n", KeGetCurrentProcessorNumberEx(NULL), phContext->OldPageStartPage, RetIsHook);

	

	KeSignalCallDpcDone(SystemArgument1);
	KeSignalCallDpcSynchronize(SystemArgument2);
}

BOOLEAN EptPageHook(PVOID funAddr, PVOID newAddr)
{
	if (!MmIsAddressValid(funAddr) || !MmIsAddressValid(newAddr))
	{
		return FALSE;
	}

	ULONG64 funAddrStartPage = ((ULONG64)funAddr >> 12) << 12;

	ULONG64 kernelCr3 = __readcr3();

	ULONG64 userCr3 = GetCurrentProcessUserCr3();
	
	PPageHookContext phContext = EptGetPageHookContext(funAddrStartPage, kernelCr3, userCr3);

	if (gPageHookContext.list.Flink == 0)
	{
		InitializeListHead(&gPageHookContext.list);
	}

	if (!phContext)
	{
		phContext = ExAllocatePool(NonPagedPool, sizeof(PageHookContext));

		if (!phContext) return FALSE;

		EptInitPageHookContext(phContext);
		
		phContext->OldPageStartPage = funAddrStartPage;

		//
	}


	phContext->OldFunAddr[phContext->HookCount] = funAddr;

	phContext->OldFunAddr[phContext->HookCount] = newAddr;

	phContext->HookCount++;

	//创建假页
	if (!phContext->NewPageStartPage)
	{
		PHYSICAL_ADDRESS heightPhy = {0};
		
		heightPhy.QuadPart = -1;

		PUCHAR newPage = MmAllocateContiguousMemory(PAGE_SIZE, heightPhy);

		//复制原始内容
		memcpy(newPage, funAddrStartPage, PAGE_SIZE);

		phContext->NewPageStartPage = newPage;


		phContext->NewAddrPageNumber = MmGetPhysicalAddress(phContext->NewPageStartPage).QuadPart / PAGE_SIZE;
		
		phContext->OldFunAddrNumber = MmGetPhysicalAddress(funAddrStartPage).QuadPart / PAGE_SIZE;
		
	}

	ULONG64 codeOffset = (ULONG64)funAddr - funAddrStartPage;
	//构建 HOOK
	BOOLEAN isX64 = IsCurrentProcessX64();
	
	if (isX64)
	{
		// push 0x12345678789798
		char bufcode[] = 
		{ 
			0x68,0x78,0x56,0x34,0x12,
			0xC7,0x44,0x24,0x04,0x01,0x00,0x00,0x00,
			0xc3
		};

		*(PULONG)&bufcode[1] = (ULONG)((ULONG64)newAddr & 0xFFFFFFFF);

		*(PULONG)&bufcode[9] = (ULONG)(((ULONG64)newAddr >> 32) & 0xFFFFFFFF);

		int inslen = GetHookLen(funAddr, sizeof(bufcode), isX64);
		
		phContext->insLen = inslen;

		memcpy(phContext->NewPageStartPage + codeOffset, bufcode, sizeof(bufcode));
	}
	else 
	{
		char bufcode[] =
		{
			0x68,0x78,0x56,0x34,0x12,
			0xc3,
		};

		*(PULONG)&bufcode[1] = (ULONG)((ULONG64)newAddr & 0xFFFFFFFF);

		int inslen = GetHookLen(funAddr, sizeof(bufcode), isX64);

		phContext->insLen = inslen;

		memcpy(phContext->NewPageStartPage + codeOffset, bufcode, sizeof(bufcode));
	}

	//插入链表
	if (IsListEmpty(&phContext->list))
	{
		InsertTailList(&gPageHookContext.list, &phContext->list);
	}
	
	//调用vmcall进入VT 去HOOK
	KeGenericCallDpc(EptPageHookVmCallDpc, phContext);

	return TRUE;
}