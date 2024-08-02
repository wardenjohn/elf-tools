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

enum architecture {
     PPC64  = 0x1 << 0,
     X86_64 = 0x1 << 1,
     S390   = 0x1 << 2,
 };

static char args_doc[] = "target.o";

int main(int argc, char *argv[])
{
    const char *elf_path = argv[1];
    Elf *elf;
    int fd;
    size_t sec_nr;
    GElf_Ehdr ehdr;

    fd = open(elf_path, O_RDONLY);
    if (!fd)
        printf("error fd\n");

    if (elf_version(EV_CURRENT) == EV_NONE )
        printf("EV_NONE");

    elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
    if (!elf)
        printf("not elf!\n");
    if (elf_getshdrnum(elf, &sec_nr))
        printf("num!");
    else
        printf("%d\n", sec_nr);

    if (!gelf_getehdr(elf, &ehdr))
        printf("EHDR!\n");
    else
        printf("%d\n", ehdr.e_machine);

    if (ehdr.e_machine == EM_X86_64)
        printf("xx86");
    printf("Ehdr : e_machine : %d , this arch x86 %d, %d \n", ehdr.e_machine, X86_64, EM_X86_64);
    printf("Ehdr: e_phoff: %d\n", ehdr.e_phoff);
    
    printf("Ehdr: e_shstrndx: %d\n", ehdr.e_shstrndx);
    
}
