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

#ifndef QUNETRA_CUTIL_H
#define QUNETRA_CUTIL_H

#include "../utility/types.h"

static inline void cCopy(void* dst, const void* src, uint n) {
	uint8* d = (uint8*)dst;
	const uint8* s = (const uint8*)src;
	while (n--) *d++ = *s++;
}
static inline void cZero(void* dst, uint n) {
	volatile uint8* d = (volatile uint8*)dst;
	while (n--) *d++ = 0;
}
static inline state cEq(const void* a, const void* b, uint n) {
	const uint8 *x = (const uint8*)a, *y = (const uint8*)b;
	uint8 diff = 0;
	while (n--) diff |= (uint8)(*x++ ^ *y++);
	return diff == 0;
}
static inline uint16 cGet16(const uint8* p) { return (uint16)((p[0] << 8) | p[1]); }
static inline uint32 cGet24(const uint8* p) { return ((uint32)p[0] << 16) | ((uint32)p[1] << 8) | p[2]; }
static inline uint32 cGet32(const uint8* p) { return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3]; }
static inline uint64 cGet64(const uint8* p) { return ((uint64)cGet32(p) << 32) | cGet32(p + 4); }
static inline void cPut16(uint8* p, const uint16 v) {
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)v;
}
static inline void cPut24(uint8* p, const uint32 v) {
	p[0] = (uint8)(v >> 16);
	p[1] = (uint8)(v >> 8);
	p[2] = (uint8)v;
}
static inline void cPut32(uint8* p, const uint32 v) {
	p[0] = (uint8)(v >> 24);
	p[1] = (uint8)(v >> 16);
	p[2] = (uint8)(v >> 8);
	p[3] = (uint8)v;
}
static inline void cPut64(uint8* p, const uint64 v) {
	cPut32(p, (uint32)(v >> 32));
	cPut32(p + 4, (uint32)v);
}

#endif
