#include "VMXTools.h"
#include "VMXDefine.h"
#include <intrin.h>

//检测Bios是否开启VT
BOOLEAN VmxIsCheckSupportVTBIOS()
{
	ULONG64 value = __readmsr(IA32_FEATURE_CONTROL);

	return (value & 0x5) == 0x5;
}


//检测CPU是否支持VT
BOOLEAN VmxIsCheckSupportVTCPUID()
{
	int cpuidinfo[4];
	__cpuidex(cpuidinfo, 1, 0);
	//CPUID 是否支持VT ecx.vmx第6位 如果为1，支持VT，否则不支持
	return (cpuidinfo[2] >> 5) & 1;
}


//检测CR4VT是否开启，如果为1 代表已经开启过了，否则没有开启
BOOLEAN VmxIsCheckSupportVTCr4()
{
	ULONG64 mcr4 = __readcr4();
	//检测CR4 VT是否开启，cr4.vmxe如果第14位为1，那么VT已经被开启，否则可以开启
	return ((mcr4 >> 13) & 1) == 0;
}


ULONG64 VmxAdjustContorls(ULONG64 value, ULONG64 msr)
{
	LARGE_INTEGER msrValue;
	msrValue.QuadPart = __readmsr(msr);
	value = (msrValue.LowPart | value ) & msrValue.HighPart;

	return value;
}


BOOLEAN VmxSetReadMsrBitMap(PUCHAR msrBitMap, ULONG64 msrAddrIndex, BOOLEAN isEnable)
{
	if (msrAddrIndex >= 0xC0000000)
	{
		msrBitMap += 1024;
		msrAddrIndex -= 0xC0000000;
	}

	ULONG64 moveByte = 0;
	ULONG64 setBit = 0;

	if (msrAddrIndex != 0)
	{
		moveByte = msrAddrIndex / 8;

		setBit = msrAddrIndex % 8;

		msrBitMap += moveByte;
	}
	
	if (isEnable)
	{
		*msrBitMap |= 1 << setBit;
	}
	else 
	{
		*msrBitMap &= ~(1 << setBit);
	}
	
	return TRUE;

}

BOOLEAN VmxSetWriteMsrBitMap(PUCHAR msrBitMap, ULONG64 msrAddrIndex, BOOLEAN isEnable)
{
	msrBitMap += 0x800;

	return VmxSetReadMsrBitMap(msrBitMap, msrAddrIndex, isEnable);

}


VOID VmxEanbleMTF(BOOLEAN isEanble)
{
	ULONG64 CpuValue = 0;
	__vmx_vmread(CPU_BASED_VM_EXEC_CONTROL, &CpuValue);
	
	if (isEanble)
	{
		CpuValue |= CPU_BASED_MONITOR_TRAP_FLAG;
	}
	else 
	{
		CpuValue &= ~CPU_BASED_MONITOR_TRAP_FLAG;
	}
	

	__vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, CpuValue);
}