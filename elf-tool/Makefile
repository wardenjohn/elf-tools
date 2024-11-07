RES = elf-tool

elf-tool: section.o elf-tool.o
	gcc section.o elf-tool.o -lelf -o $(RES)

elf-tool.o: elf-tool.c
	gcc -c elf-tool.c -o elf-tool.o

section.o: section.c
	gcc -c section.c -o section.o

clean:
	rm -rf $(RES) *.o

clear:
	rm -rf $(RES) *.o
