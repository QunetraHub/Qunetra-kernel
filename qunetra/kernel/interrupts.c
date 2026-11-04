/*
 * Copyright (c) 2026 Qunerum
 *
 * This file is part of Qunetra.
 *
 * Qunetra is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Qunetra is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Qunetra. If not, see <https://www.gnu.org/licenses/>.
 */

#include "../utility/types.h"
#include "../utility/cpu.h"
#include "interrupts.h"

typedef struct __attribute__((packed)) {
	uint16 offsetLow, selector;
	uint8 zero, flags;
	uint16 offsetHigh;
} idtEntry;
typedef struct __attribute__((packed)) {
	uint16 limit;
	uint32 base;
} idtPtr;

static idtEntry idt[256];
static idtPtr idtp;

void idtSetGate(const uint8 n, const uint32 handler) {
	idt[n].offsetLow = handler & 0xFFFF;
	idt[n].selector = 0x08;
	idt[n].zero = 0;
	idt[n].flags = 0x8E;
	idt[n].offsetHigh = (handler >> 16) & 0xFFFF;
}
static void picRemap() {
	outb(0x20, 0x11); ioWait();
	outb(0xA0, 0x11); ioWait();
	outb(0x21, 0x20); ioWait();
	outb(0xA1, 0x28); ioWait();
	outb(0x21, 0x04); ioWait();
	outb(0xA1, 0x02); ioWait();
	outb(0x21, 0x01); ioWait();
	outb(0xA1, 0x01); ioWait();
	outb(0x21, 0xFD);
	outb(0xA1, 0xFF);
}
state initInterrupts() {
	picRemap();
	idtp.limit = sizeof(idt) - 1;
	idtp.base = (uint32)(uiptr)idt;
	__asm__ volatile ("lidt %0" : : "m"(idtp));

	idtPtr check;
	__asm__ volatile ("sidt %0" : "=m"(check));
	return check.limit == idtp.limit && check.base == idtp.base && inb(0x21) == 0xFD;
}
