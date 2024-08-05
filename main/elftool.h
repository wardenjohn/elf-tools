#ifndef _ELFTOOL_H_
#define _ELFTOOL_H_

#include <gelf.h>
#include <stdbool.h>
#include "list.h"

enum architecture {
     PPC64  = 0x1 << 0,
     X86_64 = 0x1 << 1,
     S390   = 0x1 << 2,
     AARCH64= 0x1 << 3,
     ARM    = 0x1 << 4,
 };

/*
 * Structure of Elf Symbol
 *
 */
struct symbol {
    struct list_head list;
    
};

/*
 * Structure of Elf Section
 *
 * To describe a section in elf, it should be 
 * shown as a list node.
 *
 *
 */
struct section{
    struct list_head list;
};

/*
 * Structure of Elf string table
 *
 *
 */
struct string {
    struct list_head list;
    char *name;
};

struct ELFT{
    // internal
    int fd;
    int arch;
    Elf *elf;
    GElf_Shdr shdr;
    GElf_Ehdr ehdr;
    size_t sec_nr;

    // list contains information 
    // of elf file
    struct list_head sections;
    struct list_head symbols;
    struct list_head strings;

    Elf_Data *symtab_shndx;
};


#endif 
