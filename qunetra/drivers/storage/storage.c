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
#include <complex.h>

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
static uint8 storageType = 0;
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
					if (subclass == 0x01) {
						storageType = 1;
						return " IDE / ATA ";
					} else if (subclass == 0x06 && prog_if == 0x01) {
						storageType = 2;
						return " AHCI / SATA ";
					}
				}
			}
		}
	}
	return " NONE ";
}

static uint8 *tmpData, *tmpData2;
static uint64 actualSector = 0;
static void (*sswrite)(const uint64 sector, const uint8 *buf), (*ssread)(const uint64 sector, uint8 *buf);

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
static void setAddr(const uint64 sector, const uint16 byte, const uint64 v) {
	if (byte >= 504) return;
	for (uint8 i = 0; i < 8; i++) setByte((uint8)(v >> (8 * (7 - i))), sector, byte + i);
}
static uint64 getAddr(const uint64 sector, const uint16 byte) {
	if (byte >= 504) return 0;
	uint64 v = 0;
	for (uint8 i = 0; i < 8; i++) v |= getByte(sector, byte + i) << (8 * i);
	return v;
}
static void setNext(const uint64 sector, const uint64 next) {
	if (!sector) return;
	setAddr(sector, 0, next);
}
static uint64 getNext(const uint64 sector) {
	if (!sector) return 0;
	return getAddr(sector, 0);
}
// = = = = = META (0)
// 0 - meta
// 1 - 8 file count: 8 bytes
// sector 1 - files
// sector 2 - free sectors

// = = = = = SECTOR
// 0 - 7 next: 8 bytes
// 8 - 9 last: 8 bytes
// 10 - 11 size: 2 bytes
// 12 - 511 data: 494 bytes
#define QUNETRA_OS_META (uint8)(0b110101111)
static uint64 getFileCount() { return getAddr(0, 1); }
char* initStorage() {
	tmpData = kmalloc(512);
	tmpData2 = kmalloc(512);
	char* type = getStorageType();
	sswrite = storageType == 1 ? ata_writeSector : 0;
	ssread = storageType == 1 ? ata_readSector : 0;
	const uint8 meta = getByte(0, 0);
	if (meta != QUNETRA_OS_META) setByte(QUNETRA_OS_META, 0, 0);
	return storageType == 0 ? " ERROR " : type;
}
void closeStorage() {
	kfree(tmpData);
	kfree(tmpData2);
}

uint64 createFile(const uint64 catalog, const char* name) {
	(void)catalog;
	const uint64 fc = getFileCount();
	uint64 sts = 1;
	if (fc) {

	}
	setAddr(0, 1, fc + 1);
	return sts;
}
state fwriteb(const uint64 file, uint16 *actByte, const uint8 v) {
	if (!file || *actByte >= 494) return false;

	(*actByte)++;
	return true;
}
