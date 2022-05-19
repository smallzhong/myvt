#pragma once
#include <ntifs.h>

EXTERN_C VOID AsmGetGdtTable(PVOID tableBaseAddr);
EXTERN_C USHORT AsmReadES();
EXTERN_C USHORT AsmReadCS();
EXTERN_C USHORT AsmReadSS();
EXTERN_C USHORT AsmReadDS();
EXTERN_C USHORT AsmReadFS();
EXTERN_C USHORT AsmReadGS();
EXTERN_C USHORT AsmReadTR();
EXTERN_C USHORT AsmReadLDTR();
EXTERN_C VOID AsmInvd();
EXTERN_C VOID AsmVmCall(ULONG exitCode,ULONG64 kernelCr3,ULONG64 CodePfNumber,ULONG64 DataPfNumber,PULONG64 retValue);
EXTERN_C VOID AsmJmpRet(ULONG64 rip,ULONG64 rsp);

EXTERN_C void AsmVmxExitHandler();

EXTERN_C unsigned char  AsmInvvpid(unsigned long Type, void * Descriptors);

void Asminvept(ULONG type, ULONG64 eptp);