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
#include "timer.h"

static uint32 tscPerMs = 2000000;
static state calibrated = false;

static inline uint64 rdtsc() {
	uint32 lo, hi;
	__asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64)hi << 32) | lo;
}
static uint64 divU64(uint64 n, const uint64 d) {
	if (d == 0) return 0;
	uint64 q = 0, r = 0;
	for (int8 i = 63; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) {
			r -= d;
			q |= (uint64)1 << i;
		}
	}
	return q;
}
static uint16 pitRead() {
	outb(0x43, 0x00);
	const uint8 lo = inb(0x40), hi = inb(0x40);
	return (uint16)((hi << 8) | lo);
}
void timerSetTscPerMs(const uint32 ticks) {
	if (ticks == 0) return;
	tscPerMs = ticks;
	calibrated = true;
}
state initTimer() {
	if (calibrated) return true;
	outb(0x43, 0x34);
	outb(0x40, 0);
	outb(0x40, 0);
	uint32 pitTicks = 0;
	uint16 last = pitRead();
	const uint64 start = rdtsc();
	while (pitTicks < 1193u * 64) {
		const uint16 now = pitRead();
		pitTicks += (uint16)(last - now);
		last = now;
	}
	const uint64 delta = rdtsc() - start, perMs = divU64(delta * 1193, pitTicks);
	if (perMs < 100000 || perMs > 20000000) return false;
	tscPerMs = (uint32)perMs;
	calibrated = true;
	return true;
}
uint64 timerDeadline(const uint32 ms) { return rdtsc() + (uint64)ms * tscPerMs; }
state timerExpired(const uint64 deadline) { return rdtsc() >= deadline; }
uint32 timerSeed() { return (uint32)rdtsc(); }
