#include "vmx.h"
#include <intrin.h>
#include "VMXDefine.h"
#include "vmxs.h"
#include "VMXTools.h"

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
		//…Í«Îƒ⁄¥Ê ß∞‹
		return -1;
	}

	memset(pVcpu->VmxOnAddr, 0, PAGE_SIZE);

	pVcpu->VmxOnAddrPhys = MmGetPhysicalAddress(pVcpu->VmxOnAddr);

	//ÃÓ≥‰ID
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
		// Õ∑≈ƒ⁄¥Ê£¨÷ÿ÷√CR4
		mcr4 &= ~vcr04;
		__writecr4(mcr4);
		MmFreeContiguousMemorySpecifyCache(pVcpu->VmxOnAddr, PAGE_SIZE, MmCached);
		pVcpu->VmxOnAddr = NULL;
		pVcpu->VmxOnAddrPhys.QuadPart = 0;
	}

	return error;
}

void FullGdtDataItem(int index,short selector)
{
	GdtTable gdtTable = {0};
	AsmGetGdtTable(&gdtTable);
	//00cf9300`0000ffff
	selector &= 0xFFF8;

	ULONG limit = __segmentlimit(selector);
	PULONG item = (PULONG)(gdtTable.Base + selector);

	LARGE_INTEGER itemBase = {0};
	itemBase.LowPart = (*item & 0xFFFF0000) >> 16;
	item += 1;
	itemBase.LowPart |= (*item & 0xFF000000) | ((*item & 0xFF) << 16);

	// Ù–‘
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
	FullGdtDataItem(0, AsmReadES());
	FullGdtDataItem(1, AsmReadCS());
	FullGdtDataItem(2, AsmReadSS());
	FullGdtDataItem(3, AsmReadDS());
	FullGdtDataItem(4, AsmReadFS());
	FullGdtDataItem(5, AsmReadGS());
	FullGdtDataItem(6, AsmReadLDTR());

	GdtTable gdtTable = { 0 };
	AsmGetGdtTable(&gdtTable);

	ULONG trSelector = AsmReadTR();

	trSelector &= 0xFFF8;
	ULONG trlimit = __segmentlimit(trSelector);

	LARGE_INTEGER trBase = {0};

	PULONG trItem = (PULONG)(gdtTable.Base + trSelector);

	
	//∂¡TR
	trBase.LowPart = ((trItem[0] >> 16) & 0xFFFF) | ((trItem[1] & 0xFF) << 16) | ((trItem[1] & 0xFF000000));
	trBase.HighPart = trItem[2];

	// Ù–‘
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


	//∂¡TR
	trBase.LowPart = ((trItem[0] >> 16) & 0xFFFF) | ((trItem[1] & 0xFF) << 16) | ((trItem[1] & 0xFF000000));
	trBase.HighPart = trItem[2];

	// Ù–‘
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
	__vmx_vmwrite(HOST_GS_BASE, __readmsr(IA32_GS_BASE));

	__vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(0x174));
	__vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(0x175));
	__vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(0x176));


	//IDT GDT

	GdtTable idtTable;
	__sidt(&idtTable);

	__vmx_vmwrite(HOST_GDTR_BASE, gdtTable.Base);
	__vmx_vmwrite(HOST_IDTR_BASE, idtTable.Base);
}

void VmxInitEntry()
{
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);
	ULONG64 mseregister = ( (vmxBasic >> 55) & 1 ) ? IA32_MSR_VMX_TRUE_ENTRY_CTLS  :IA32_VMX_ENTRY_CTLS;

	ULONG64 value = VmxAdjustContorls(0x200,mseregister);
	__vmx_vmwrite(VM_ENTRY_CONTROLS, value);
	__vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);
}

void VmxInitExit()
{
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);

	ULONG64 mseregister = ((vmxBasic >> 55) & 1) ? IA32_MSR_VMX_TRUE_EXIT_CTLS : IA32_MSR_VMX_EXIT_CTLS;

	ULONG64 value = VmxAdjustContorls(0x200 | 0x8000, mseregister);
	__vmx_vmwrite(VM_EXIT_CONTROLS, value);
	__vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VM_EXIT_INTR_INFO, 0);
}

