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

#ifndef QUNETRA_MATH_H
#define QUNETRA_MATH_H

#include "types.h"

static inline uint kabs(const int v) { return v < 0 ? -v : v; }
static inline uint64 kabs64(const int64 v) { return v < 0 ? -v : v; }

static inline int kmin(const int a, const int b) { return a < b ? a : b; }
static inline int64 kmin64(const int64 a, const int64 b) { return a < b ? a : b; }

static inline int kmax(const int a, const int b) { return a > b ? a : b; }
static inline int64 kmax64(const int64 a, const int64 b) { return a > b ? a : b; }

static inline int kclamp(const int v, const int _min, const int _max) { return v < _min ? _min : v > _max ? _max : v; }
static inline int64 kclamp64(const int64 v, const int64 _min, const int64 _max) { return v < _min ? _min : v > _max ? _max : v; }

static inline int8 ksign(const int v) { return v < 0 ? -1 : v > 0 ? 1 : 0; }
static inline int8 ksign64(const int64 v) { return v < 0 ? -1 : v > 0 ? 1 : 0; }

static inline uint mathTest() {
	uint x = 0;
	x += kabs(-51) == 51;
	x += kmin(-21, 37) == -21;
	x += kmax(21, -37) == 21;
	x += kclamp(239, 41, 203) == 203 && kclamp(124, 231, 429) == 231;
	x += ksign(-12) == -1 && ksign(16) == 1 && ksign(0) == 0;
	return x;
}

#endif
