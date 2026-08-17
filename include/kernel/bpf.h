#ifndef _KERNEL_BPF_H
#define _KERNEL_BPF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BPF_MAX_INSNS 256
#define BPF_MAX_MAPS  16
#define BPF_MAX_PROGS 16
#define BPF_REG_CNT   11 // R0 (ret), R1-R5 (args), R6-R9 (callee-saved), R10 (frame pointer)

// BPF Instruction Class
#define BPF_CLASS_LD   0x00
#define BPF_CLASS_LDX  0x01
#define BPF_CLASS_ST   0x02
#define BPF_CLASS_STX  0x03
#define BPF_CLASS_ALU  0x04
#define BPF_CLASS_JMP  0x05
#define BPF_CLASS_ALU64 0x07

// BPF ALU Operations
#define BPF_OP_ADD  0x00
#define BPF_OP_SUB  0x10
#define BPF_OP_MUL  0x20
#define BPF_OP_DIV  0x30
#define BPF_OP_OR   0x40
#define BPF_OP_AND  0x50
#define BPF_OP_LSH  0x60
#define BPF_OP_RSH  0x70
#define BPF_OP_NEG  0x80
#define BPF_OP_MOD  0x90
#define BPF_OP_XOR  0xa0
#define BPF_OP_MOV  0xb0

// BPF Jump Operations
#define BPF_JMP_JA   0x00
#define BPF_JMP_JEQ  0x10
#define BPF_JMP_JGT  0x20
#define BPF_JMP_JGE  0x30
#define BPF_JMP_JNE  0x50
#define BPF_JMP_CALL 0x80
#define BPF_JMP_EXIT 0x90

typedef struct {
    uint8_t  opcode;
    uint8_t  dst_reg:4;
    uint8_t  src_reg:4;
    int16_t  offset;
    int32_t  imm;
} __attribute__((packed)) bpf_insn_t;

typedef enum {
    BPF_PROG_TYPE_UNSPEC = 0,
    BPF_PROG_TYPE_SOCKET_FILTER,
    BPF_PROG_TYPE_KPROBE,
    BPF_PROG_TYPE_TRACEPOINT,
    BPF_PROG_TYPE_XDP,
    BPF_PROG_TYPE_SYSCALL
} bpf_prog_type_t;

typedef struct {
    uint32_t id;
    char name[32];
    bpf_prog_type_t type;
    bpf_insn_t insns[BPF_MAX_INSNS];
    uint32_t insn_count;
    uint64_t run_count;
    uint64_t total_runtime_ns;
    bool in_use;
} bpf_prog_t;

typedef struct {
    uint32_t id;
    char name[32];
    uint32_t key_size;
    uint32_t value_size;
    uint32_t max_entries;
    uint8_t* data;
    bool in_use;
} bpf_map_t;

void bpf_init(void);
int  bpf_prog_load(bpf_prog_type_t type, const char* name, const bpf_insn_t* insns, uint32_t count);
int  bpf_prog_run(uint32_t prog_id, void* ctx, uint64_t* ret_val);
bool bpf_verify_prog(const bpf_insn_t* insns, uint32_t count);

int  bpf_map_create(const char* name, uint32_t key_size, uint32_t value_size, uint32_t max_entries);
int  bpf_map_update_elem(uint32_t map_id, const void* key, const void* value);
int  bpf_map_lookup_elem(uint32_t map_id, const void* key, void* value_out);

size_t bpf_get_prog_count(void);
const bpf_prog_t* bpf_get_prog(size_t index);

#endif // _KERNEL_BPF_H