void VmxInitControls()
{
	PVMXCPUPCB pVcpu = VmxGetCurrentCPUPCB();

	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);

	ULONG64 mseregister = ((vmxBasic >> 55) & 1) ? IA32_MSR_VMX_TRUE_PINBASED_CTLS : IA32_MSR_VMX_PINBASED_CTLS;

	ULONG64 value = VmxAdjustContorls(0, mseregister);

	__vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, value);

	

	mseregister = ((vmxBasic >> 55) & 1) ? IA32_MSR_VMX_TRUE_PROCBASED_CTLS : IA32_MSR_VMX_PROCBASED_CTLS;

	ULONG64 CpuValue = CPU_BASED_ACTIVATE_MSR_BITMAP | CPU_BASED_ACTIVATE_SECONDARY_CONTROLS;

	//CpuValue |= CPU_BASED_CR3_LOAD_EXITING | CPU_BASED_CR3_STORE_EXITING;

	value = VmxAdjustContorls(CpuValue, mseregister);

	__vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, value);

	PHYSICAL_ADDRESS HPhy = {0};
	HPhy.QuadPart = -1;
	pVcpu->MsrBitMap = MmAllocateContiguousMemory(PAGE_SIZE, HPhy);

	memset(pVcpu->MsrBitMap, 0, PAGE_SIZE);

	pVcpu->MsrBitMapAddr = MmGetPhysicalAddress(pVcpu->MsrBitMap);

	__vmx_vmwrite(MSR_BITMAP, pVcpu->MsrBitMapAddr.QuadPart);

	VmxSetReadMsrBitMap(pVcpu->MsrBitMap, 0xc0000082, TRUE);
	VmxSetWriteMsrBitMap(pVcpu->MsrBitMap, 0xc0000082, TRUE);



	__vmx_vmwrite(CR3_TARGET_COUNT, 0);
	__vmx_vmwrite(CR3_TARGET_VALUE0, 0);
	__vmx_vmwrite(CR3_TARGET_VALUE1, 0);
	__vmx_vmwrite(CR3_TARGET_VALUE2, 0);
	__vmx_vmwrite(CR3_TARGET_VALUE3, 0);

	//¿©’π≤ø∑÷
	mseregister = IA32_MSR_VMX_PROCBASED_CTLS2;

	ULONG64 secValue = SECONDARY_EXEC_ENABLE_RDTSCP | SECONDARY_EXEC_ENABLE_INVPCID | SECONDARY_EXEC_XSAVES;

	if (VmxInitEpt())
	{
		secValue |= SECONDARY_EXEC_ENABLE_EPT | SECONDARY_EXEC_ENABLE_VPID;
	
		ULONG number = KeGetCurrentProcessorNumberEx(NULL);
		
		//‘ˆº”VPID ”≈ªØ–ß¬ 
		__vmx_vmwrite(VIRTUAL_PROCESSOR_ID, number + 1);
	
		//–¥»ÎEPT µÿ÷∑
		__vmx_vmwrite(EPT_POINTER, pVcpu->vmxEptp.Flags);
	
		
	}

	value = VmxAdjustContorls(secValue, mseregister);

	__vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, value);


	//…Ë÷√“Ï≥£¿πΩÿ

	//ULONG64 exceptBitMap =  1 << 3;
	//
	//__vmx_vmwrite(EXCEPTION_BITMAP, exceptBitMap);

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
		//…Í«Îƒ⁄¥Ê ß∞‹
		return -1;
	}

	memset(pVcpu->VmxcsAddr, 0, PAGE_SIZE);

	pVcpu->VmxcsAddrPhys = MmGetPhysicalAddress(pVcpu->VmxcsAddr);


	pVcpu->VmxHostStackTop = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE * 36, lowphys, heiPhy, lowphys, MmCached);

	if (!pVcpu->VmxHostStackTop)
	{
		//…Í«Îƒ⁄¥Ê ß∞‹
		
		return -1;
	}

	memset(pVcpu->VmxHostStackTop, 0, PAGE_SIZE * 36);

	pVcpu->VmxHostStackBase = (ULONG64)pVcpu->VmxHostStackTop + PAGE_SIZE * 36 - 0x200;

	//ÃÓ≥‰ID
	ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);

	*(PULONG)pVcpu->VmxcsAddr = (ULONG)vmxBasic;

	//º”‘ÿVMCS
	__vmx_vmclear(&pVcpu->VmxcsAddrPhys.QuadPart);

	__vmx_vmptrld(&pVcpu->VmxcsAddrPhys.QuadPart);

	VmxInitGuest(GuestEip, GuestEsp);

	VmxInitHost(hostEip);

	VmxInitEntry();

	VmxInitExit();

	VmxInitControls();

	return 0;
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

	if (pVcpu->MsrBitMap && MmIsAddressValid(pVcpu->MsrBitMap))
	{
		MmFreeContiguousMemory(pVcpu->MsrBitMap);

	}


	
	pVcpu->MsrBitMap = NULL;
	
	ULONG64 mcr4 = __readcr4();
	mcr4 &= ~0x2000;
	__writecr4(mcr4);
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
		DbgPrintEx(77, 0, "[db]:vmon ≥ı ºªØ ß∞‹ error = %d,cpunumber %d\r\n", error, pVcpu->cpuNumber);

		return error;
	}

	error = VmxInitVmcs(guestEip, guestEsp, hostEip);

	if (error)
	{
		DbgPrintEx(77, 0, "[db]:vmcs ≥ı ºªØ ß∞‹ error = %d,cpunumber %d\r\n", error, pVcpu->cpuNumber);

		__vmx_off();
		VmxDestory();
		return error;
	}

	//ø™∆ÙVT
	error = __vmx_vmlaunch();

	if (error)
	{
		DbgPrintEx(77, 0, "[db]:__vmx_vmlaunch ß∞‹ error = %d,cpunumber %d\r\n", error, pVcpu->cpuNumber);
		__vmx_off();
		VmxDestory();
	}
	return 0;
}

