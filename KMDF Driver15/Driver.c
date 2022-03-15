#include<ntifs.h> 
#include <ntddk.h>
#include <ntstrsafe.h>
#include "VMXTools.h"
#include "vmx.h"
#include "vmxs.h"

VOID KeGenericCallDpc(__in PKDEFERRED_ROUTINE Routine,__in_opt PVOID Context);

VOID KeSignalCallDpcDone(__in PVOID SystemArgument1);

LOGICAL KeSignalCallDpcSynchronize(__in PVOID SystemArgument2);

VOID driverunload(_In_ struct _DRIVER_OBJECT* DriverObject)
{
	KdPrintEx((77, 0, "unload\r\n"));
}

VOID VmxStartVT(_In_ struct _KDPC *Dpc,_In_opt_ PVOID DeferredContext,_In_opt_ PVOID SystemArgument1,_In_opt_ PVOID SystemArgument2)
{
	if (VmxIsCheckSupportVTCPUID())
	{
		DbgPrintEx(77, 0, "[db]:VmxIsCheckSupportVTCPUID  number = %d\r\n", KeGetCurrentProcessorNumber());
	}

	if (VmxIsCheckSupportVTBIOS())
	{
		DbgPrintEx(77, 0, "[db]:VmxIsCheckSupportVTBIOS  number = %d\r\n", KeGetCurrentProcessorNumber());
	}

	if (VmxIsCheckSupportVTCr4())
	{
		DbgPrintEx(77, 0, "[db]:VmxIsCheckSupportVTCr4  number = %d\r\n", KeGetCurrentProcessorNumber());
	}

	VmxInit(DeferredContext);

	KeSignalCallDpcDone(SystemArgument1);
	KeSignalCallDpcSynchronize(SystemArgument2);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pReg)
{
	KdPrintEx((77, 0, "entry\r\n"));

	KeGenericCallDpc(VmxStartVT, NULL);

	pDriver->DriverUnload = driverunload;
	return STATUS_SUCCESS;
}