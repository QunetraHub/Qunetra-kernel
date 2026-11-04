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

#ifndef QUNETRA_BITS_H
#define QUNETRA_BITS_H

#include "types.h"

static inline state getBit(const uint8 val, const uint8 bit_index) {
	if (bit_index > 7) return false;
	return (val >> bit_index) & 1;
}
static inline uint8 setBit(const uint8 val, const uint8 bit_index) {
	if (bit_index > 7) return 0;
	return val | (1 << bit_index);
}
static inline uint8 clearBit(const uint8 val, uint8 bit_index) {
	if (bit_index > 7) return 0;
	return val & ~(1 << bit_index);
}

#endif
