#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <gelf.h>
#include <libgen.h>
#include <unistd.h>

#include "elftool.h"
#include "elftool_err.h"
#include "list.h"

static char args_doc[] = "target.o";

static void print_elf_machine_type(size_t type)
{
    switch (type)
    {
        case EM_X86_64:
            PRINT_NEWLINE("x86_64");
            break;
        case EM_IA_64:
            PRINT_NEWLINE("IA_64");
            break;
        case EM_ARM:
            PRINT_NEWLINE("ARM");
            break;
        case EM_AARCH64:
            PRINT_NEWLINE("AARCH64");
        default:
            PRINT_NEWLINE("NOT INCLUDED ARCH");
            break;
    }
}

int get_sections(struct ELFT *elf_tool)
{
    int section_num;
    size_t shstrndx, sections_nr;
    Elf_Scn *scn = NULL; // used to get each section
    struct section *sec;

    if (elf_getshdrnum(elf_tool->elf, &elf_tool->sec_nr))
        ERROR("get elf section header number failed\n");
    
    section_num = elf_tool->sec_nr;

    // becase index of elf_tool->sec_nr count in section with section 0
    // but elf_nextscn will not return this section.
    section_num--;

    /**
     * shstrndx is the index of the section header string table of elf file
    */
    if (elf_getshdrstrndx(elf_tool->elf, &shstrndx))
		ERROR("elf_getshdrstrndx");

    for (int i=0; i< section_num; i++){
        ALLOC_LINK(sec, &elf_tool->sections); // alloc a section node to sections list

        scn = elf_nextscn(elf_tool->elf, scn); // get the next section of elf

        if (!scn)
            ERROR("Get scn failed");

        // return this section's section header
        if (!gelf_getshdr(scn, &sec->shdr))
            ERROR("Get section header failed");

        /* sh_name is the index into section header string table
         * shstrndx is the index to section header string table
         * here we got the name index and the section header string table
         * can return the string of the section name
         */
        sec->name = elf_strptr(elf_tool->elf, shstrndx, sec->shdr.sh_name);
        if (!sec->name)
            ERROR("section name !");
        
        sec->data = elf_getdata(scn, NULL);

        if (!sec->data)
			ERROR("elf_getdata");
        
        sec->index = (unsigned int)elf_ndxscn(scn);

		if (sec->shdr.sh_type == SHT_SYMTAB_SHNDX)
			elf_tool->symtab_shndx = sec->data;

    }
}

static void get_object_arch(struct ELFT *elf_tool)
{
    int type = elf_tool->ehdr.e_machine;

    switch (type)
    {
        case EM_X86_64:
            elf_tool->arch = X86_64;
            break;
            
        case EM_ARM:
            elf_tool->arch = ARM;
            break;

        case EM_AARCH64:
            elf_tool->arch = AARCH64;
            break;
         
        default:
            ERROR("Unsupported arch");
            break;
    }
}

static void elf_open(struct ELFT *elf_tool, char *elf_path)
{
    int fd;
    
    fd = open(elf_path, O_RDONLY);
    if (!fd)
        ERROR("wrong fd!");
    
    if (elf_version(EV_CURRENT) == EV_NONE)
        ERROR("Invalid ELF Version");

    elf_tool->elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
    if (!elf_tool->elf)
        ERROR("elf begin failed");

    if (!gelf_getehdr(elf_tool->elf, &elf_tool->ehdr))
        ERROR("EHDR!\n");

    // init elf_tool's list head
    INIT_LIST_HEAD(&elf_tool->symbols);
    INIT_LIST_HEAD(&elf_tool->sections);
    INIT_LIST_HEAD(&elf_tool->strings);

    get_object_arch(elf_tool);

    get_sections(elf_tool);
}

int main(int argc, char *argv[])
{
    const char *elf_path = argv[1];
    struct ELFT *elf_tool;
    int fd;
    size_t sec_nr;
    
    elf_tool = malloc(sizeof(struct ELFT));
    elf_open(elf_tool, elf_path);

    printf("%d", elf_tool->arch);
}
