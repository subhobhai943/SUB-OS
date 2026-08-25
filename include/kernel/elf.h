#ifndef _KERNEL_ELF_H
#define _KERNEL_ELF_H

#include <stdint.h>
#include <stddef.h>
#include <mm/vmspace.h>

/* ELF64 object file format, as far as a static executable needs it. */

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr;
    Elf64_Addr  p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} __attribute__((packed)) Elf64_Phdr;

/* e_ident indices and values */
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6
#define ELFCLASS64    2
#define ELFDATA2LSB   1
#define EV_CURRENT    1

/* e_type */
#define ET_EXEC       2
#define ET_DYN        3

/* e_machine */
#define EM_X86_64     62
#define EM_AARCH64    183
#define EM_ARM        40

/* p_type */
#define PT_NULL       0
#define PT_LOAD       1
#define PT_DYNAMIC    2
#define PT_INTERP     3
#define PT_NOTE       4
#define PT_PHDR       6
#define PT_GNU_STACK  0x6474e551

/* p_flags */
#define PF_X          0x1
#define PF_W          0x2
#define PF_R          0x4

/* elf_load return codes */
#define ELF_OK             0
#define ELF_ETRUNCATED    -1   /* image smaller than the headers claim */
#define ELF_EMAGIC        -2   /* not an ELF file */
#define ELF_ECLASS        -3   /* not ELF64 little-endian */
#define ELF_ETYPE         -4   /* not a static executable */
#define ELF_EMACHINE      -5   /* built for another architecture */
#define ELF_ERANGE        -6   /* a segment falls outside the user range */
#define ELF_ENOMEM        -7   /* out of physical pages */
#define ELF_ENOENTRY      -8   /* entry point is not inside any PT_LOAD */

const char* elf_strerror(int err);

/*
 * Parse a static ELF64 executable held in kernel memory and lay its PT_LOAD
 * segments into `vm`. On success *entry_out receives e_entry and *brk_out the
 * page-aligned end of the highest segment, which is where the heap starts.
 */
int elf_load(vmspace_t* vm, const void* image, size_t len,
             uint64_t* entry_out, uint64_t* brk_out);

#endif /* _KERNEL_ELF_H */
