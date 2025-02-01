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
	printf("%d***\n", EI_NIDENT);
	
	for (i=0; i < EI_NIDENT; i++){
		printf(elf_tool->ehdr.e_ident[i]);
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

void usage() 
{
	printf("ELF-TOOL USAGE:\n");
	printf("elf-tool <origin_elf_file> <output_elf_file> <command> <sub-command>\n");
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
		printf("		--change-vermagic\n");
		printf("		--show-elfheader\n");
		printf("		--show-elf-machinetype");
		printf("\n");
	}
}

int main(int argc, char *argv[])
{
	const char *command = argv[1];
	const char *subcommand = argv[2];
	const char *origin_elf_file_path = argv[3];
	char *output_elf_file_path;
	struct ELFT *elf_tool;
	int fd;
	size_t sec_nr;

	if (argc <= 3) {
		usage();
		return -1;
    }

	if (argc >=4 ) {
		output_elf_file_path = argv[4];
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
		if (!strcmp(subcommand, "--show-elfheader")){
			printf("showling elfheader of object file: %s\n", origin_elf_file_path);
			show_ehdr(elf_tool);
		}
		if (!strcmp(subcommand, "--show-elf-machinetype")) {
			print_elf_machine_type(1);
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
