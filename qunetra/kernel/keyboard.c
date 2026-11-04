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
#include "keyboard.h"

#define KBD_BUF_SIZE 64

static const char keyboardMap[128] = {
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
	0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
	0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
	'*',  0,  ' '
};

static volatile char buffer[KBD_BUF_SIZE];
static volatile uint8 head = 0, tail = 0;

extern void irq1_stub();

void keyboardIrqHandler() {
	const uint8 scancode = inb(0x60);
	if (!(scancode & 0x80) && scancode < 128) {
		const char c = keyboardMap[scancode];
		if (c) {
			const uint8 next = (head + 1) % KBD_BUF_SIZE;
			if (next != tail) {
				buffer[head] = c;
				head = next;
			}
		}
	}
	outb(0x20, 0x20);
}
char keyboardGet() {
	if (head == tail) return 0;
	const char c = buffer[tail];
	tail = (tail + 1) % KBD_BUF_SIZE;
	return c;
}
static state waitInput() {
	for (uint32 i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) return true;
	return false;
}
static state waitOutput() {
	for (uint32 i = 0; i < 100000; i++) if (inb(0x64) & 1) return true;
	return false;
}
static void flushOutput() { for (uint8 i = 0; i < 16 && (inb(0x64) & 1); i++) inb(0x60); }

state initKeyboard() {
	idtSetGate(0x21, (uint32)(uiptr)irq1_stub);
	flushOutput();
	if (!waitInput()) return false;
	outb(0x64, 0xAB);
	if (!waitOutput()) return false;
	const state ok = inb(0x60) == 0x00;
	flushOutput();
	return ok;
}
