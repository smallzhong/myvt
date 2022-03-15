#include "vmx.h"
#include <intrin.h>
#include "VMXDefine.h"
#include "vmxs.h"

VMXCPUPCB vmxCpuPcbs[128];

PVMXCPUPCB VmxGetCPUPCB(ULONG cpuNumber)
{
	return &vmxCpuPcbs[cpuNumber];
}

PVMXCPUPCB VmxGetCurrentCPUPCB()
{
	return VmxGetCPUPCB(KeGetCurrentProcessorNumberEx(NULL));
}

int VmxInitVmOn()
{
	PVMXCPUPCB pVcpu = VmxGetCurrentCPUPCB();
	
	PHYSICAL_ADDRESS lowphys,heiPhy;
	
	lowphys.QuadPart = 0;
	heiPhy.QuadPart = -1;

	pVcpu->VmxOnAddr = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, lowphys, heiPhy, lowphys, MmCached);

	if (!pVcpu->VmxOnAddr)
	{
		//申请内存失败
		return -1;
	}

	memset(pVcpu->VmxOnAddr, 0, PAGE_SIZE);

	pVcpu->VmxOnAddrPhys = MmGetPhysicalAddress(pVcpu->VmxOnAddr);

	//填充ID
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);

	*(PULONG)pVcpu->VmxOnAddr = (ULONG)vmxBasic;

	//CR0,CR4

	ULONG64 vcr00 = __readmsr(IA32_VMX_CR0_FIXED0);
	ULONG64 vcr01 = __readmsr(IA32_VMX_CR0_FIXED1);

	ULONG64 vcr04 = __readmsr(IA32_VMX_CR4_FIXED0);
	ULONG64 vcr14 = __readmsr(IA32_VMX_CR4_FIXED1);

	ULONG64 mcr4 = __readcr4();
	ULONG64 mcr0 = __readcr0();

	mcr4 |= vcr04;
	mcr4 &= vcr14;

	mcr0 |= vcr00;
	mcr0 &= vcr01;

	//
	__writecr0(mcr0);

	__writecr4(mcr4);

	int error = __vmx_on(&pVcpu->VmxOnAddrPhys.QuadPart);

	if (error)
	{
		//释放内存，重置CR4
		mcr4 &= ~vcr04;
		__writecr4(mcr4);
		MmFreeContiguousMemorySpecifyCache(pVcpu->VmxOnAddr, PAGE_SIZE, MmCached);
		pVcpu->VmxOnAddr = NULL;
		pVcpu->VmxOnAddrPhys.QuadPart = 0;
	}

	return error;
}

void fillGdtDataItem(int index,short selector)
{
	GdtTable gdtTable = {0};
	AsmGetGdtTable(&gdtTable);

	selector &= 0xFFF8;

	ULONG limit = __segmentlimit(selector);
	PULONG item = (PULONG)(gdtTable.Base + selector);

	LARGE_INTEGER itemBase = {0};
	itemBase.LowPart = (*item & 0xFFFF0000) >> 16;
	item += 1;
	itemBase.LowPart |= (*item & 0xFF000000) | ((*item & 0xFF) << 16);

	//属性
	ULONG attr = (*item & 0x00F0FF00) >> 8;

	if (selector == 0)
	{
		attr |= 1 << 16;
	}

	__vmx_vmwrite(GUEST_ES_BASE + index * 2, itemBase.QuadPart);
	__vmx_vmwrite(GUEST_ES_LIMIT + index * 2, limit);
	__vmx_vmwrite(GUEST_ES_AR_BYTES + index * 2, attr);
	__vmx_vmwrite(GUEST_ES_SELECTOR + index * 2, selector);
}

