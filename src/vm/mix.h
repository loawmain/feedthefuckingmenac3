#ifndef FTM_MIX_H
#define FTM_MIX_H
#include <stdint.h>

static uint64_t rol64(uint64_t v,unsigned n){n&=63;return n?(v<<n)|(v>>(64-n)):v;}
static uint64_t ror64(uint64_t v,unsigned n){n&=63;return n?(v>>n)|(v<<(64-n)):v;}

static uint64_t sm64(uint64_t x){
    x+=0x9E3779B97F4A7C15ull;
    x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;
    x=(x^(x>>27))*0x94d049bb133111ebull;
    return x^(x>>31);
}
static uint64_t mulhi(uint64_t a,uint64_t b){
    uint64_t al=a&0xffffffffu,ah=a>>32,bl=b&0xffffffffu,bh=b>>32;
    uint64_t ll=al*bl,lh=al*bh,hl=ah*bl,hh=ah*bh;
    uint64_t cr=(ll>>32)+(lh&0xffffffffu)+(hl&0xffffffffu);
    return hh+(lh>>32)+(hl>>32)+(cr>>32);
}

static uint8_t ksb(uint64_t seed,uint32_t pos){return (uint8_t)(sm64(seed^((uint64_t)pos*0x9E3779B97F4A7C15ull))>>56);}
static uint8_t dselof(uint64_t seed,uint32_t opos){return (uint8_t)(sm64((seed^0xD1B54A32D192ED03ull)^((uint64_t)opos*0x2545F4914F6CDD1Dull))&0x3F);}
static uint64_t maskof(uint64_t mseed,uint8_t i){uint64_t m=mseed^((uint64_t)i*0x9E3779B97F4A7C15ull);m^=m>>33;m*=0xff51afd7ed558ccdull;m^=m>>33;return m;}

static uint64_t hashcode(const uint8_t*b,uint32_t n){
    uint64_t h=0x9E3779B97F4A7C15ull;
    for(uint32_t i=0;i<n;i++) h=rol64(h^b[i],11)*6364136223846793005ull+1442695040888963407ull;
    return h;
}

#endif
