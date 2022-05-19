#include "vmxHandler.h"
#include <intrin.h>
#include "VMXDefine.h"
#include "vmxs.h"
#include "VMXTools.h"
#include "VmxSsdtHook.h"
#include "VmxEpt.h"

#define MAKE_REG(XXX1,XXX2) ((XXX1 & 0xFFFFFFFF) | (XXX2<<32))

VOID VmxHandlerCpuid(PGuestContext context)
{
	if (context->mRax == 0x8888)
	{
		context->mRax = 0x11111111;
		context->mRbx = 0x22222222;
		context->mRcx = 0x33333333;
		context->mRdx = 0x44444444;
		
		VmxEanbleMTF(TRUE);

		/*
		//注入指令前事件
		__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, 0);
		VMXExitIntEvent VmEvent = { 0 };
		VmEvent.vaild = 1;
		VmEvent.type = 7;
		VmEvent.vector = 0;
		__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, *(PULONG64)&VmEvent);
		__vmx_vmwrite(VM_ENTRY_INSTRUCTION_LEN, 0);
		*/
	}
	else 
	{
		int cpuids[4] = {0};
		__cpuidex(cpuids,context->mRax, context->mRcx);
		context->mRax = cpuids[0];
		context->mRbx = cpuids[1];
		context->mRcx = cpuids[2];
		context->mRdx = cpuids[3];
	}
}

VOID VmxHandlerReadMsr(PGuestContext context)
{

	if (context->mRcx == 0xC0000082 && VmxGetOldSysCallEntry())
	{
		DbgPrintEx(77, 0, "[db]:read %x\r\n", context->mRcx);
		ULONG64 value = VmxGetOldSysCallEntry();
		context->mRax = value & 0xFFFFFFFF;
		context->mRdx = (value >> 32) & 0xFFFFFFFF;
	}
	else 
	{
		DbgPrintEx(77, 0, "[db]:read---------------------:%x\r\n", context->mRcx);
		ULONG64 value = __readmsr(context->mRcx);
		context->mRax = value & 0xFFFFFFFF;
		context->mRdx = (value >> 32) & 0xFFFFFFFF;
	}

	
}


VOID VmxHandlerWriteMsr(PGuestContext context)
{

	if (context->mRcx == 0xc0000082 && VmxGetOldSysCallEntry())
	{
		DbgPrintEx(77, 0, "[db]:write:%x\r\n", context->mRcx);
		
		//syscall 指令 msr c00000082
	}
	else 
	{
		DbgPrintEx(77, 0, "[db]:write---------------------:%x\r\n", context->mRcx);
		ULONG64 value = MAKE_REG(context->mRax, context->mRdx);
		__writemsr(context->mRcx, value);
	}

	
	
}


VOID VmxExitInvpcidHandler(PGuestContext context)
{
	ULONG64 mrsp = 0;
	ULONG64 instinfo = 0;
	ULONG64 qualification = 0;
	__vmx_vmread(VMX_INSTRUCTION_INFO, &instinfo); //指令详细信息
	__vmx_vmread(EXIT_QUALIFICATION, &qualification); //偏移量
	__vmx_vmread(GUEST_RSP, &mrsp);

	PINVPCID pinfo = (PINVPCID)&instinfo;

	ULONG64 base = 0;
	ULONG64 index = 0;
	ULONG64 scale = pinfo->scale ? 2 ^ pinfo->scale : 0;
	ULONG64 addr = 0;
	ULONG64 regopt = ((PULONG64)context)[pinfo->regOpt];;

	if (!pinfo->baseInvaild)
	{
		if (pinfo->base == 4)
		{
			base = mrsp;
		}
		else 
		{
			base = ((PULONG64)context)[pinfo->base];
		}
		
	}

	if (!pinfo->indexInvaild)
	{
		if (pinfo->index == 4)
		{
			index = mrsp;
		}
		else
		{
			index = ((PULONG64)context)[pinfo->index];
		}

	}

	if (pinfo->addrssSize == 0)
	{
		addr = *(PSHORT)(base + index * scale + qualification);
	}
	else if (pinfo->addrssSize == 1)
	{
		addr = *(PULONG)(base + index * scale + qualification);
	}
	else 
	{
		addr = *(PULONG64)(base + index * scale + qualification);
	}

	_invpcid(regopt, &addr);
}

