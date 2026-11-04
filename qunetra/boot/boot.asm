 ;
 ; Copyright (c) 2026 Qunerum
 ;
 ; This file is part of Qunetra.
 ;
 ; Qunetra is free software: you can redistribute it and/or modify
 ; it under the terms of the GNU General Public License as published by
 ; the Free Software Foundation, either version 3 of the License, or
 ; (at your option) any later version.
 ;
 ; Qunetra is distributed in the hope that it will be useful,
 ; but WITHOUT ANY WARRANTY; without even the implied warranty of
 ; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 ; GNU General Public License for more details.
 ;
 ; You should have received a copy of the GNU General Public License
 ; along with Qunetra. If not, see <https://www.gnu.org/licenses/>.
 ;

section .multiboot
header_start:
	dd 0xe85250d6
	dd 0
	dd header_end - header_start
	dd -(0xe85250d6 + 0 + (header_end - header_start))

	align 8
	dw 5
	dw 0
	dd 20
	dd 0
	dd 0
	dd 32

	align 8
	dw 0
	dw 0
	dd 8
header_end:

section .text
global start
extern kernel_main

start:
	cli
	lgdt [gdt_descriptor]
	jmp 0x08:.set_cs

.set_cs:
	mov cx, 0x10
	mov ds, cx
	mov es, cx
	mov fs, cx
	mov gs, cx
	mov ss, cx

	mov esp, stack_top

	push ebx
	push eax
	call kernel_main

global irq1_stub
extern keyboardIrqHandler
irq1_stub:
	pushad
	cld
	call keyboardIrqHandler
	popad
	iretd

.halt:
	cli
	hlt
	jmp .halt

section .data
align 8
gdt_start:
	dq 0

gdt_code:
	dw 0xFFFF
	dw 0x0000
	db 0x00
	db 10011010b
	db 11001111b
	db 0x00

gdt_data:
	dw 0xFFFF
	dw 0x0000
	db 0x00
	db 10010010b
	db 11001111b
	db 0x00

gdt_end:

gdt_descriptor:
	dw gdt_end - gdt_start - 1
	dd gdt_start

section .bss
align 16
stack_bottom:
	resb 16384
stack_top:
