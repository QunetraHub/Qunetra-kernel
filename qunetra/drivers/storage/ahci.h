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

#ifndef QUNETRA_AHCI_H
#define QUNETRA_AHCI_H

#include "../../utility/types.h"

state ahci_init(const uint8 bus, const uint8 device, const uint8 function);

void ahci_readSector(const uint64 lba, uint8 *buf);
void ahci_writeSector(const uint64 lba, const uint8 *buf);
uint64 ahci_getSectorCount();

#endif