void VmxInitGuest(ULONG64 GuestEip, ULONG64 GuestEsp)
{
	fillGdtDataItem(0, AsmReadES());
	fillGdtDataItem(1, AsmReadCS());
	fillGdtDataItem(2, AsmReadSS());
	fillGdtDataItem(3, AsmReadDS());
	fillGdtDataItem(4, AsmReadFS());
	fillGdtDataItem(5, AsmReadGS());
	fillGdtDataItem(6, AsmReadLDTR());

	GdtTable gdtTable = { 0 };
	AsmGetGdtTable(&gdtTable);

	ULONG trSelector = AsmReadTR();

	trSelector &= 0xFFF8;
	ULONG trlimit = __segmentlimit(trSelector);

	LARGE_INTEGER trBase = {0};

	PULONG trItem = (PULONG)(gdtTable.Base + trSelector);

	
	//读TR
	trBase.LowPart = ((trItem[0] >> 16) & 0xFFFF) | ((trItem[1] & 0xFF) << 16) | ((trItem[1] & 0xFF000000));
	trBase.HighPart = trItem[2];

	//属性
	ULONG attr = (trItem[1] & 0x00F0FF00) >> 8;
	__vmx_vmwrite(GUEST_TR_BASE, trBase.QuadPart);
	__vmx_vmwrite(GUEST_TR_LIMIT, trlimit);
	__vmx_vmwrite(GUEST_TR_AR_BYTES, attr);
	__vmx_vmwrite(GUEST_TR_SELECTOR, trSelector);

	__vmx_vmwrite(GUEST_CR0, __readcr0());
	__vmx_vmwrite(GUEST_CR4, __readcr4());
	__vmx_vmwrite(GUEST_CR3, __readcr3());
	__vmx_vmwrite(GUEST_DR7, __readdr(7));
	__vmx_vmwrite(GUEST_RFLAGS, __readeflags());
	__vmx_vmwrite(GUEST_RSP, GuestEsp);
	__vmx_vmwrite(GUEST_RIP, GuestEip);

	__vmx_vmwrite(VMCS_LINK_POINTER, -1);

	__vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(IA32_MSR_DEBUGCTL));
	__vmx_vmwrite(GUEST_IA32_PAT, __readmsr(IA32_MSR_PAT));
	__vmx_vmwrite(GUEST_IA32_EFER, __readmsr(IA32_MSR_EFER));

	__vmx_vmwrite(GUEST_FS_BASE, __readmsr(IA32_FS_BASE));
	__vmx_vmwrite(GUEST_GS_BASE, __readmsr(IA32_GS_BASE));

	__vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(0x174));
	__vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(0x175));
	__vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(0x176));
	

	//IDT GDT

	GdtTable idtTable;
	__sidt(&idtTable);

	__vmx_vmwrite(GUEST_GDTR_BASE, gdtTable.Base);
	__vmx_vmwrite(GUEST_GDTR_LIMIT, gdtTable.limit);
	__vmx_vmwrite(GUEST_IDTR_LIMIT, idtTable.limit);
	__vmx_vmwrite(GUEST_IDTR_BASE, idtTable.Base);

}


void VmxInitHost(ULONG64 HostEip)
{
	GdtTable gdtTable = { 0 };
	AsmGetGdtTable(&gdtTable);

	PVMXCPUPCB pVcpu = VmxGetCurrentCPUPCB();

	ULONG trSelector = AsmReadTR();

	trSelector &= 0xFFF8;

	LARGE_INTEGER trBase = { 0 };

	PULONG trItem = (PULONG)(gdtTable.Base + trSelector);


	//读TR
	trBase.LowPart = ((trItem[0] >> 16) & 0xFFFF) | ((trItem[1] & 0xFF) << 16) | ((trItem[1] & 0xFF000000));
	trBase.HighPart = trItem[2];

	//属性
	__vmx_vmwrite(HOST_TR_BASE, trBase.QuadPart);
	__vmx_vmwrite(HOST_TR_SELECTOR, trSelector);

	__vmx_vmwrite(HOST_ES_SELECTOR, AsmReadES() & 0xfff8);
	__vmx_vmwrite(HOST_CS_SELECTOR, AsmReadCS() & 0xfff8);
	__vmx_vmwrite(HOST_SS_SELECTOR, AsmReadSS() & 0xfff8);
	__vmx_vmwrite(HOST_DS_SELECTOR, AsmReadDS() & 0xfff8);
	__vmx_vmwrite(HOST_FS_SELECTOR, AsmReadFS() & 0xfff8);
	__vmx_vmwrite(HOST_GS_SELECTOR, AsmReadGS() & 0xfff8);
	


	__vmx_vmwrite(HOST_CR0, __readcr0());
	__vmx_vmwrite(HOST_CR4, __readcr4());
	__vmx_vmwrite(HOST_CR3, __readcr3());
	__vmx_vmwrite(HOST_RSP, (ULONG64)pVcpu->VmxHostStackBase);
	__vmx_vmwrite(HOST_RIP, HostEip);


	__vmx_vmwrite(HOST_IA32_PAT, __readmsr(IA32_MSR_PAT));
	__vmx_vmwrite(HOST_IA32_EFER, __readmsr(IA32_MSR_EFER));

	__vmx_vmwrite(HOST_FS_BASE, __readmsr(IA32_FS_BASE));
	__vmx_vmwrite(HOST_GS_BASE, __readmsr(IA32_GS_KERNEL_BASE));

	__vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(0x174));
	__vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(0x175));
	__vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(0x176));


	//IDT GDT

	GdtTable idtTable;
	__sidt(&idtTable);

	__vmx_vmwrite(HOST_GDTR_BASE, gdtTable.Base);
	__vmx_vmwrite(HOST_IDTR_BASE, idtTable.Base);
}

