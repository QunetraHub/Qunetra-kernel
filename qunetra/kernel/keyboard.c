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
#define MAP_SIZE 58

static const char keyboardMap[MAP_SIZE] = {
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
	0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
	0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
	'*',  0,  ' '
}, keyboardMapShift[MAP_SIZE] = {
	0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
	0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
	0,  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
	'*',  0,  ' '
};

static volatile char buffer[KBD_BUF_SIZE];
static volatile uint8 head = 0, tail = 0;
static uint8 shiftMask = 0;
static state capsOn = false, extended = false;

extern void irq1_stub();

static char translate(const uint8 sc) {
	if (sc >= MAP_SIZE) return 0;
	const char c = keyboardMap[sc];
	const state shift = shiftMask != 0;
	if (c >= 'a' && c <= 'z') return (shift != capsOn) ? keyboardMapShift[sc] : c;
	return shift ? keyboardMapShift[sc] : c;
}
static char translateExtended(const uint8 sc) {
	switch (sc) {
		case 0x48: return KEY_UP;
		case 0x50: return KEY_DOWN;
		case 0x4B: return KEY_LEFT;
		case 0x4D: return KEY_RIGHT;
		case 0x47: return KEY_HOME;
		case 0x4F: return KEY_END;
		case 0x53: return KEY_DELETE;
		case 0x1C: return '\n';
		case 0x35: return '/';
		default: return 0;
	}
}
static void push(const char c) {
	const uint8 next = (head + 1) % KBD_BUF_SIZE;
	if (next != tail) {
		buffer[head] = c;
		head = next;
	}
}

void keyboardIrqHandler() {
	const uint8 raw = inb(0x60);
	if (raw == 0xE0) extended = true;
	else {
		const state release = (raw & 0x80) != 0;
		const uint8 sc = raw & 0x7F;
		char c = 0;
		if (extended) {
			extended = false;
			if (!release) c = translateExtended(sc);
		} else if (sc == 0x2A) {
			shiftMask = release ? (shiftMask & ~1) : (shiftMask | 1);
		} else if (sc == 0x36) {
			shiftMask = release ? (shiftMask & ~2) : (shiftMask | 2);
		} else if (sc == 0x3A) {
			if (!release) capsOn = !capsOn;
		} else if (!release) c = translate(sc);
		if (c) push(c);
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
	shiftMask = 0;
	capsOn = false;
	extended = false;
	idtSetGate(0x21, (uint32)(uiptr)irq1_stub);
	flushOutput();
	if (!waitInput()) return false;
	outb(0x64, 0xAB);
	if (!waitOutput()) return false;
	const state ok = inb(0x60) == 0x00;
	flushOutput();
	return ok;
}
