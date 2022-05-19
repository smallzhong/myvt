
#include "ssdt.h"
#include <intrin.h>
#include "SearchCode.h"


SSDTStruct * g_SSDT = NULL;
SSDTStruct * g_SSSDT = NULL;

SSDTStruct* SSDTfind()
{
	
	if (g_SSDT) return g_SSDT;
	static SSDTStruct* SSDT = 0;
	if (!SSDT)
	{
#ifndef _WIN64
		//x86 code
		UNICODE_STRING routineName;
		RtlInitUnicodeString(&routineName, L"KeServiceDescriptorTable");
		SSDT = (SSDTStruct*)MmGetSystemRoutineAddress(&routineName);
#else
		//x64 code
		ULONG_PTR kernelBase = NULL;
		ULONG kernelSize = NULL;
		GetNtModuleBaseAndSize(&kernelBase, &kernelSize);

		if (kernelBase == 0 || kernelSize == 0)
			return NULL;

		// Find KiSystemServiceStart
		const unsigned char KiSystemServiceStartPattern[] = { 0x8B, 0xF8, 0xC1, 0xEF, 0x07, 0x83, 0xE7, 0x20, 0x25, 0xFF, 0x0F, 0x00, 0x00 };
		const ULONG signatureSize = sizeof(KiSystemServiceStartPattern);
		BOOLEAN found = FALSE;
		ULONG KiSSSOffset;
		for (KiSSSOffset = 0; KiSSSOffset < kernelSize - signatureSize; KiSSSOffset++)
		{
			if (RtlCompareMemory(((unsigned char*)kernelBase + KiSSSOffset), KiSystemServiceStartPattern, signatureSize) == signatureSize)
			{
				found = TRUE;
				break;
			}
		}
		if (!found)
			return NULL;

		// lea r10, KeServiceDescriptorTable
		ULONG_PTR address = kernelBase + KiSSSOffset + signatureSize;
		LONG relativeOffset = 0;
		if ((*(unsigned char*)address == 0x4c) &&
			(*(unsigned char*)(address + 1) == 0x8d) &&
			(*(unsigned char*)(address + 2) == 0x15))
		{
			relativeOffset = *(LONG*)(address + 3);
		}
		if (relativeOffset == 0)
			return NULL;

		SSDT = (SSDTStruct*)(address + relativeOffset + 7);
#endif
	}

	g_SSDT = SSDT;
	return SSDT;
}

SSDTStruct* SSSDTfind()
{

	if (g_SSSDT) return g_SSSDT;

	if (!g_SSSDT)
	{
#ifndef _WIN64
		//x86 code
		UNICODE_STRING routineName;
		RtlInitUnicodeString(&routineName, L"KeServiceDescriptorTable");
		g_SSSDT = (SSDTStruct*)((ULONG)MmGetSystemRoutineAddress(&routineName) - 0x40);
#else
		//x64 code
		ULONG_PTR kernelBase = NULL;
		ULONG kernelSize = NULL;
		GetNtModuleBaseAndSize(&kernelBase, &kernelSize);

		if (kernelBase == 0 || kernelSize == 0)
			return NULL;

		// Find KiSystemServiceStart
		const unsigned char KiSystemServiceStartPattern[] = { 0x8B, 0xF8, 0xC1, 0xEF, 0x07, 0x83, 0xE7, 0x20, 0x25, 0xFF, 0x0F, 0x00, 0x00 };
		const ULONG signatureSize = sizeof(KiSystemServiceStartPattern);
		BOOLEAN found = FALSE;
		ULONG KiSSSOffset;
		for (KiSSSOffset = 0; KiSSSOffset < kernelSize - signatureSize; KiSSSOffset++)
		{
			if (RtlCompareMemory(((unsigned char*)kernelBase + KiSSSOffset), KiSystemServiceStartPattern, signatureSize) == signatureSize)
			{
				found = TRUE;
				break;
			}
		}
		if (!found)
			return NULL;

		ULONG_PTR address = kernelBase + KiSSSOffset + signatureSize;
		LONG relativeOffset = 0;
		if ((*(unsigned char*)address == 0x4c) &&
			(*(unsigned char*)(address + 1) == 0x8d) &&
			(*(unsigned char*)(address + 2) == 0x15))
		{
			relativeOffset = *(LONG*)(address + 10);
		}
		if (relativeOffset == 0)
			return NULL;

		g_SSSDT = (SSDTStruct*)((address + relativeOffset + 7 + 7) + sizeof(SSDTStruct));
#endif
	}

	return g_SSSDT;
}


ULONG64 GetSSSDTFunc(ULONG funcIndex)
{
	SSDTStruct* sssdt = SSSDTfind();
	if (sssdt != NULL) 
	{
		int offset1 = sssdt->pServiceTable[funcIndex];
		offset1 >>= 4;
		LARGE_INTEGER in = { 0 };
		in.QuadPart = (ULONG64)sssdt->pServiceTable;
		in.LowPart += offset1;
		ULONG64 myfunc = in.QuadPart;
		KdPrint(("find sssdt func %p index %x\r\n", myfunc,funcIndex));
		return myfunc;
	}
	
	KdPrint(("not find sssdt table \r\n"));
	return 0;
}

ULONG64 GetSSDTFunc(ULONG funcIndex)
{
	SSDTStruct* ssdt = SSDTfind();
	if (ssdt != NULL)
	{
		int offset1 = ssdt->pServiceTable[funcIndex];
		offset1 >>= 4;
		LARGE_INTEGER in = { 0 };
		in.QuadPart = (ULONG64)ssdt->pServiceTable;
		in.LowPart += offset1;
		ULONG64 myfunc = in.QuadPart;
		KdPrint(("find ssdt func %p index %x\r\n", myfunc, funcIndex));
		return myfunc;
	}

	KdPrint(("not find ssdt table \r\n"));
	return 0;
}



