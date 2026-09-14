#ifndef FTM_PROTECT_H
#define FTM_PROTECT_H
#include <stdint.h>
#include "../vm/isa.h"
#include "../asm/asm.h"

typedef struct { uint8_t* data; uint32_t len; } ftm_blob;

ftm_blob ftm_pack(ftm_asm* a, uint64_t code_seed, uint64_t pack_key);
void ftm_blob_free(ftm_blob* b);
int ftm_exec(const uint8_t* blob, uint32_t len, uint64_t pack_key, const vm_thunk* th, uint32_t nth);

#endif
