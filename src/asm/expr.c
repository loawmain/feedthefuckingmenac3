#include "expr.h"
#include "../vm/isa.h"
#include "../vm/mix.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum { V_CONST, V_VAR, V_TMP };
typedef struct { int kind; uint64_t k; int reg; } val;

typedef struct {
    const char* s;
    const expr_var* vars; int nvars;
    ftm_asm* a;
    int lo;
    uint16_t used;
    int err;
} P;

static int ralloc(P*p){
    for(int i=p->lo;i<8;i++) if(!(p->used&(1u<<i))){ p->used|=(uint16_t)(1u<<i); return i; }
    p->err=1; return p->lo;
}
static void rfree(P*p,int r){ if(r>=p->lo&&r<16) p->used&=(uint16_t)~(1u<<r); }

static val mkc(uint64_t k){ val v; v.kind=V_CONST; v.k=k; v.reg=-1; return v; }
static void ws(P*p){ while(*p->s==' '||*p->s=='\t') p->s++; }

static val to_tmp(P*p,val v){
    if(v.kind==V_TMP) return v;
    int r=ralloc(p);
    if(v.kind==V_CONST) a_ldi(p->a,r,v.k);
    else a_mov(p->a,r,v.reg);
    val t; t.kind=V_TMP; t.k=0; t.reg=r; return t;
}
static int rdof(P*p,val v,int*owned){
    if(v.kind==V_CONST){ int r=ralloc(p); a_ldi(p->a,r,v.k); *owned=1; return r; }
    *owned = (v.kind==V_TMP);
    return v.reg;
}

static uint64_t cfold(int op,uint64_t x,uint64_t y){
    switch(op){
        case OP_ADD:return x+y; case OP_SUB:return x-y; case OP_MUL:return x*y;
        case OP_XOR:return x^y; case OP_AND:return x&y; case OP_OR:return x|y;
    }
    return 0;
}

static val combine(P*p,int op,val A,val B){
    if(A.kind==V_CONST&&B.kind==V_CONST) return mkc(cfold(op,A.k,B.k));
    if(B.kind==V_CONST&&(op==OP_ADD||op==OP_XOR||op==OP_AND)){
        val t=to_tmp(p,A);
        int io = op==OP_ADD?OP_ADDI : op==OP_XOR?OP_XORI : OP_ANDI;
        a_imm(p->a,io,t.reg,B.k);
        return t;
    }
    val t=to_tmp(p,A);
    int owned; int rb=rdof(p,B,&owned);
    a_bin(p->a,op,t.reg,rb);
    if(owned) rfree(p,rb);
    return t;
}

static val p_expr(P*);

static val p_primary(P*p){
    ws(p);
    char c=*p->s;
    if(c=='('){ p->s++; val v=p_expr(p); ws(p); if(*p->s==')') p->s++; else p->err=1; return v; }
    if(isdigit((unsigned char)c)){
        char*end; uint64_t n=strtoull(p->s,&end,0); p->s=end; return mkc(n);
    }
    if(isalpha((unsigned char)c)||c=='_'){
        char nm[32]; int i=0;
        while((isalnum((unsigned char)*p->s)||*p->s=='_')&&i<31) nm[i++]=*p->s++;
        nm[i]=0;
        ws(p);
        if(*p->s=='('){
            p->s++;
            val a1=p_expr(p); ws(p); if(*p->s==',') p->s++; else p->err=1;
            val a2=p_expr(p); ws(p); if(*p->s==')') p->s++; else p->err=1;
            if(a2.kind!=V_CONST){ p->err=1; return mkc(0); }
            int isrol = !strcmp(nm,"rol");
            if(!isrol && strcmp(nm,"ror")){ p->err=1; return mkc(0); }
            if(a1.kind==V_CONST) return mkc(isrol?rol64(a1.k,(unsigned)a2.k):ror64(a1.k,(unsigned)a2.k));
            val t=to_tmp(p,a1);
            a_sh(p->a, isrol?OP_ROL:OP_ROR, t.reg,(int)(a2.k&63));
            return t;
        }
        for(int v=0;v<p->nvars;v++) if(!strcmp(p->vars[v].name,nm)){ val r; r.kind=V_VAR; r.k=0; r.reg=p->vars[v].reg; return r; }
        p->err=1; return mkc(0);
    }
    p->err=1; return mkc(0);
}

static val p_unary(P*p){
    ws(p);
    if(*p->s=='~'){ p->s++; val v=p_unary(p); if(v.kind==V_CONST) return mkc(~v.k); val t=to_tmp(p,v); a_un(p->a,OP_NOT,t.reg); return t; }
    if(*p->s=='-'){ p->s++; val v=p_unary(p); if(v.kind==V_CONST) return mkc((uint64_t)(-(int64_t)v.k)); val t=to_tmp(p,v); a_un(p->a,OP_NEG,t.reg); return t; }
    return p_primary(p);
}
static val p_mul(P*p){ val a=p_unary(p); for(;;){ ws(p); if(*p->s=='*'){p->s++; a=combine(p,OP_MUL,a,p_unary(p));} else break; } return a; }
static val p_add(P*p){ val a=p_mul(p); for(;;){ ws(p); char c=*p->s; if(c=='+'){p->s++; a=combine(p,OP_ADD,a,p_mul(p));} else if(c=='-'){p->s++; a=combine(p,OP_SUB,a,p_mul(p));} else break; } return a; }

static val p_shift(P*p){
    val a=p_add(p);
    for(;;){ ws(p);
        int sh = (p->s[0]=='<'&&p->s[1]=='<')?OP_SHL : (p->s[0]=='>'&&p->s[1]=='>')?OP_SHR : -1;
        if(sh<0) break;
        p->s+=2;
        val b=p_add(p);
        if(a.kind==V_CONST&&b.kind==V_CONST){ a=mkc(sh==OP_SHL? a.k<<(b.k&63) : a.k>>(b.k&63)); continue; }
        if(b.kind!=V_CONST){ p->err=1; return a; }
        val t=to_tmp(p,a); a_sh(p->a,sh,t.reg,(int)(b.k&63)); a=t;
    }
    return a;
}
static val p_band(P*p){ val a=p_shift(p); for(;;){ ws(p); if(*p->s=='&'){p->s++; a=combine(p,OP_AND,a,p_shift(p));} else break; } return a; }
static val p_bxor(P*p){ val a=p_band(p); for(;;){ ws(p); if(*p->s=='^'){p->s++; a=combine(p,OP_XOR,a,p_band(p));} else break; } return a; }
static val p_bor(P*p){ val a=p_bxor(p); for(;;){ ws(p); if(*p->s=='|'){p->s++; a=combine(p,OP_OR,a,p_bxor(p));} else break; } return a; }
static val p_expr(P*p){ return p_bor(p); }

int expr_compile(ftm_asm*a,const char*src,int dst,const expr_var*vars,int nvars,int tmp_lo){
    P p; p.s=src; p.vars=vars; p.nvars=nvars; p.a=a; p.lo=(tmp_lo<1||tmp_lo>7)?2:tmp_lo; p.used=0; p.err=0;
    val v=p_expr(&p);
    ws(&p);
    if(*p.s) p.err=1;
    if(p.err) return 1;
    if(v.kind==V_CONST) a_ldi(a,dst,v.k);
    else if(v.reg!=dst) a_mov(a,dst,v.reg);
    return 0;
}
