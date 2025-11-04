#ifndef __ELF_H
#define __ELF_H

#include "types.h"

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" little endian

struct elfhdr {
    uint magic;
    uchar elf[12];
    ushort type;
    ushort machine;
    uint version;
    uint64 entry;
    uint64 phoff;
    uint64 shoff;
    uint flags;
    ushort ehsize;
    ushort phentsize;
    ushort phnum;
    ushort shentsize;
    ushort shnum;
    ushort shstrndx;
};

struct proghdr {
    uint32 type;
    uint32 flags;
    uint64 off;
    uint64 vaddr;
    uint64 paddr;
    uint64 filesz;
    uint64 memsz;
    uint64 align;
};

#define ELF_PROG_LOAD    1

// ELF 类型
#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3

// 动态段（.dynamic）条目
typedef struct {
    int64 d_tag;
    union { uint64 d_val; uint64 d_ptr; } d_un;
} Elf64_Dyn;

// d_tag 常量（子集）
#define DT_NULL     0
#define DT_NEEDED   1
#define DT_PLTRELSZ 2
#define DT_PLTGOT   3
#define DT_HASH     4
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_SONAME   14
#define DT_RPATH    15
#define DT_SYMBOLIC 16
#define DT_REL      17
#define DT_RELSZ    18
#define DT_RELENT   19
#define DT_PLTREL   20
#define DT_DEBUG    21
#define DT_TEXTREL  22
#define DT_JMPREL   23

// 重定位（RELA）
typedef struct {
    uint64 r_offset;
    uint64 r_info;
    int64  r_addend;
} Elf64_Rela;

#define ELF64_R_SYM(i)   ((uint32)((i) >> 32))
#define ELF64_R_TYPE(i)  ((uint32)(i))

// RISC-V 重定位类型（子集）
#define R_RISCV_NONE       0
#define R_RISCV_32         1
#define R_RISCV_64         2
#define R_RISCV_RELATIVE   3
#define R_RISCV_COPY       4
#define R_RISCV_JUMP_SLOT  5
#define R_RISCV_GLOB_DAT   6

#endif
