use16
org 0x7c00

start:
	mov ax, cs
	mov ds, ax
	mov ss, ax
	mov sp, start

main:
	call cls
	mov bx, loading_str
	call puts

	jmp keyboard_check

cls:
	mov al, 0x03
	mov ah, 0x00
	int 0x10
	ret

puts:					;output
	mov al, [bx]
	test al, al
	jz end_puts
	mov ah, 0x0e
	int 0x10
	inc bx
	jmp puts

end_puts:
	ret

gdt:
	db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	db 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
	db 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00
gdt_info:
	dw gdt_info - gdt
	dw gdt, 0 



loading_str: db "press space", 0
new_line: db 0x0d, 0x0a, 0

keyboard_check:
    mov ah, 0
    int 0x16
    cmp al, 0x20   
    jne keyboard_check


kernel:
	mov ax, 0x1000
	mov es, ax
	mov bx, 0x00
	mov ah, 0x02
	mov dl, 0x01
	mov dh, 0x00
	mov ch, 0x00
	mov al, 20
	mov cl, 0x01
	int 0x13

	cli
	lgdt [gdt_info]

	in al, 0x92
	or al, 2
	out 0x92, al

	mov eax, cr0
	or al, 1
	mov cr0, eax
	jmp 0x8:protected_mode

use32
protected_mode:
	mov ax, 0x10
	mov es, ax
	mov ds, ax
	mov ss, ax
	call 0x10000



times (512 - ($ - start) - 2) db 0
db 0x55, 0xAA
