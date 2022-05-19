#include "VmxEpt.h"
#include "vmx.h"
#include "VMXDefine.h"
#include <intrin.h>
#include "vmxs.h"
#include "PageHook.h"

#define EPML4_INDEX(__ADDRESS__)		((__ADDRESS__ >> 39) & 0x1FF)
#define EPDPTE_INDEX(__ADDRESS__)		((__ADDRESS__ >> 30) & 0x1FF)
#define EPDE_INDEX(__ADDRESS__)			((__ADDRESS__ >> 21) & 0x1FF)
#define EPTE_INDEX(__ADDRESS__)			((__ADDRESS__ >> 12) & 0x1FF)

#define ACCESS_EPT_READ		1
#define ACCESS_EPT_WRITE	2
#define ACCESS_EPT_EXECUTE	4

BOOLEAN VmxIsSupportEpt()
{
	ULONG64 ctls2 = __readmsr(IA32_MSR_VMX_PROCBASED_CTLS2);

	if ( ((ctls2 >> 33) & 1) == 0 ) return FALSE;
	
	ULONG64 capValue = __readmsr(IA32_MSR_VMX_EPT_VPID_CAP);

	BOOLEAN isExcute = capValue & 1;

	BOOLEAN is4Level = (capValue >> 6) & 1;

	BOOLEAN is2M = (capValue >> 16) & 1;

	if (!isExcute || !is4Level || !is2M) return FALSE;

	return TRUE;
}

BOOLEAN VmxInitEpt()
{
	if (!VmxIsSupportEpt()) return FALSE;

	ULONG64 capValue = __readmsr(IA32_MSR_VMX_EPT_VPID_CAP);

	BOOLEAN isWb = (capValue >> 14) & 1;

	// 6 是缓存回写属性 ，0 是没有缓存

	ULONG MemoryType = isWb ? 6 : 0;

	PVMXCPUPCB vmxEntry = VmxGetCurrentCPUPCB();

	vmxEntry->vmxMamgerPage = ExAllocatePool(NonPagedPool, sizeof(VMX_MAMAGER_PAGE_ENTRY));

	if (!vmxEntry->vmxMamgerPage) return FALSE;

	vmxEntry->vmxMamgerPage->pmlt[0].Flags = 0;
	vmxEntry->vmxMamgerPage->pmlt[0].ExecuteAccess = 1;
	vmxEntry->vmxMamgerPage->pmlt[0].ReadAccess = 1;
	vmxEntry->vmxMamgerPage->pmlt[0].WriteAccess = 1;
	vmxEntry->vmxMamgerPage->pmlt[0].PageFrameNumber = MmGetPhysicalAddress(&vmxEntry->vmxMamgerPage->pdptt).QuadPart / PAGE_SIZE;


	for (int i = 0; i < PDPTE_ENTRY_COUNT; i++)
	{
		vmxEntry->vmxMamgerPage->pdptt[i].Flags = 0;
		vmxEntry->vmxMamgerPage->pdptt[i].ExecuteAccess = 1;
		vmxEntry->vmxMamgerPage->pdptt[i].ReadAccess = 1;
		vmxEntry->vmxMamgerPage->pdptt[i].WriteAccess = 1;
		vmxEntry->vmxMamgerPage->pdptt[i].PageFrameNumber = MmGetPhysicalAddress(&vmxEntry->vmxMamgerPage->pdt[i][0]).QuadPart / PAGE_SIZE;
	
		for (int j = 0; j < PDE_ENTRY_COUNT; j++)
		{
			vmxEntry->vmxMamgerPage->pdt[i][j].Flags = 0;
			vmxEntry->vmxMamgerPage->pdt[i][j].ExecuteAccess = 1;
			vmxEntry->vmxMamgerPage->pdt[i][j].ReadAccess = 1;
			vmxEntry->vmxMamgerPage->pdt[i][j].WriteAccess = 1;
			vmxEntry->vmxMamgerPage->pdt[i][j].MemoryType = MemoryType;
			vmxEntry->vmxMamgerPage->pdt[i][j].LargePage = 1;
			vmxEntry->vmxMamgerPage->pdt[i][j].PageFrameNumber = i * 512 + j;
		}
	}

	//DbgBreakPoint();

	vmxEntry->vmxEptp.Flags = 0;

	vmxEntry->vmxEptp.MemoryType = MemoryType;

	vmxEntry->vmxEptp.PageWalkLength = 3;

	vmxEntry->vmxEptp.EnableAccessAndDirtyFlags = (capValue >> 21) & 1;

	vmxEntry->vmxEptp.PageFrameNumber = MmGetPhysicalAddress(&vmxEntry->vmxMamgerPage->pmlt).QuadPart / PAGE_SIZE;
	
	return TRUE;
}


