#include "protect.h"
#include "guard.h"
#include "../vm/mix.h"
#include <stdlib.h>
#include <string.h>

#define HDR 16

static uint64_t skey(uint64_t pk,uint32_t clen){ return sm64(pk^(uint64_t)VM_MAGIC^(uint64_t)clen); }

ftm_blob ftm_pack(ftm_asm*a,uint64_t code_seed,uint64_t pack_key){
    ftm_blob r={0,0};
    uint32_t clen=0;
    uint8_t*code=asm_build(a,code_seed,&clen);
    uint32_t total=HDR+clen;
    uint8_t*b=(uint8_t*)malloc(total);
    uint64_t h=hashcode(code,clen);
    uint64_t obf=code_seed^skey(pack_key,clen)^h;
    uint32_t mg=VM_MAGIC;
    memcpy(b,&mg,4);
    memcpy(b+4,&obf,8);
    memcpy(b+12,&clen,4);
    memcpy(b+HDR,code,clen);
    free(code);
    r.data=b; r.len=total;
    return r;
}

void ftm_blob_free(ftm_blob*b){ if(b&&b->data){ free(b->data); b->data=0; b->len=0; } }

int ftm_exec(const uint8_t*blob,uint32_t len,uint64_t pack_key,const vm_thunk*th,uint32_t nth){
    if(len<HDR) return (int)0xDEAD;
    uint32_t mg; memcpy(&mg,blob,4);
    if(mg!=VM_MAGIC) return (int)0xDEAD;
    uint64_t obf; memcpy(&obf,blob+4,8);
    uint32_t clen; memcpy(&clen,blob+12,4);
    if((uint64_t)HDR+clen!=len) return (int)0xDEAD;
    const uint8_t*code=blob+HDR;
    uint64_t h=hashcode(code,clen);
    uint64_t seed=obf^skey(pack_key,clen)^h;
    seed^=ftm_dbg_check();
    vm_ctx c; vm_seed(&c,seed);
    c.code=code; c.code_len=clen; c.thunks=th; c.nthunks=nth;
    return vm_run(&c);
}
