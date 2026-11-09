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

#ifndef QUNETRA_PCI_H
#define QUNETRA_PCI_H

#include "../utility/types.h"
#include "../utility/cpu.h"

static inline uint32 pciRead32(const uint8 bus, const uint8 device, const uint8 function, const uint8 offset) {
	outl(0xCF8, 0x80000000u | ((uint32)bus << 16) | ((uint32)(device & 0x1F) << 11) | ((uint32)(function & 0x07) << 8) | (offset & 0xFC));
	return inl(0xCFC);
}
static inline void pciWrite32(const uint8 bus, const uint8 device, const uint8 function, const uint8 offset, const uint32 v) {
	outl(0xCF8, 0x80000000u | ((uint32)bus << 16) | ((uint32)(device & 0x1F) << 11) | ((uint32)(function & 0x07) << 8) | (offset & 0xFC));
	outl(0xCFC, v);
}

#endif
