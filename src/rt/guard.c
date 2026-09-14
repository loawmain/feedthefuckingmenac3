#include "guard.h"
#include <windows.h>
#include <intrin.h>

typedef LONG(WINAPI*qip_t)(HANDLE,ULONG,PVOID,ULONG,PULONG);

uint64_t ftm_dbg_check(void){
    uint64_t v=0;
#if defined(_M_X64)
    uintptr_t peb=__readgsqword(0x60);
    if(*(volatile unsigned char*)(peb+2)) v|=1;
    if(*(volatile unsigned long*)(peb+0xbc)&0x70) v|=2;
#endif
    HMODULE nt=GetModuleHandleA("ntdll.dll");
    qip_t q = nt ? (qip_t)GetProcAddress(nt,"NtQueryInformationProcess") : 0;
    if(q){
        HANDLE self=(HANDLE)(LONG_PTR)-1;
        uintptr_t port=0;
        if(q(self,7,&port,(ULONG)sizeof port,0)==0 && port) v|=4;
        ULONG df=1;
        if(q(self,0x1f,&df,(ULONG)sizeof df,0)==0 && df==0) v|=8;
    }
    return v ? v*0x9E3779B97F4A7C15ull : 0;
}
