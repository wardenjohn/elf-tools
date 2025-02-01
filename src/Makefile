RES = elf-tool

elf-tool: section.o symtab.o reloc.o elf-tool.o 
	gcc section.o symtab.o reloc.o elf-tool.o -lelf -o $(RES)

elf-tool.o: elf-tool.c section.o symtab.o reloc.o
	gcc -c elf-tool.c -o elf-tool.o

section.o: section.c
	gcc -c section.c -o section.o

symtab.o: symtab.c
	gcc -c symtab.c -o symtab.o

reloc.o: reloc.c
	gcc -c reloc.c -o reloc.o

clean:
	rm -rf $(RES) *.o

clear:
	rm -rf $(RES) *.o
