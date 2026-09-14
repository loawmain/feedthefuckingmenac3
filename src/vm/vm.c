#include "isa.h"
#include "mix.h"
#include <string.h>

#define STKX 0xA5A5A5A5A5A5A5A5ull

void vm_seed(vm_ctx*c,uint64_t seed){
    c->seed=seed;
    c->mseed=sm64(seed^0xF33DBEE5ull);
    for(int i=0;i<VM_REGS;i++) c->r[i]=maskof(c->mseed,(uint8_t)i);
    c->sp=0;c->pc=0;c->flags=0;c->halted=0;c->status=0;
}

static uint8_t b8(vm_ctx*c){uint8_t k=ksb(c->seed,c->pc);uint8_t v=(uint8_t)(c->code[c->pc]^k);c->pc++;return v;}
static uint32_t b32(vm_ctx*c){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)b8(c)<<(8*i);return v;}
static uint64_t b64(vm_ctx*c){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)b8(c)<<(8*i);return v;}
static uint8_t rgi(vm_ctx*c){return b8(c)&(VM_REGS-1);}
static uint64_t rget(vm_ctx*c,uint8_t i){return c->r[i]^maskof(c->mseed,i);}
static void rset(vm_ctx*c,uint8_t i,uint64_t v){c->r[i]=v^maskof(c->mseed,i);}
static uint8_t dop(vm_ctx*c){uint32_t o=c->pc;uint8_t raw=b8(c);return (uint8_t)((raw-dselof(c->seed,o))&0x3F);}

static void h_ldi(vm_ctx*c){uint8_t d=rgi(c);rset(c,d,b64(c));}
static void h_mov(vm_ctx*c){uint8_t d=rgi(c),s=rgi(c);rset(c,d,rget(c,s));}
static void h_ld(vm_ctx*c){uint8_t d=rgi(c),s=rgi(c);int32_t o=(int32_t)b32(c);uint64_t v;memcpy(&v,(const uint8_t*)(uintptr_t)rget(c,s)+o,8);rset(c,d,v);}
static void h_st(vm_ctx*c){uint8_t d=rgi(c);int32_t o=(int32_t)b32(c);uint8_t s=rgi(c);uint64_t v=rget(c,s);memcpy((uint8_t*)(uintptr_t)rget(c,d)+o,&v,8);}
static void h_add(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,(x^y)+((x&y)<<1));}
static void h_sub(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,x+~y+1);}
static void h_xor(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,(x|y)&~(x&y));}
static void h_and(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,(x|y)-(x^y));}
static void h_or(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,(x&y)+(x^y));}
static void h_mul(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);rset(c,a,rget(c,a)*rget(c,b));}
static void h_mulh(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);rset(c,a,mulhi(rget(c,a),rget(c,b)));}
static void h_addi(vm_ctx*c){uint8_t d=rgi(c);uint64_t im=b64(c),x=rget(c,d);rset(c,d,(x^im)+((x&im)<<1));}
static void h_xori(vm_ctx*c){uint8_t d=rgi(c);uint64_t im=b64(c),x=rget(c,d);rset(c,d,(x|im)&~(x&im));}
static void h_andi(vm_ctx*c){uint8_t d=rgi(c);uint64_t im=b64(c),x=rget(c,d);rset(c,d,(x|im)-(x^im));}
static void h_rol(vm_ctx*c){uint8_t d=rgi(c),n=b8(c);rset(c,d,rol64(rget(c,d),n));}
static void h_ror(vm_ctx*c){uint8_t d=rgi(c),n=b8(c);rset(c,d,ror64(rget(c,d),n));}
static void h_shl(vm_ctx*c){uint8_t d=rgi(c),n=(uint8_t)(b8(c)&63);rset(c,d,rget(c,d)<<n);}
static void h_shr(vm_ctx*c){uint8_t d=rgi(c),n=(uint8_t)(b8(c)&63);rset(c,d,rget(c,d)>>n);}
static void h_not(vm_ctx*c){uint8_t d=rgi(c);rset(c,d,~rget(c,d));}
static void h_neg(vm_ctx*c){uint8_t d=rgi(c);rset(c,d,(uint64_t)(-(int64_t)rget(c,d)));}
static void h_bsw(vm_ctx*c){uint8_t d=rgi(c);uint64_t v=rget(c,d),o=0;for(int i=0;i<8;i++){o=(o<<8)|(v&0xff);v>>=8;}rset(c,d,o);}
static void h_push(vm_ctx*c){uint8_t s=rgi(c);if(c->sp<VM_STACK)c->stack[c->sp++]=rget(c,s)^STKX;}
static void h_pop(vm_ctx*c){uint8_t d=rgi(c);if(c->sp>0)rset(c,d,c->stack[--c->sp]^STKX);}
static void h_cmp(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);c->flags=(rget(c,a)==rget(c,b))?FLAG_ZERO:0;}
static void h_cmov(vm_ctx*c){uint8_t d=rgi(c),s=rgi(c);if(c->flags&FLAG_ZERO)rset(c,d,rget(c,s));}
static void h_jmp(vm_ctx*c){int32_t r=(int32_t)b32(c);c->pc+=r;}
static void h_jz(vm_ctx*c){int32_t r=(int32_t)b32(c);if(c->flags&FLAG_ZERO)c->pc+=r;}
static void h_jnz(vm_ctx*c){int32_t r=(int32_t)b32(c);if(!(c->flags&FLAG_ZERO))c->pc+=r;}
static void h_vmc(vm_ctx*c){uint8_t id=b8(c);if(id<c->nthunks&&c->thunks[id])rset(c,0,c->thunks[id](c,rget(c,0),rget(c,1)));}
static void h_chk(vm_ctx*c){uint64_t h=hashcode(c->code,c->pc);uint64_t e=b64(c);if(h!=e){c->status=(int32_t)0xBADC0DE;c->halted=1;}}
static void h_chkx(vm_ctx*c){uint8_t d=rgi(c);uint64_t h=hashcode(c->code,c->pc);uint64_t e=b64(c);rset(c,d,rget(c,d)^(h^e));}
static void h_nop(vm_ctx*c){(void)c;}
static void h_halt(vm_ctx*c){c->status=(int32_t)rget(c,0);c->halted=1;}
static void h_dc0(vm_ctx*c){uint8_t d=rgi(c);rset(c,d,rget(c,d)^0xdeadbeefu);}
static void h_dc1(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);rset(c,a,rget(c,a)+rget(c,b)*3);}

