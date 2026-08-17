#include <kernel/bpf.h>
#include <kernel/printk.h>
#include <arch/x86_64/pit.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

static bpf_prog_t bpf_programs[BPF_MAX_PROGS];
static bpf_map_t  bpf_maps[BPF_MAX_MAPS];
static uint32_t   next_bpf_id = 1;

void bpf_init(void) {
    memset(bpf_programs, 0, sizeof(bpf_programs));
    memset(bpf_maps, 0, sizeof(bpf_maps));
    next_bpf_id = 1;

    // Pre-load a default network packet counter BPF program
    bpf_insn_t default_insns[] = {
        {BPF_CLASS_ALU64 | BPF_OP_MOV, 0, 1, 0, 0},        // r0 = r1 (packet len)
        {BPF_CLASS_ALU64 | BPF_OP_ADD, 0, 0, 0, 14},       // r0 += 14 (eth header)
        {BPF_CLASS_JMP   | BPF_JMP_EXIT, 0, 0, 0, 0}       // exit r0
    };
    bpf_prog_load(BPF_PROG_TYPE_SOCKET_FILTER, "net_pkt_counter", default_insns, 3);

    printk(KERN_INFO "EBPF: In-Kernel Safe Bytecode Virtual Machine & Verifier online (v1.0)\n");
}

bool bpf_verify_prog(const bpf_insn_t* insns, uint32_t count) {
    if (!insns || count == 0 || count > BPF_MAX_INSNS) return false;

    bool has_exit = false;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t cls = insns[i].opcode & 0x07;
        uint8_t dst = insns[i].dst_reg;
        uint8_t src = insns[i].src_reg;

        if (dst >= BPF_REG_CNT || src >= BPF_REG_CNT) return false;

        // Check if instruction is EXIT
        if (insns[i].opcode == (BPF_CLASS_JMP | BPF_JMP_EXIT)) {
            has_exit = true;
        }

        // Check jump offset bounds
        if (cls == BPF_CLASS_JMP && insns[i].opcode != (BPF_CLASS_JMP | BPF_JMP_EXIT)) {
            int target = (int)i + 1 + insns[i].offset;
            if (target < 0 || target >= (int)count) return false; // Out-of-bounds jump
        }
    }

    return has_exit;
}

int bpf_prog_load(bpf_prog_type_t type, const char* name, const bpf_insn_t* insns, uint32_t count) {
    if (!name || !insns || count == 0) return -1;

    if (!bpf_verify_prog(insns, count)) {
        printk(KERN_ERR "BPF: Verifier rejected program '%s' (Safety bounds violation)\n", name);
        return -1;
    }

    for (size_t i = 0; i < BPF_MAX_PROGS; i++) {
        if (!bpf_programs[i].in_use) {
            bpf_programs[i].id = next_bpf_id++;
            strncpy(bpf_programs[i].name, name, sizeof(bpf_programs[i].name) - 1);
            bpf_programs[i].type = type;
            memcpy(bpf_programs[i].insns, insns, count * sizeof(bpf_insn_t));
            bpf_programs[i].insn_count = count;
            bpf_programs[i].run_count = 0;
            bpf_programs[i].total_runtime_ns = 0;
            bpf_programs[i].in_use = true;
            return (int)bpf_programs[i].id;
        }
    }
    return -1;
}

int bpf_prog_run(uint32_t prog_id, void* ctx, uint64_t* ret_val) {
    bpf_prog_t* prog = NULL;
    for (size_t i = 0; i < BPF_MAX_PROGS; i++) {
        if (bpf_programs[i].in_use && bpf_programs[i].id == prog_id) {
            prog = &bpf_programs[i];
            break;
        }
    }
    if (!prog) return -1;

    uint64_t regs[BPF_REG_CNT] = {0};
    uint64_t stack[64] = {0};
    regs[1] = (uint64_t)ctx;
    regs[10] = (uint64_t)&stack[63]; // Frame pointer

    uint32_t pc = 0;
    while (pc < prog->insn_count) {
        bpf_insn_t insn = prog->insns[pc];
        uint8_t cls = insn.opcode & 0x07;
        uint8_t op  = insn.opcode & 0xF0;
        uint8_t dst = insn.dst_reg;
        uint8_t src = insn.src_reg;
        uint64_t val = (insn.opcode & 0x08) ? regs[src] : (uint32_t)insn.imm;

        switch (cls) {
            case BPF_CLASS_ALU64:
            case BPF_CLASS_ALU:
                switch (op) {
                    case BPF_OP_ADD: regs[dst] += val; break;
                    case BPF_OP_SUB: regs[dst] -= val; break;
                    case BPF_OP_MUL: regs[dst] *= val; break;
                    case BPF_OP_DIV: if (val != 0) regs[dst] /= val; break;
                    case BPF_OP_OR:  regs[dst] |= val; break;
                    case BPF_OP_AND: regs[dst] &= val; break;
                    case BPF_OP_XOR: regs[dst] ^= val; break;
                    case BPF_OP_MOV: regs[dst] = val; break;
                    default: break;
                }
                break;

            case BPF_CLASS_JMP:
                if (op == BPF_JMP_EXIT) {
                    if (ret_val) *ret_val = regs[0];
                    prog->run_count++;
                    prog->total_runtime_ns += 50;
                    return 0;
                } else if (op == BPF_JMP_JA) {
                    pc += insn.offset;
                } else if (op == BPF_JMP_JEQ && regs[dst] == val) {
                    pc += insn.offset;
                } else if (op == BPF_JMP_JNE && regs[dst] != val) {
                    pc += insn.offset;
                } else if (op == BPF_JMP_JGT && regs[dst] > val) {
                    pc += insn.offset;
                } else if (op == BPF_JMP_JGE && regs[dst] >= val) {
                    pc += insn.offset;
                }
                break;

            default:
                break;
        }
        pc++;
    }

    if (ret_val) *ret_val = regs[0];
    prog->run_count++;
    return 0;
}

int bpf_map_create(const char* name, uint32_t key_size, uint32_t value_size, uint32_t max_entries) {
    if (!name || key_size == 0 || value_size == 0 || max_entries == 0) return -1;

    for (size_t i = 0; i < BPF_MAX_MAPS; i++) {
        if (!bpf_maps[i].in_use) {
            bpf_maps[i].id = next_bpf_id++;
            strncpy(bpf_maps[i].name, name, sizeof(bpf_maps[i].name) - 1);
            bpf_maps[i].key_size = key_size;
            bpf_maps[i].value_size = value_size;
            bpf_maps[i].max_entries = max_entries;
            bpf_maps[i].data = (uint8_t*)kzalloc(max_entries * (key_size + value_size));
            bpf_maps[i].in_use = true;
            return (int)bpf_maps[i].id;
        }
    }
    return -1;
}

size_t bpf_get_prog_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < BPF_MAX_PROGS; i++) {
        if (bpf_programs[i].in_use) count++;
    }
    return count;
}

const bpf_prog_t* bpf_get_prog(size_t index) {
    if (index >= BPF_MAX_PROGS || !bpf_programs[index].in_use) return NULL;
    return &bpf_programs[index];
}
