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
 * @list: list node of a section
 * @shdr: section header of this section
 * @data: data of this section
 * @name: the name of this section
 */
struct section{
    struct list_head list;
    GElf_Shdr shdr;
    Elf_Data *data;
    char *name; 
    unsigned int index;
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

/*
 * ELF Header -- 32bit
 */
typedef struct {
    unsigned char   e_ident[EI_NIDENT]; // elf magic
    Elf32_Half      e_type;             // elf type
    Elf32_Half      e_machine;          // elf machine
    Elf32_Word      e_version;          // elf version
    Elf32_Addr      e_entry;            // elf head entry address
    Elf32_Off       e_phoff;            // elf program header offset
    Elf32_Off       e_shoff;            // elf section header offset
    Elf32_Word      e_flag;             // elf 
    Elf32_Half      e_ehsize;           // elf entry size
    Elf32_Half      e_phentsize;        // entry size of each program ent
    Elf32_Half      e_shentsize;        // entry size of each section header ent
    Elf32_Half      e_shnum;            // number of section header
    Elf32_Half      e_shstrndx;         // index of section os section header string table
}Elf32_EHDR;

/*
 * ELF Header -- 64bit
 */
typedef struct {
    unsigned char   e_identp[EI_NIDENT];
    Elf64_Half      e_type;
    Elf64_Half      e_machine;
    Elf64_Word      e_version;
    Elf64_Addr      e_entry;
    Elf64_Off       e_phoff;
    Elf64_Off       e_shoff;
    Elf64_Half      e_flag;
    Elf64_Half      e_ehsize;
    Elf64_Half      e_phentsize;
    Elf64_Half      e_shentsize;
    Elf64_Half      e_shnum;
    Elf64_Half      e_shstrndx;
} Elf64_EHDR;


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