VOID VmxExceptionHandler(PGuestContext context)
{
	//获取中断信息
	VMXExitIntEvent VmEvent = { 0 };
	ULONG64 mrip = 0;
	ULONG64 instLen = 0;
	ULONG64 mrsp = 0;
	ULONG64 errorcode = 0;
	
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &instLen); // 获取指令长度
	__vmx_vmread(GUEST_RIP, &mrip); //获取客户机触发VT事件的地址
	__vmx_vmread(GUEST_RSP, &mrsp);
	__vmx_vmread(VM_EXIT_INTR_INFO, &VmEvent); //中断详情
	__vmx_vmread(VM_EXIT_INTR_ERROR_CODE, &errorcode); //指令错误


	if (!VmEvent.vaild)
	{
		__vmx_vmwrite(GUEST_RIP, mrip + instLen);
		__vmx_vmwrite(GUEST_RSP, mrsp);
		return;
	}

	if (VmEvent.errorCode)
	{
		__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, errorcode);
	}

	switch (VmEvent.type)
	{
		case EXCEPTION_W_INT:
			break;
		case EXCEPTION_NMI_INT:
			break;
		case EXCEPTION_HARDWARE:
			break;
		
		case EXCEPTION_SOFT:
		{
			if (VmEvent.vector == 3)
			{
				DbgPrintEx(77, 0, "[db]:interput int 3\r\n");
				
				//如果是int 3
				__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, *(PULONG64)&VmEvent);
				__vmx_vmwrite(VM_ENTRY_INSTRUCTION_LEN, instLen);
				instLen = 0;
			}
		}
			break;
	}


	__vmx_vmwrite(GUEST_RIP, mrip + instLen);
	__vmx_vmwrite(GUEST_RSP, mrsp);

}

VOID VmxExitMTF(PGuestContext context)
{
	

	VmxEanbleMTF(FALSE);

}

VOID InvvpidSingleContext(UINT16 Vpid)
{
	typedef struct _INVVPID_DESCRIPTOR
	{
		UINT64 VPID : 16;
		UINT64 RESERVED : 48;
		UINT64 LINEAR_ADDRESS;

	} INVVPID_DESCRIPTOR, *PINVVPID_DESCRIPTOR;

	INVVPID_DESCRIPTOR Descriptor = { Vpid, 0, 0 };
	AsmInvvpid(1, &Descriptor);

}

VOID VmxExitCtrlReg(PGuestContext context)
{
	struct 
	{
		ULONG64 crn : 4;
		ULONG64 accessType : 2;
		ULONG64 LMSWOp : 1;
		ULONG64 rv1 : 1;
		ULONG64 gpr : 4;
		ULONG64 rv2 : 4;
		ULONG64 LMSWSrc : 16;
		ULONG64 rv3 : 32;
	}crinfo;
	
	ULONG64 mrsp = 0;
	ULONG64 mrip = 0;
	ULONG64 instLen = 0;
	__vmx_vmread(EXIT_QUALIFICATION, (PULONG64)&crinfo); //偏移量
	__vmx_vmread(GUEST_RSP, &mrsp);
	__vmx_vmread(GUEST_RIP, &mrip);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &instLen); // 获取指令长度



	ULONG64 vcr3 = 0;
	__vmx_vmread(GUEST_CR3, &vcr3);

	

	DbgPrintEx(77, 0, "[db]:vcr3 = %llx,Type = %d,gpr = %d,crn = %d,number = %d\r\n", vcr3, crinfo.accessType, crinfo.gpr, crinfo.crn,KeGetCurrentProcessorNumberEx(NULL));

	if (crinfo.accessType == 0)
	{
		if (crinfo.crn == 3)
		{
			
			ULONG64 vxcr3 = 0;
			//mov cr3,rax
			if (crinfo.gpr == 4)
			{
				vxcr3 = mrsp;
			}
			else
			{
				vxcr3 = ((PULONG64)context)[crinfo.gpr];
			}

			vxcr3 = (vxcr3 & ~(1ULL << 63));

			

			DbgPrintEx(77, 0, "[db]:write cr3 %llx\r\n", vxcr3);
			__vmx_vmwrite(GUEST_CR3, vxcr3);

			//InvvpidSingleContext(1);
		}
		
	}
	else if (crinfo.accessType == 1)
	{
		//mov rax,cr3

		if (crinfo.crn == 3)
		{
			DbgPrintEx(77, 0, "[db]:read cr3 \r\n");
			if (crinfo.gpr == 4)
			{
				mrsp = vcr3;
			}
			else
			{
				((PULONG64)context)[crinfo.gpr] = vcr3;
			}
		}
		

		
	}

	__vmx_vmwrite(GUEST_RIP, mrip + instLen);
	__vmx_vmwrite(GUEST_RSP, mrsp);
}

