.code


AsmGetGdtTable proc
 sgdt [rcx];
 ret;
AsmGetGdtTable endp;


AsmReadES proc
	xor eax,eax;
	mov ax,es;
	ret;
AsmReadES endp
	
AsmReadCS proc
	xor eax,eax;
	mov ax,cs;
	ret;
AsmReadCS endp

AsmReadSS proc
	xor eax,eax;
	mov ax,ss;
	ret;
AsmReadSS endp

AsmReadDS proc
	xor eax,eax;
	mov ax,ds;
	ret;
AsmReadDS endp

AsmReadFS proc
	xor eax,eax;
	mov ax,fs;
	ret;
AsmReadFS endp

AsmReadGS proc
	xor eax,eax;
	mov ax,gs;
	ret;
AsmReadGS endp

AsmReadTR proc
	xor eax,eax;
	str ax;
	ret;

AsmReadTR endp

AsmReadLDTR proc
	xor eax,eax;
	sldt ax
	ret;
AsmReadLDTR endp

end
