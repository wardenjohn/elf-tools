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

//static char args_doc[] = "target.o";

extern int get_sections(struct ELFT *elf_tool);

static void print_elf_machine_type(size_t type)
{
	switch (type)
	{
		case EM_X86_64:
			printf("Arch: x86_64\n");
			break;
		case EM_IA_64:
			printf("Arch: IA_64\n");
			break;
		case EM_ARM:
			printf("Arch: ARM\n");
			break;
		case EM_AARCH64:
			printf("Arch: AARCH64\n");
		default:
			printf("Arch: NOT INCLUDED ARCH\n");
			break;
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

static void show_ehdr(struct ELFT *elf_tool)
{
	int i;
	printf("==========================\n");
	printf("  Elf Header Information\n");
	printf("==========================\n");
	print_elf_machine_type(elf_tool->ehdr.e_machine);
	printf("ELF Version: %d\n", elf_tool->ehdr.e_version);
	printf("ELF Entry point virtual address: %lx\n", elf_tool->ehdr.e_entry);
	printf("ELF Header size (bytes): %d\n", elf_tool->ehdr.e_ehsize);
	printf("ELF Program header table file offset: %lx\n", elf_tool->ehdr.e_phoff);
	printf("ELF Program header table entry size: %d\n", elf_tool->ehdr.e_phentsize);
	printf("ELF Program header table entry count: %d\n", elf_tool->ehdr.e_phnum);
	printf("ELF Section header table file offset: %lx\n", elf_tool->ehdr.e_shoff);
	printf("ELF Section header table entry size: %d\n", elf_tool->ehdr.e_shentsize);
	printf("ELF Section header table entry count: %d\n", elf_tool->ehdr.e_shnum);
	printf("ELF Section header string table index: %d\n", elf_tool->ehdr.e_shstrndx);
	printf("ELF e_ident character (combines into one line): ");
	for (i=0; i < EI_NIDENT; i++){
		printf("%c", elf_tool->ehdr.e_ident[i]);
	}
	printf("\n");
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

static void write_result_elf_output(struct ELFT *elf_tool, char *elf_output_path) {

}

void usage() 
{
	printf("ELF-TOOL USAGE:\n");
	printf("elf-tool <command> <sub-command> <origin_elf_file> <output_elf_file(if provided)> \n");
	printf("Commands: \n");
	printf("	help\n");
	printf("		sections\n");
	printf("		symtab\n");
	printf("		elfheader\n");
}

void print_subcommand(char *subcommand) 
{

	printf("elf-tool <command> <sub-command> <origin_elf_file> <output_elf_file(if provided)> \n");
	printf("Commands: \n");
	if (!strcmp(subcommand, "sections")) {
		printf("	sections \n");
		printf("		--show-all-sections \"show all sections of this elf file\"\n");
		printf("		--show-sections-by-name \"show sections filter by the given name\"\n");
	}
	if (!strcmp(subcommand, "symtab")) {
		printf("	symtab\n");
		printf("		--show \"show this elf file's symbol table\" \n");
		printf("		--show-symbols \"show this elf file's symbols\"");
	}
	if (!strcmp(subcommand, "elfheader")) {
		printf("	elfheader\n");
		printf("		--change-vermagic <new vermagic>\n");
		printf("		--show-elfheader\n");
		printf("\n");
	}
}

int main(int argc, char *argv[])
{
	const char *command = argv[COMMAND_PARAMS_POS];
	const char *subcommand = argv[SUBCOMMAND_PARAMS_POS];
	const char *origin_elf_file_path = argv[ORIGINAL_OBJECT_FILE_PARAMS_POS];
	char *output_elf_file_path = NULL;
	char *new_vermagic = NULL;
	struct ELFT *elf_tool;

	if (argc <= 3) {
		usage();
		return -1;
    }
	
	if (argc >= 5) {
		if (OUTPUT_OBJECT_FILE_PARAMS_POS >= argc) {
            fprintf(stderr, "Error: Missing output ELF file path.\n");
            usage();
            return -1;
        }
		output_elf_file_path = argv[OUTPUT_OBJECT_FILE_PARAMS_POS];
	}

	if (!strcmp(origin_elf_file_path, "help") ||
		!strcmp(origin_elf_file_path, "h")) {
		usage();
		return 0;
	}

	if(access(origin_elf_file_path, F_OK)) {
		printf("file %s is not exist, please check\n", origin_elf_file_path);
		return -1;
	}

	if(!strcmp(command, "help")){
		print_subcommand(subcommand);
	}
		
	elf_tool = malloc(sizeof(struct ELFT));
	elf_open(elf_tool, origin_elf_file_path);
	
	if (!strcmp(command, "elfheader")){
		if (!strcmp(subcommand, "--show-elfheader")) {
			printf("showling elfheader of object file: %s\n", origin_elf_file_path);
			show_ehdr(elf_tool);
		}	
		if (!strcmp(subcommand, "--change-vermagic")) {
			if (argc < 6) {
				fprintf(stderr, "vermagic is not provided");
				usage();
				return -1;
			}
			new_vermagic = argv[VERMAGIC_PARAMS_POS];
			printf("Start to change the vermagic to %s\n", new_vermagic);
			change_vermagic(elf_tool, new_vermagic);
		}
	} else if (!strcmp(command, "symtab")) {
		printf("calling symtab\n");
		print_helloworl();
	} else if (!strcmp(command, "sections")) {
		printf("calling sections");
	} else {
		printf("not support command\n");
		return -1;
	}

	return 0;
}