EXTERN_C VOID VmxExitHandler(PGuestContext context)
{
	ULONG64 reason = 0;
	ULONG64 instLen = 0;
	ULONG64 instinfo = 0;
	ULONG64 mrip = 0;
	ULONG64 mrsp = 0;
	
	__vmx_vmread(VM_EXIT_REASON, &reason);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &instLen); // 获取指令长度
	__vmx_vmread(VMX_INSTRUCTION_INFO, &instinfo); //指令详细信息
	__vmx_vmread(GUEST_RIP, &mrip); //获取客户机触发VT事件的地址
	__vmx_vmread(GUEST_RSP, &mrsp); 

	//获取事件码
	reason = reason & 0xFFFF;

	if (reason != 0x1f && reason != 0x20 && reason != 0xa && reason != 0x1c)
	{
		DbgPrintEx(77, 0, "[db]:reason = %x\r\n", reason);
	}
	

	switch (reason)
	{
		case EXIT_REASON_EXCEPTION_NMI:
			VmxExceptionHandler(context);
			return;

		case EXIT_REASON_CPUID:
			VmxHandlerCpuid(context);
			break;

		case EXIT_REASON_GETSEC:
		{
			DbgBreakPoint();
			DbgPrintEx(77, 0, "[db]:GETSEC reason = %x rip = %llx\r\n", reason, mrip);
		}
			break;

		case EXIT_REASON_TRIPLE_FAULT:
		{
			DbgBreakPoint();
			DbgPrintEx(77, 0, "[db]:GETSEC reason = %x rip = %llx\r\n", reason, mrip);
		}
			break;

		case EXIT_REASON_INVD:
		{
			AsmInvd();
		}
			break;

		case EXIT_REASON_VMCALL:
		{
			if (context->mRax == 'babq')
			{
				__vmx_off();
				AsmJmpRet(mrip + instLen, mrsp);
				return;
			}
			else if(context->mRax == __EPT_PAGE_HOOK)
			{
				EptHookVmCall(context->mRcx, context->mRdx, context->mR8, context->mR9);
			}
			else 
			{
				ULONG64 rfl = 0;
				__vmx_vmread(GUEST_RFLAGS, &rfl);
				rfl |= 0x41;
				__vmx_vmwrite(GUEST_RFLAGS, &rfl);
			}
		}
	
			break;
		case EXIT_REASON_VMCLEAR		:
		case EXIT_REASON_VMLAUNCH		:
		case EXIT_REASON_VMPTRLD		:
		case EXIT_REASON_VMPTRST		:
		case EXIT_REASON_VMREAD			:
		case EXIT_REASON_VMRESUME		:
		case EXIT_REASON_VMWRITE		:
		case EXIT_REASON_VMXOFF			:
		case EXIT_REASON_VMXON			:
		{
			ULONG64 rfl = 0;
			__vmx_vmread(GUEST_RFLAGS, &rfl);
			rfl |= 0x41;
			__vmx_vmwrite(GUEST_RFLAGS, &rfl);
		}
		break;

		case EXIT_REASON_MSR_READ:
		{
			VmxHandlerReadMsr(context);
		}
			break;

		case EXIT_REASON_MSR_WRITE:
		{
			VmxHandlerWriteMsr(context);
		}
			break;

		case EXIT_REASON_RDTSCP:
		{
			int aunx = 0;
			LARGE_INTEGER in = {0};
			in.QuadPart =  __rdtscp(&aunx);
			context->mRax = in.LowPart;
			context->mRdx = in.HighPart;
			context->mRcx = aunx;
		}
		break;

		case EXIT_REASON_XSETBV:
		{
			ULONG64 value = MAKE_REG(context->mRax, context->mRdx);
			_xsetbv(context->mRcx, value);
		}
		break;

		case EXIT_REASON_INVPCID:
		{
			VmxExitInvpcidHandler(context);
		}
		break;

		case EXIT_REASON_MTF:
		{
			VmxExitMTF(context);
			return;
		}
		break;

		case EXIT_REASON_CR_ACCESS:
		{
			VmxExitCtrlReg(context);
			return;
		}
		break;

		case EXIT_REASON_EPT_VIOLATION:
		{
			VmxEptHandler(context);
		}
		break;
		
		case EXIT_REASON_EPT_CONFIG:
			DbgBreakPoint();
			DbgPrintEx(77, 0, "[db]:GETSEC reason = %x rip = %llx\r\n", reason, mrip);
			break;
	}


	__vmx_vmwrite(GUEST_RIP, mrip + instLen);
	__vmx_vmwrite(GUEST_RSP, mrsp);
}