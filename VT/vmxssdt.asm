.code

EXTERN MmUserProbeAddress:dq
EXTERN gSsdtHookBitMap:dq
EXTERN pgSsdtSeviceArray:dq
EXTERN gSsdtRet1:dq
EXTERN gSsdtRet2:dq
EXTERN KiSaveDebugRegisterState:dq
EXTERN KiUmsCallEntry:dq

KiSystemCall64Shadow proc ;syscall 
	
	swapgs
	mov     gs:[10h], rsp
	mov     rsp, gs:[1A8h]
	
	cmp    rax,0fffh;
	ja	   __ret1;

	push	rbx;
	mov		rbx,[gSsdtHookBitMap];
	cmp		byte ptr [rbx+rax],1
	pop		rbx;

	jne		__ret1;

	push    2Bh
	push    qword ptr gs:[10h]
	push    r11
	push    33h
	push    rcx
	mov     rcx, r10
	sub     rsp, 8
	push    rbp
	sub     rsp, 158h
	lea     rbp, [rsp+80h]
	mov     [rbp+0C0h], rbx
	mov     [rbp+0C8h], rdi
	mov     [rbp+0D0h], rsi
	mov     [rbp-50h], rax
	mov     [rbp-48h], rcx
	mov     [rbp-40h], rdx
	mov     rcx, gs:[188h]
	mov     rcx, [rcx+210h]
	mov     rcx, [rcx+4E8h]
	mov     gs:[2370h], rcx
	movzx   eax, byte ptr gs:[237Bh]
	cmp     gs:[237Ah], al
	jz      short __HANDLE1
	mov     gs:[237Ah], al
	mov     ecx, 48h
	xor     edx, edx
	wrmsr

__HANDLE1:
	  movzx   edx, byte ptr gs:[2378h]
	  test    edx, 8
	  jz      short __HANDLE2
	  mov     eax, 1
	  xor     edx, edx
	  mov     ecx, 49h
	  wrmsr
	  jmp     KiSystemServiceUser

__HANDLE2:
	  lfence
KiSystemServiceUser:
	  mov     byte ptr [rbp-55h], 2
	  mov     rbx, gs:[188h]
	  prefetchw byte ptr [rbx+1D8h]
	  stmxcsr dword ptr [rbp-54h]
	  ldmxcsr dword ptr gs:[180h]
	  cmp     byte ptr [rbx+3], 0
	  mov     word ptr [rbp+80h], 0
	  jz      __HANDLE7
	  test    byte ptr [rbx+3], 3
	  mov     [rbp-38h], r8
	  mov     [rbp-30h], r9
	  jz      short __HANDLE4
	  call    KiSaveDebugRegisterState

__HANDLE4:
	   test    byte ptr [rbx+3], 80h
		jz      short __HANDLE5
		mov     ecx, 0C0000102h
		rdmsr
		shl     rdx, 20h
		or      rax, rdx
		cmp     rax, [MmUserProbeAddress]
		cmovnb  rax, [MmUserProbeAddress]
		cmp     [rbx+0B8h], rax
		jz      short __HANDLE5
		cmp     [rbx+1B0h], rax
		jz      short __HANDLE5
		mov     rdx, [rbx+1B8h]
		bts     dword ptr [rbx+4Ch], 0Bh
		dec     word ptr [rbx+1C4h]
		mov     [rdx+80h], rax
		sti
		call    KiUmsCallEntry
		jmp     short __HANDLE6

__HANDLE5:
	 test    byte ptr [rbx+3], 40h
	 jz      short __HANDLE6
	 lock bts dword ptr [rbx+100h], 8
__HANDLE6:
	 mov     r8, [rbp-38h]
	 mov     r9, [rbp-30h]
__HANDLE7:
    mov     rax, [rbp-50h]
	mov     rcx, [rbp-48h]
	mov     rdx, [rbp-40h]
	db      66h, 66h, 66h, 66h
	nop     word ptr [rax+rax+00000000h]
	sti
	mov     [rbx+1E0h], rcx
	mov     [rbx+1F8h], eax

 KiSystemServiceStart:                   ; DATA XREF: KiServiceInternal+5Ao
		                         ; .data:00000001401DF838o
		 mov     [rbx+1D8h], rsp
		 mov     edi, eax
		 shr     edi, 7
		 and     edi, 20h
		 and     eax, 0FFFh
KiSystemServiceRepeat:                  ; CODE XREF: KiSystemCall64+68Fj
		mov     r10, qword ptr [pgSsdtSeviceArray]
		mov     r11, qword ptr [pgSsdtSeviceArray]
		jmp		gSsdtRet2

__ret1:
	    jmp		gSsdtRet1;
KiSystemCall64Shadow endp;

end