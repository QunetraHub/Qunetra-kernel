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

#ifndef QUNETRA_CPU_H
#define QUNETRA_CPU_H

#include "types.h"

static inline uint8 inb(const uint16 port) {
	uint8 val;
	__asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}
static inline uint32 inl(uint16 port) {
	uint32 ret;
	__asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}
static inline void insw(uint16 port, void *addr, uint32 count) { __asm__ volatile("cld; rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory"); }

static inline void outb(uint16 port, uint8 data) { __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port)); }
static inline void outl(uint16 port, uint32 val) { __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port)); }
static inline void outsw(uint16 port, const void *addr, uint32 count) { __asm__ volatile("cld; rep outsw" : "+S"(addr), "+c"(count) : "d"(port)); }

static inline void sti() { __asm__ volatile ("sti"); }
static inline void cli() { __asm__ volatile ("cli"); }
static inline void ioWait() { outb(0x80, 0); }

static inline void vga(const char *str) {
	volatile uint16 *vga = (volatile uint16 *)0xB8000;
	for (uint i = 0; str[i] != '\0'; i++) vga[i] = (uint16)str[i] | 0x4F00;
}

static inline void freeze() { while (1) __asm__ volatile ("hlt"); }

#endif
