#include "../src/vm/isa.h"
#include "../src/vm/mix.h"
#include "../src/asm/asm.h"
#include "../src/asm/expr.h"
#include "../src/rt/guard.h"
#include "../src/rt/protect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t g_in, g_out;
static uint64_t th_in(vm_ctx*c,uint64_t a,uint64_t b){(void)c;(void)a;(void)b;return g_in;}
static uint64_t th_out(vm_ctx*c,uint64_t a,uint64_t b){(void)c;(void)b;g_out=a;return a;}
static uint64_t th_guard(vm_ctx*c,uint64_t a,uint64_t b){(void)c;(void)a;(void)b;return ftm_dbg_check();}

static uint64_t rnd64(void){
    uint64_t x=(uint64_t)time(NULL);
    x^=(uint64_t)(uintptr_t)&x*0x9E3779B97F4A7C15ull;
    x^=(uint64_t)clock()<<21;
    return sm64(x);
}
static uint64_t hx(const char*s){ return strtoull(s,NULL,0); }

static ftm_asm* build_fn(const char*expr,const expr_var*vars,int nvars,int dst){
    ftm_asm*a=asm_new();
    a_vmcall(a,0);
    if(expr_compile(a,expr,dst,vars,nvars,2)){ asm_free(a); return NULL; }
    a_mov(a,0,dst);
    a_vmcall(a,1);
    a_chk(a);
    a_halt(a);
    return a;
}

static int do_build(int argc,char**argv){
    const char*expr=NULL,*out="out.ftmb";
    expr_var vars[8]; int nvars=0;
    int dst=1, raw=0;
    int junk=4,opaque=5,traps=3,chk=2,scratch=8;
    uint64_t seed=0, key=0; int haveseed=0,havekey=0;

    for(int i=0;i<argc;i++){
        char*a=argv[i];
        if(a[0]!='-' && !expr){ expr=a; continue; }
        if(!strcmp(a,"--var")&&i+1<argc){ char*p=argv[++i]; char*eq=strchr(p,'='); if(eq&&nvars<8){ *eq=0; vars[nvars].name=p; vars[nvars].reg=atoi(eq+1); nvars++; } }
        else if(!strcmp(a,"--dst")&&i+1<argc) dst=atoi(argv[++i]);
        else if(!strcmp(a,"--out")&&i+1<argc) out=argv[++i];
        else if(!strcmp(a,"--seed")&&i+1<argc){ seed=hx(argv[++i]); haveseed=1; }
        else if(!strcmp(a,"--key")&&i+1<argc){ key=hx(argv[++i]); havekey=1; }
        else if(!strcmp(a,"--junk")&&i+1<argc) junk=atoi(argv[++i]);
        else if(!strcmp(a,"--opaque")&&i+1<argc) opaque=atoi(argv[++i]);
        else if(!strcmp(a,"--traps")&&i+1<argc) traps=atoi(argv[++i]);
        else if(!strcmp(a,"--chk")&&i+1<argc) chk=atoi(argv[++i]);
        else if(!strcmp(a,"--scratch")&&i+1<argc) scratch=atoi(argv[++i]);
        else if(!strcmp(a,"--raw")) raw=1;
    }
    if(!expr){ fprintf(stderr,"need an expression\n"); return 2; }
    if(!nvars){ vars[0].name="x"; vars[0].reg=0; nvars=1; }
    if(!haveseed) seed=rnd64();
    if(!havekey) key=rnd64();

    ftm_asm*fn=build_fn(expr,vars,nvars,dst);
    if(!fn){ fprintf(stderr,"expr compile failed\n"); return 3; }

    ftm_asm*use=fn;
    if(!raw){
        poly_cfg cfg={0};
        cfg.junk=junk; cfg.opaque=opaque; cfg.traps=traps; cfg.chk=chk;
        cfg.poison_reg=1; cfg.trap_thunk=2; cfg.scratch_lo=scratch;
        cfg.seed=sm64(seed^0xC0DEC0DEC0DEull);
        use=asm_polymorph(fn,&cfg);
    }

    ftm_blob blob=ftm_pack(use,seed,key);
    FILE*f=fopen(out,"wb");
    if(!f){ fprintf(stderr,"cant write %s\n",out); return 4; }
    fwrite(blob.data,1,blob.len,f);
    fclose(f);

    printf("wrote %s  (%u bytes)\n",out,blob.len);
    printf("key  = 0x%016llX\n",(unsigned long long)key);
    printf("run: ftmc run %s --key 0x%016llX --in <value>\n",out,(unsigned long long)key);

    ftm_blob_free(&blob);
    if(use!=fn) asm_free(use);
    asm_free(fn);
    return 0;
}

static int do_run(int argc,char**argv){
    const char*file=NULL; uint64_t key=0,in=0; int havekey=0;
    for(int i=0;i<argc;i++){
        char*a=argv[i];
        if(a[0]!='-'&&!file){ file=a; continue; }
        if(!strcmp(a,"--key")&&i+1<argc){ key=hx(argv[++i]); havekey=1; }
        else if(!strcmp(a,"--in")&&i+1<argc) in=hx(argv[++i]);
    }
    if(!file||!havekey){ fprintf(stderr,"need <file> --key HEX\n"); return 2; }

    FILE*f=fopen(file,"rb");
    if(!f){ fprintf(stderr,"cant open %s\n",file); return 4; }
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if(n<=0){ fclose(f); fprintf(stderr,"bad file\n"); return 4; }
    uint8_t*buf=(uint8_t*)malloc((size_t)n);
    if(fread(buf,1,(size_t)n,f)!=(size_t)n){ fclose(f); free(buf); return 4; }
    fclose(f);

    g_in=in; g_out=0;
    vm_thunk th[3]={th_in,th_out,th_guard};
    int st=ftm_exec(buf,(uint32_t)n,key,th,3);
    printf("result = 0x%016llX  (status %d)\n",(unsigned long long)g_out,st);
    free(buf);
    return 0;
}

int main(int argc,char**argv){
    if(argc>=2 && !strcmp(argv[1],"build")) return do_build(argc-2,argv+2);
    if(argc>=2 && !strcmp(argv[1],"run"))   return do_run(argc-2,argv+2);
    fprintf(stderr,"usage:\n  ftmc build \"<expr>\" [--var n=r --dst R --out F --seed HEX --key HEX --junk N --opaque N --traps N --chk N --scratch LO --raw]\n  ftmc run <file> --key HEX --in HEX\n");
    return 1;
}
