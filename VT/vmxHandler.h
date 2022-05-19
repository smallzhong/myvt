#pragma once
#include <ntifs.h>

typedef struct _GuestContext 
{
	ULONG64 mRax;
	ULONG64 mRcx;
	ULONG64 mRdx;
	ULONG64 mRbx;
	ULONG64 mRsp;
	ULONG64 mRbp;
	ULONG64 mRsi;
	ULONG64 mRdi;
	ULONG64 mR8;
	ULONG64 mR9;
	ULONG64 mR10;
	ULONG64 mR11;
	ULONG64 mR12;
	ULONG64 mR13;
	ULONG64 mR14;
	ULONG64 mR15;
}GuestContext,*PGuestContext;

typedef struct _INVPCID
{
	ULONG64 scale : 2;
	ULONG64 und : 5;
	ULONG64 addrssSize : 3;
	ULONG64 rev1 : 1;
	ULONG64 und2 : 4;
	ULONG64 segement : 3;
	ULONG64 index : 4;
	ULONG64 indexInvaild : 1;
	ULONG64 base : 4;
	ULONG64 baseInvaild : 1;
	ULONG64 regOpt : 4;
	ULONG64 un3 : 32;
}INVPCID,*PINVPCID;


typedef struct _VMXExitIntEvent
{
	ULONG64 vector : 8;
	ULONG64 type : 3;
	ULONG64 errorCode : 1;
	ULONG64 NMIUNBlocking : 1;
	ULONG64 reun : 18;
	ULONG64 vaild : 1;
	ULONG64 reun2 : 32;
}VMXExitIntEvent, *PVMXExitIntEvent;


#define EXCEPTION_W_INT 0
#define EXCEPTION_NMI_INT 2
#define EXCEPTION_HARDWARE 3
#define EXCEPTION_SOFT 6


EXTERN_C VOID VmxExitHandler(PGuestContext context);