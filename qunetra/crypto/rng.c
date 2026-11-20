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
#include "cutil.h"
#include "sha2.h"
#include "rng.h"

static uint8 pool[32];
static uint32 counter = 0;
static state seeded = false, hardware = false;

static inline uint64 rdtsc() {
	uint32 lo, hi;
	__asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64)hi << 32) | lo;
}
static state cpuHasRdrand() {
	uint32 a = 1, b, c, d;
	__asm__ volatile ("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
	return (c >> 30) & 1;
}
static state rdrand32(uint32* out) {
	for (uint8 i = 0; i < 10; i++) {
		uint32 v;
		uint8 ok;
		__asm__ volatile ("rdrand %0; setc %1" : "=r"(v), "=qm"(ok) : : "cc");
		if (ok) {
			*out = v;
			return true;
		}
	}
	return false;
}
static uint8 pitLow() {
	outb(0x43, 0x00);
	return inb(0x40);
}
void rngInit() {
	sha256Ctx c;
	sha256Init(&c);
	hardware = cpuHasRdrand();
	if (hardware) {
		for (uint8 i = 0; i < 16; i++) {
			uint32 v;
			if (!rdrand32(&v)) {
				hardware = false;
				break;
			}
			sha256Update(&c, (const uint8*)&v, 4);
		}
	}
	uint8 samples[1024];
	for (uint16 i = 0; i < 1024; i += 2) {
		const uint64 t1 = rdtsc();
		const uint8 p = pitLow();
		const uint64 t2 = rdtsc();
		samples[i] = (uint8)(t2 - t1);
		samples[i + 1] = (uint8)(p ^ (uint8)t2);
	}
	sha256Update(&c, samples, sizeof(samples));
	const uint64 now = rdtsc();
	sha256Update(&c, (const uint8*)&now, 8);
	sha256Final(&c, pool);
	counter = 0;
	seeded = true;
}
state rngHasHardware() { return seeded && hardware; }
void rngBytes(uint8* out, uint32 n) {
	if (!seeded) rngInit();
	while (n > 0) {
		sha256Ctx c;
		uint8 block[32], ctr[4];
		const uint64 t = rdtsc();
		cPut32(ctr, counter++);
		sha256Init(&c);
		sha256Update(&c, pool, 32);
		sha256Update(&c, ctr, 4);
		sha256Update(&c, (const uint8*)&t, 8);
		if (hardware) {
			uint32 v;
			if (rdrand32(&v)) sha256Update(&c, (const uint8*)&v, 4);
		}
		sha256Final(&c, block);
		const uint k = n < 32 ? n : 32;
		cCopy(out, block, k);
		out += k;
		n -= k;
		sha256Init(&c);
		sha256Update(&c, pool, 32);
		sha256Update(&c, block, 32);
		sha256Final(&c, pool);
	}
}