PEPDE_2MB GetPDE2M_HPAByGPA(ULONG64 Gpa)
{
	PVMXCPUPCB vmxEntry = VmxGetCurrentCPUPCB();

	//EPML4INDEX
	ULONG64 pml4Index = EPML4_INDEX(Gpa);

	if (pml4Index > 0)
	{
		return NULL;
	}

	//EPDPTEINDEX
	ULONG64 pdepteIndex = EPDPTE_INDEX(Gpa);
	//PDEINDEX
	ULONG64 pdeIndex = EPDE_INDEX(Gpa);

	return &vmxEntry->vmxMamgerPage->pdt[pdepteIndex][pdeIndex];
}

PEPTE GetPTE(ULONG64 Gpa)
{
	PEPDE_2MB pde = GetPDE2M_HPAByGPA(Gpa);

	if (!pde && pde->LargePage) return NULL;

	PHYSICAL_ADDRESS addr;
	addr.QuadPart = pde->PageFrameNumber * PAGE_SIZE;

	ULONG pteIndex = EPTE_INDEX(Gpa);

	PEPTE ptes =  MmGetVirtualForPhysical(addr);

	if (!ptes) return NULL;

	return &ptes[pteIndex];
}


VOID EptSplit(PEPDE_2MB pde, ULONG64 PfNumber)
{
	PVMXCPUPCB vmxEntry = VmxGetCurrentCPUPCB();
	//我们得给切割
	PEPTE ptes = (PEPTE)ExAllocatePool(NonPagedPool, sizeof(EPTE) * 512);

	for (int i = 0; i < PTE_ENTRY_COUNT; i++)
	{
		ptes[i].ExecuteAccess = 1;
		ptes[i].WriteAccess = 1;
		ptes[i].ReadAccess = 1;
		ptes[i].MemoryType = vmxEntry->vmxEptp.MemoryType;
		ptes[i].PageFrameNumber = (pde->PageFrameNumber << 9) + i;
	}

	pde->PageFrameNumber = MmGetPhysicalAddress(ptes).QuadPart / PAGE_SIZE;

	pde->LargePage = 0;

}

PEPTE EptGetPte(ULONG64 PfNumber)
{
	PVMXCPUPCB vmxEntry = VmxGetCurrentCPUPCB();

	PEPDE_2MB pde = GetPDE2M_HPAByGPA(PfNumber);
	
	if (pde->LargePage) return NULL;

	ULONG64 ptePhy = pde->PageFrameNumber * PAGE_SIZE;

	PHYSICAL_ADDRESS ptePhyAddr = { 0 };

	ptePhyAddr.QuadPart = ptePhy;

	PEPTE ptes = (PEPTE)MmGetVirtualForPhysical(ptePhyAddr);

	ULONG64 pteindex = EPTE_INDEX(PfNumber);

	return &ptes[pteindex];
}

VOID EptHookVmCall(ULONG64 kernelCr3, ULONG64 CodePfNumber, ULONG64 DataPfNumber, PULONG64 isHook)
{
	ULONG64 cr3 = __readcr3();

	__writecr3(kernelCr3);
	
	PVMXCPUPCB vmxEntry = VmxGetCurrentCPUPCB();
	//获取HPA
	do 
	{
		ULONG64 pfnData = DataPfNumber * PAGE_SIZE;

		ULONG64 pfnCode = CodePfNumber * PAGE_SIZE;

		PEPDE_2MB pdeData = GetPDE2M_HPAByGPA(pfnData);

		PEPDE_2MB pdeCode = GetPDE2M_HPAByGPA(pfnCode);
		
		if (!pdeData || !pdeCode) break;

		if (pdeData->LargePage)
		{
			EptSplit(pdeData, pfnData);
		}

		if (pdeCode->LargePage)
		{
			EptSplit(pdeCode, pfnCode);
		}
	
		ULONG64 pteindex = EPTE_INDEX(pfnData);

		//ULONG64 pteCodeindex =  EPTE_INDEX(pfnCode);
		PEPTE pteData = EptGetPte(pfnData);
		
		PEPTE pteCode = EptGetPte(pfnCode);

		pteCode->PageFrameNumber = pteData->PageFrameNumber;

		pteCode->ReadAccess = 0;

		*isHook = TRUE;
	} while (0);
	

	__writecr3(cr3);

	Asminvept(2, &vmxEntry->vmxEptp.Flags);
}

