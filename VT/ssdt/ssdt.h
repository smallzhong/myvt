#ifndef __SSDT_HOOK_H_
#define __SSDT_HOOK_H_

#include <ntifs.h>



typedef struct _SSDTStruct
{
	ULONG* pServiceTable;
	PVOID pCounterTable;
#ifdef _WIN64
	ULONGLONG NumberOfServices;
#else
	ULONG NumberOfServices;
#endif
	PCHAR pArgumentTable;
}SSDTStruct;


SSDTStruct* SSDTfind();
SSDTStruct* SSSDTfind();

ULONG64 GetSSSDTFunc(ULONG funcIndex);
ULONG64 GetSSDTFunc(ULONG funcIndex);

#endif