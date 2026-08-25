#include <kernel/elf.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <mm/vmspace.h>

#if defined(__x86_64__)
#define ELF_NATIVE_MACHINE EM_X86_64
#elif defined(__aarch64__)
#define ELF_NATIVE_MACHINE EM_AARCH64
#else
#define ELF_NATIVE_MACHINE EM_ARM
#endif

static const char* const elf_errors[] = {
    [0]                = "ok",
    [-ELF_ETRUNCATED]  = "image is truncated",
    [-ELF_EMAGIC]      = "not an ELF object",
    [-ELF_ECLASS]      = "not a little-endian ELF64 object",
    [-ELF_ETYPE]       = "not a static executable",
    [-ELF_EMACHINE]    = "built for a different architecture",
    [-ELF_ERANGE]      = "segment falls outside the user address range",
    [-ELF_ENOMEM]      = "out of memory",
    [-ELF_ENOENTRY]    = "entry point is not inside a loadable segment",
};

const char* elf_strerror(int err) {
    unsigned idx = (unsigned)(-err);
    if (err > 0 || idx >= sizeof(elf_errors) / sizeof(elf_errors[0]) || !elf_errors[idx]) {
        return "unknown error";
    }
    return elf_errors[idx];
}

static int elf_prot_of(Elf64_Word p_flags) {
    int prot = VM_PROT_USER;
    if (p_flags & PF_R) prot |= VM_PROT_READ;
    if (p_flags & PF_W) prot |= VM_PROT_WRITE;
    if (p_flags & PF_X) prot |= VM_PROT_EXEC;
    return prot;
}

static int elf_check_header(const Elf64_Ehdr* eh, size_t len) {
    if (len < sizeof(Elf64_Ehdr)) return ELF_ETRUNCATED;

    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        return ELF_EMAGIC;
    }
    if (eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_ident[EI_DATA]  != ELFDATA2LSB) {
        return ELF_ECLASS;
    }
    /* Only static executables: a PT_INTERP image would need a loader we have
     * no business running this early. */
    if (eh->e_type != ET_EXEC) return ELF_ETYPE;
    if (eh->e_machine != ELF_NATIVE_MACHINE) return ELF_EMACHINE;
    if (eh->e_phnum == 0 || eh->e_phentsize != sizeof(Elf64_Phdr)) return ELF_ETRUNCATED;

    /* The program header table itself has to be inside the image. */
    uint64_t ph_bytes = (uint64_t)eh->e_phnum * eh->e_phentsize;
    if (eh->e_phoff > len || ph_bytes > len - eh->e_phoff) return ELF_ETRUNCATED;

    return ELF_OK;
}

int elf_load(vmspace_t* vm, const void* image, size_t len,
             uint64_t* entry_out, uint64_t* brk_out) {
    if (!vm || !image) return ELF_EMAGIC;

    const Elf64_Ehdr* eh = (const Elf64_Ehdr*)image;
    int err = elf_check_header(eh, len);
    if (err != ELF_OK) return err;

    const Elf64_Phdr* ph = (const Elf64_Phdr*)((const uint8_t*)image + eh->e_phoff);
    uint64_t brk = 0;
    bool entry_mapped = false;

    for (Elf64_Half i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr* p = &ph[i];
        if (p->p_type != PT_LOAD || p->p_memsz == 0) continue;

        /* A segment may declare more memory than it stores (.bss), but never
         * less, and its file image must lie inside the blob we were handed. */
        if (p->p_filesz > p->p_memsz) return ELF_ETRUNCATED;
        if (p->p_offset > len || p->p_filesz > len - p->p_offset) return ELF_ETRUNCATED;

        /* Keep every mapping inside the slot reserved for userland, and reject
         * a length that would wrap past the top of the address space. */
        if (p->p_vaddr < USER_BASE) return ELF_ERANGE;
        if (p->p_memsz > USER_LIMIT - p->p_vaddr) return ELF_ERANGE;

        err = vmspace_alloc(vm, p->p_vaddr, p->p_memsz, elf_prot_of(p->p_flags));
        if (err != 0) return ELF_ENOMEM;

        if (p->p_filesz) {
            err = vmspace_write(vm, p->p_vaddr,
                                (const uint8_t*)image + p->p_offset, p->p_filesz);
            if (err != 0) return ELF_ENOMEM;
        }
        /* vmspace_alloc hands back zeroed pages, so .bss needs no extra work. */

        uint64_t seg_end = p->p_vaddr + p->p_memsz;
        if (seg_end > brk) brk = seg_end;

        if (eh->e_entry >= p->p_vaddr && eh->e_entry < seg_end) entry_mapped = true;
    }

    if (!brk) return ELF_ERANGE;
    if (!entry_mapped) return ELF_ENOENTRY;

    if (entry_out) *entry_out = eh->e_entry;
    if (brk_out) {
        *brk_out = (brk + VM_PAGE_SIZE - 1) & ~(VM_PAGE_SIZE - 1);
    }
    return ELF_OK;
}
