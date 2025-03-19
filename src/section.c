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

size_t get_section_size()
{

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
	
void show_all_section_names(struct list_head *list)
{
	struct section *sec;

	printf("==========================\n");
	printf("  Sections of ELF File\n");
	printf("==========================\n");
	list_for_each_entry(sec, list, list) {
		printf("%s\n", sec->name);
	}
	printf("\n");
}

struct section *find_section_by_name(struct list_head *list, char *name)
{
	struct section *sec;
	list_for_each_entry(sec, list, list){
		if (!strcmp(sec->name, name))
			return sec;
	}
	return NULL;
}

/**
 * extra vermagic from .modinfo section
 * 
 * @param 	pointer to the Elf_Data of .modinfo
 * @return  pointer to vermagic
 */
char *extract_vermagic(const Elf_Data *data) {
    if (!data || !data->d_buf || data->d_size == 0) {
        return NULL;
    }

    char *modinfo = (char *)data->d_buf;
    size_t remaining = data->d_size;

    // find all pair end with '\0'
    while (remaining > 0) {
        /* move to next pair
		 * should know that strnlen will search 
		 * the length end with the first '\0'
		 * so, using this function can find the string
		 * sepreate by '\0' easyily
		 */
        size_t len = strnlen(modinfo, remaining);
        if (len == 0) {
            break;
        }

        // search in modinfo is a vermagic string
        if (strncmp(modinfo, "vermagic=", 9) == 0) {
            char *vermagic_start = modinfo + 9;
            size_t vermagic_len = len - 9;
            char *vermagic = malloc(vermagic_len + 1);
            if (!vermagic) {
                return NULL;
            }
            strncpy(vermagic, vermagic_start, vermagic_len);
            vermagic[vermagic_len] = '\0';
            return vermagic;
        }

        // move to the next pair
        modinfo += len + 1;
        remaining -= len + 1;
    }

    return NULL; 
}

int replace_vermagic(Elf_Data *data, char *new_vermagic, char *old_vermagic) {
	if (!data || !data->d_buf || data->d_size == 0) {
        return NULL;
    }

    char *modinfo = (char *)data->d_buf;
    size_t remaining = data->d_size;

	// alloc a newer size of the modinfo with new_vermagic
	size_t old_vermagic_len = strlen(old_vermagic);
	size_t new_vermagic_len = strlen(new_vermagic);

	/*
	 * 
	 */
	if (old_vermagic_len == new_vermagic_len) {
		
	} else {
		if (old_vermagic_len < new_vermagic_len) {

		} else {

		}
	}

    return -1; 
}

char *get_vermagic_strings(struct section *sec, size_t section_size) 
{
	char *vermagic_string;
	Elf_Data *elf_data;
	char *start  = sec->data;
	char *end = sec->data + section_size;
	char *vermagic_start = NULL;

	elf_data = sec->data->d_buf;
	// find the vermagic start
	while (start < end) {
		char *new_line = memchr(start, '\0', end - start);
		if (!new_line) break;
		// if (!start) break;
		if (strncmp(start, "vermagic=", 9) == 0) {
            vermagic_start = start;
			printf("FOUND!!!!\n");
            break;
        }

        start = start + 1;
	}
	return vermagic_start;
	//vermagic_string = extract_vermagic(elf_data);
	return vermagic_string;
}


void change_vermagic(struct ELFT *elf_tool, char *new_vermagic) 
{
	struct section *modinfo_section;
	char *old_vermagic_start;
	Elf_Data *elf_data;
	char *data;
	size_t section_size;

	modinfo_section = find_section_by_name(&elf_tool->sections, ".modinfo");
	
	if (modinfo_section == NULL) {
		fprintf(stderr, "can not find .modinfo section");
		return -1;
	}

	section_size = modinfo_section->shdr.sh_size;

	old_vermagic_start = get_vermagic_strings(modinfo_section, section_size);

	printf("%s", old_vermagic_start);
}