static void h_add_b(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,(x|y)+(x&y));}
static void h_sub_b(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,(x^y)-(((~x)&y)<<1));}
static void h_xor_b(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,(x|y)-(x&y));}
static void h_and_b(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,x+y-(x|y));}
static void h_or_b(vm_ctx*c){uint8_t a=rgi(c),b=rgi(c);uint64_t x=rget(c,a),y=rget(c,b);rset(c,a,(x+y)-(x&y));}
static void h_addi_b(vm_ctx*c){uint8_t d=rgi(c);uint64_t im=b64(c),x=rget(c,d);rset(c,d,(x|im)+(x&im));}
static void h_xori_b(vm_ctx*c){uint8_t d=rgi(c);uint64_t im=b64(c),x=rget(c,d);rset(c,d,(x|im)-(x&im));}
static void h_andi_b(vm_ctx*c){uint8_t d=rgi(c);uint64_t im=b64(c),x=rget(c,d);rset(c,d,x+im-(x|im));}

static uint32_t gcd32(uint32_t a,uint32_t b){while(b){uint32_t t=a%b;a=b;b=t;}return a;}

int vm_run(vm_ctx*c){
    uint64_t hk=sm64(c->seed^0x5DEECE66Dull);
    uint64_t vs=sm64(c->seed^0x243F6A8885A308D3ull);
    uint32_t S=VM_SLOTS;
    uint32_t Am=(uint32_t)(sm64(c->seed^0xA1B2C3D4E5F60718ull)%S); if(!Am) Am=1;
    while(gcd32(Am,S)!=1){ if(++Am>=S) Am=1; }
    uint32_t Bm=(uint32_t)(sm64(c->seed^0x0F1E2D3C4B5A6978ull)%S);
    #define SLOT(op) (((uint32_t)(op)*Am+Bm)%S)
    #define SET(op,fn) H[SLOT(op)]=(uintptr_t)(fn)^hk

    uintptr_t H[VM_SLOTS];
    SET(OP_LDI,&h_ldi); SET(OP_MOV,&h_mov); SET(OP_LD,&h_ld); SET(OP_ST,&h_st);
    SET(OP_ADD, (vs&1)?&h_add:&h_add_b);
    SET(OP_SUB, (vs&2)?&h_sub:&h_sub_b);
    SET(OP_XOR, (vs&4)?&h_xor:&h_xor_b);
    SET(OP_AND, (vs&8)?&h_and:&h_and_b);
    SET(OP_OR,  (vs&16)?&h_or:&h_or_b);
    SET(OP_MUL,&h_mul); SET(OP_MULH,&h_mulh);
    SET(OP_ADDI,(vs&32)?&h_addi:&h_addi_b);
    SET(OP_XORI,(vs&64)?&h_xori:&h_xori_b);
    SET(OP_ANDI,(vs&128)?&h_andi:&h_andi_b);
    SET(OP_ROL,&h_rol); SET(OP_ROR,&h_ror); SET(OP_SHL,&h_shl); SET(OP_SHR,&h_shr);
    SET(OP_NOT,&h_not); SET(OP_NEG,&h_neg); SET(OP_BSWAP,&h_bsw);
    SET(OP_PUSH,&h_push); SET(OP_POP,&h_pop); SET(OP_CMP,&h_cmp); SET(OP_CMOV,&h_cmov);
    SET(OP_JMP,&h_jmp); SET(OP_JZ,&h_jz); SET(OP_JNZ,&h_jnz); SET(OP_VMCALL,&h_vmc);
    SET(OP_CHK,&h_chk); SET(OP_CHKX,&h_chkx); SET(OP_NOP,&h_nop); SET(OP_HALT,&h_halt);
    SET(OP__COUNT,&h_dc0); SET(OP__COUNT+1,&h_dc1);

    uint32_t budget=6000000u;
    while(!c->halted && c->pc<c->code_len && budget--){
        uint8_t op=dop(c);
        if(op<VM_SLOTS){void(*fn)(vm_ctx*)=(void(*)(vm_ctx*))(H[SLOT(op)]^hk);fn(c);}
        else{c->status=(int32_t)0xDEAD;c->halted=1;}
    }
    #undef SLOT
    #undef SET
    return c->status;
}
