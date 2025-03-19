#ifndef _ELFTOOL_H_
#define _ELFTOOL_H_
// For more information
// Please read /usr/include/libelf.h or /usr/include/elf.h
#include <gelf.h>
#include <stdbool.h>
#include <linux/module.h>
#include "list.h"

/*
 * Define the argv pos for each parameters
 */
#define COMMAND_PARAMS_POS 1
#define SUBCOMMAND_PARAMS_POS 2
#define ORIGINAL_OBJECT_FILE_PARAMS_POS 3
#define OUTPUT_OBJECT_FILE_PARAMS_POS 4
#define VERMAGIC_PARAMS_POS 5

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