int VmxInitVmcs(ULONG64 GuestEip,ULONG64 GuestEsp, ULONG64 hostEip)
{
	PVMXCPUPCB pVcpu = VmxGetCurrentCPUPCB();

	PHYSICAL_ADDRESS lowphys, heiPhy;

	lowphys.QuadPart = 0;
	heiPhy.QuadPart = -1;

	pVcpu->VmxcsAddr = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, lowphys, heiPhy, lowphys, MmCached);

	if (!pVcpu->VmxcsAddr)
	{
		//申请内存失败
		return -1;
	}

	memset(pVcpu->VmxcsAddr, 0, PAGE_SIZE);

	pVcpu->VmxcsAddrPhys = MmGetPhysicalAddress(pVcpu->VmxcsAddr);


	pVcpu->VmxHostStackTop = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE * 36, lowphys, heiPhy, lowphys, MmCached);

	if (!pVcpu->VmxHostStackTop)
	{
		//申请内存失败
		
		return -1;
	}

	memset(pVcpu->VmxHostStackTop, 0, PAGE_SIZE * 36);

	pVcpu->VmxHostStackBase = (ULONG64)pVcpu->VmxHostStackTop + PAGE_SIZE * 36 - 0x200;

	//填充ID
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);

	*(PULONG)pVcpu->VmxcsAddr = (ULONG)vmxBasic;

	//加载VMCS
	__vmx_vmclear(&pVcpu->VmxcsAddrPhys.QuadPart);

	__vmx_vmptrld(&pVcpu->VmxcsAddrPhys.QuadPart);

	VmxInitGuest(GuestEip, GuestEsp);

	VmxInitHost(hostEip);
}

void VmxDestory()
{
	PVMXCPUPCB pVcpu = VmxGetCurrentCPUPCB();

	if (pVcpu->VmxOnAddr && MmIsAddressValid(pVcpu->VmxOnAddr))
	{
		MmFreeContiguousMemorySpecifyCache(pVcpu->VmxOnAddr, PAGE_SIZE, MmCached);
		
	}

	pVcpu->VmxOnAddr = NULL;


	if (pVcpu->VmxcsAddr && MmIsAddressValid(pVcpu->VmxcsAddr))
	{
		MmFreeContiguousMemorySpecifyCache(pVcpu->VmxcsAddr, PAGE_SIZE, MmCached);

	}

	pVcpu->VmxcsAddr = NULL;

	if (pVcpu->VmxHostStackTop && MmIsAddressValid(pVcpu->VmxHostStackTop))
	{
		MmFreeContiguousMemorySpecifyCache(pVcpu->VmxHostStackTop, PAGE_SIZE * 36, MmCached);

	}

	pVcpu->VmxHostStackTop = NULL;
}

int VmxInit(ULONG64 hostEip)
{
	
	PVMXCPUPCB pVcpu = VmxGetCurrentCPUPCB();

	pVcpu->cpuNumber = KeGetCurrentProcessorNumberEx(NULL);

	PULONG64 retAddr = (PULONG64)_AddressOfReturnAddress();
	ULONG64 guestEsp = retAddr + 1;
	ULONG64 guestEip = *retAddr;

	int error = VmxInitVmOn();

	if (error)
	{
		DbgPrintEx(77, 0, "[db]:vmon 初始化失败 error = %d,cpunumber %d\r\n", error, pVcpu->cpuNumber);

		return error;
	}

	error = VmxInitVmcs(guestEip, guestEsp, hostEip);

	if (error)
	{
		DbgPrintEx(77, 0, "[db]:vmcs 初始化失败 error = %d,cpunumber %d\r\n", error, pVcpu->cpuNumber);

		
		VmxDestory();
		return error;
	}

	return 0;
}