VOID VmxEptUpdatePage(ULONG Access,ULONG64 cr3,ULONG64 LinerAddr,ULONG64 guestPhyAddress)
{
	ULONG64 startPage = (LinerAddr >> 12) << 12;

	PPageHookContext context = EptGetPageHookContext(startPage, cr3, cr3);
	
	if (!context) return;

	PEPTE pte = EptGetPte(guestPhyAddress);

	if (!pte) return;

	if (Access == ACCESS_EPT_READ)
	{
		PEPTE readPte = EptGetPte(context->OldFunAddrNumber * PAGE_SIZE);

		if (!readPte) return;

		pte->PageFrameNumber = readPte->PageFrameNumber;

		pte->ReadAccess = 1;

		pte->ExecuteAccess = 0;

		pte->WriteAccess = 0;
	}
	else if (Access == ACCESS_EPT_WRITE || Access == ACCESS_EPT_EXECUTE)
	{
		PEPTE codePte = EptGetPte(context->NewAddrPageNumber * PAGE_SIZE);

		if (!codePte) return;

		pte->PageFrameNumber = codePte->PageFrameNumber;

		pte->ReadAccess = 0;

		pte->ExecuteAccess = 1;

		pte->WriteAccess = 1;

	}

}

VOID VmxEptHandler(PGuestContext context)
{
	struct 
	{
		ULONG64 read : 1;
		ULONG64 wrire : 1;
		ULONG64 execute : 1;

		ULONG64 readable : 1;
		ULONG64 wrireable : 1;
		ULONG64 executeable : 1;
		ULONG64 un1 : 1;
		ULONG64 vaild : 1;
		ULONG64 translation : 1;
		ULONG64 un2 : 3;
		ULONG64 NMIUnblocking : 1;
		ULONG64 un3 : 51;
	}eptinfo;

	ULONG64 mrip = 0; //0x0400000 : mov eax,dword ptr ds:[0x12345678]
	ULONG64 mrsp = 0;
	ULONG64 mCr3 = 0;
	ULONG64 instLen = 0;
	ULONG64 guestLineAddress = 0;
	ULONG64 guestPhyAddress = 0;

	__vmx_vmread(EXIT_QUALIFICATION, (PULONG64)&eptinfo); //偏移量
	__vmx_vmread(GUEST_RSP, &mrsp);
	__vmx_vmread(GUEST_RIP, &mrip);
	__vmx_vmread(GUEST_CR3, &mCr3);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &instLen); // 获取指令长度

	PVMXCPUPCB vmxEntry = VmxGetCurrentCPUPCB();

	if (!eptinfo.vaild)
	{
		return;
	}
	
	//获取线性地址
	__vmx_vmread(GUEST_LINEAR_ADDRESS, &guestLineAddress);

	//获取GPA
	__vmx_vmread(GUEST_PHYSICAL_ADDRESS, &guestPhyAddress);

	//GPA 转化为HPA
	PEPDE_2MB hpaPde2M = GetPDE2M_HPAByGPA(guestPhyAddress);

	if (eptinfo.read)
	{
		//读引起的异常
		VmxEptUpdatePage(ACCESS_EPT_READ, mCr3, guestLineAddress, guestPhyAddress);
	}


	if (eptinfo.wrire)
	{
		//写引起的异常
		VmxEptUpdatePage(ACCESS_EPT_WRITE, mCr3, guestLineAddress, guestPhyAddress);
	}

	if (eptinfo.execute)
	{
		//执行引起的异常
		VmxEptUpdatePage(ACCESS_EPT_EXECUTE, mCr3, guestLineAddress, guestPhyAddress);
	}

	Asminvept(2, &vmxEntry->vmxEptp.Flags);

}