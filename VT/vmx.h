#pragma once
#include <ntifs.h>
#include "VmxEpt.h"

typedef struct _VMXCPUPCB 
{
	ULONG cpuNumber;
	PVOID VmxOnAddr;
	PHYSICAL_ADDRESS VmxOnAddrPhys;

	PVOID VmxcsAddr;
	PHYSICAL_ADDRESS VmxcsAddrPhys;

	PVOID VmxHostStackTop;  //栈顶 小
	PVOID VmxHostStackBase; //栈低 大

	PVOID MsrBitMap;  //
	PHYSICAL_ADDRESS MsrBitMapAddr; 

	PVMX_MAMAGER_PAGE_ENTRY vmxMamgerPage;
	VMX_EPTP vmxEptp;  //相当于CR3

}VMXCPUPCB,*PVMXCPUPCB;

#pragma pack(push,1)
typedef struct _GdtTable
{
	USHORT limit;
	ULONG64 Base;
}GdtTable,*PGdtTable;
#pragma pack(pop)

PVMXCPUPCB VmxGetCPUPCB(ULONG cpuNumber);

PVMXCPUPCB VmxGetCurrentCPUPCB();


void FullGdtDataItem(int index, short selector);

int VmxInit(ULONG64 hostEip);

void VmxDestory();