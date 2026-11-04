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

#include "../../utility/memory.h"
#include "../../utility/types.h"
#include "../../utility/cpu.h"
#include "ata.h"

static uint32 pciCfgRead(const uint8 bus, const uint8 device, const uint8 function, const uint8 offset) {
	const uint32 address =
		(uint32)((1 << 31) |
		(bus << 16) |
		((device & 0x1F) << 11) |
		((function & 0x07) << 8) |
		(offset & 0xFC));
	outl(0xCF8, address);
	return inl(0xCFC);
}
static char* getStorageType() {
	for (uint8 bus = 0; bus < 8; bus++) {
		for (uint8 device = 0; device < 32; device++) {
			for (uint8 function = 0; function < 8; function++) {
				const uint32 vendor_device = pciCfgRead(bus, device, function, 0x00);
				if ((vendor_device & 0xFFFF) == 0xFFFF) continue;
				const uint32 class_reg = pciCfgRead(bus, device, function, 0x08);
				const uint8 class_code = (class_reg >> 24) & 0xFF,
					subclass   = (class_reg >> 16) & 0xFF,
					prog_if    = (class_reg >> 8) & 0xFF;
				if (class_code == 0x01) {
					if (subclass == 0x01) return " IDE / ATA ";
					else if (subclass == 0x06 && prog_if == 0x01) return " AHCI / SATA ";
				}
			}
		}
	}
	return " NONE ";
}

static uint8 *tmpData, *tmpData2;
static uint64 actualSector = 0;
static void (*sswrite)(const uint64 sector, const uint8 *buf), (*ssread)(const uint64 sector, uint8 *buf);

char* initStorage() {
	tmpData = kmalloc(512);
	tmpData2 = kmalloc(512);
	sswrite = ata_writeSector;
	ssread = ata_readSector;
	return getStorageType();
}
void closeStorage() {
	kfree(tmpData);
	kfree(tmpData2);
}
static void readTmp(const uint64 sector) {
	if (sector != actualSector) {
		ssread(sector, tmpData);
		actualSector = sector;
	}
}
static void readTmp2(const uint64 sector) {
	if (sector != actualSector) {
		ssread(sector, tmpData2);
		actualSector = sector;
	}
}
static void writeTmp() { sswrite(actualSector, tmpData); }
static state setByte(const uint8 val, const uint64 sector, const uint16 byte) {
	if (byte >= 512) return false;
	readTmp(sector);
	tmpData[byte] = val;
	return true;
}
static uint8 getByte(const uint64 sector, const uint16 byte) {
	if (byte >= 512) return 0;
	readTmp2(sector);
	return tmpData2[byte];
}
static void setNext(const uint64 sector, const uint64 next) { for (uint8 i = 0; i < 8; i++) setByte((uint8)(next >> (8 * (7 - i))), sector, i); }
static uint64 getNext(const uint64 sector) {
	if (sector == 0) return 0;
	uint64 next = getByte(sector, 0);
	for (uint8 i = 1; i < 8; i++) next = (next << 8) | getByte(sector, i);
	return next;
}
// = = = = = META
// 0 - 7 sector with id's: 8 bytes
// 8 - 15 sector with free sectors: 8 bytes
// 16... other data

// = = = = = SECTOR
// 0 - 7 next: 8 bytes
// 8 - 511 data: 504 bytes
void writeFile(const char* name, const uint64 dirID, const uint8* data, const uint64 len) {
	if (len == 0 || !data) return;
	(void)name;
	(void)dirID;
	return;
	getNext(0);
	setNext(0,0);
	writeTmp();
}
