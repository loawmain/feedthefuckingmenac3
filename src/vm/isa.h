#ifndef FTM_ISA_H
#define FTM_ISA_H
#include <stdint.h>

struct vm_ctx;

#define VM_REGS 16
#define VM_STACK 256
#define VM_MAGIC 0xF33DBEE5u

enum {
    OP_LDI=0, OP_MOV, OP_LD, OP_ST,
    OP_ADD, OP_SUB, OP_XOR, OP_AND, OP_OR, OP_MUL, OP_MULH,
    OP_ADDI, OP_XORI, OP_ANDI,
    OP_ROL, OP_ROR, OP_SHL, OP_SHR, OP_NOT, OP_NEG, OP_BSWAP,
    OP_PUSH, OP_POP, OP_CMP, OP_CMOV,
    OP_JMP, OP_JZ, OP_JNZ, OP_VMCALL, OP_CHK, OP_CHKX, OP_NOP, OP_HALT,
    OP__COUNT
};

#define VM_SLOTS (OP__COUNT+2)
#define FLAG_ZERO 1u

typedef uint64_t (*vm_thunk)(struct vm_ctx *c, uint64_t a, uint64_t b);

typedef struct vm_ctx {
    uint64_t r[VM_REGS];
    uint64_t stack[VM_STACK];
    int32_t sp;
    const uint8_t *code;
    uint32_t code_len, pc;
    uint64_t seed, mseed;
    uint32_t flags;
    const vm_thunk *thunks;
    uint32_t nthunks;
    uint8_t halted;
    int32_t status;
} vm_ctx;

void vm_seed(vm_ctx *c, uint64_t seed);
int vm_run(vm_ctx *c);

#endif
