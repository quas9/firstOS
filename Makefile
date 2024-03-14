all:
	gcc -fno-pie -ffreestanding -m32 -o kernel.o -c kernel.cpp
	ld --oformat binary -Ttext 0x10000 -o kernel.bin --entry=kmain -m elf_i386 kernel.o
	fasm bootsect.asm
	qemu-system-i386 -fda bootsect.bin -fdb kernel.bin
