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

#include "../../utility/types.h"
#include "../../utility/cpu.h"
#include "../../utility/string.h"
#include "../../kernel/console.h"
#include "ata.h"
#include "ahci.h"
#include "storage.h"

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
static uint8 storageType = 0; // 0 = none, 1 = IDE / ATA, 2 = AHCI / SATA
static state haveIde = false, haveAhci = false;
static uint8 ahciBus = 0, ahciDevice = 0, ahciFunction = 0;
static void scanPci() {
	haveIde = false;
	haveAhci = false;
	for (uint8 bus = 0; bus < 8; bus++) {
		for (uint8 device = 0; device < 32; device++) {
			for (uint8 function = 0; function < 8; function++) {
				const uint32 vendor_device = pciCfgRead(bus, device, function, 0x00);
				if ((vendor_device & 0xFFFF) == 0xFFFF) continue;
				const uint32 class_reg = pciCfgRead(bus, device, function, 0x08);
				const uint8 class_code = (class_reg >> 24) & 0xFF,
				subclass   = (class_reg >> 16) & 0xFF,
				prog_if    = (class_reg >> 8) & 0xFF;
				if (class_code != 0x01) continue;
				if (subclass == 0x01) haveIde = true;
				else if (subclass == 0x06 && prog_if == 0x01 && !haveAhci) {
					haveAhci = true;
					ahciBus = bus;
					ahciDevice = device;
					ahciFunction = function;
				}
			}
		}
	}
}
// ===================================================================================
// Layout
//
// Disk sectors (512 B):
//   0 - meta:        byte 0 = QUNETRA_OS_META, bytes 1-8 = file count
//   1 - root catalog (head of a sector chain holding directory entries)
//   2 - free sectors: next = head of the free list,
//                     data[0-7] = first never used sector (high-water mark)
//   3+ - data sectors
//
// Every chain sector:
//   0  - 7   next: next sector in the chain (0 = end)
//   8  - 15  last: last sector of the chain (valid in the FIRST sector of a chain)
//   16 - 17  size: number of used data bytes in this sector
//   18 - 511 data: 494 bytes
//
// A file = a chain of sectors, its id = number of the first sector.
// Directory entry (40 B): 0-7 first sector of the file (0 = free slot), 8-39 name (NUL terminated)
// Numbers are stored big endian.
// ===================================================================================
#define QUNETRA_OS_META (uint8)(0b11100111)

#define SEC_NEXT 0
#define SEC_LAST 8
#define SEC_SIZE 16
#define SEC_DATA 18
#define SEC_DATA_MAX 494

#define SECTOR_META 0
#define SECTOR_ROOT 1
#define SECTOR_FREE 2
#define SECTOR_FIRST_DATA 3

#define ENTRY_SIZE 40
#define NAME_MAX 31
#define ENTRIES_PER_SECTOR (SEC_DATA_MAX / ENTRY_SIZE)

static uint8 tmpData[512] = {0};
static state tmpDirty = false, tmpValid = false;
static uint64 actualSector = 0, totalSectors = 0;
static void (*sswrite)(const uint64 sector, const uint8 *buf), (*ssread)(const uint64 sector, uint8 *buf);

static void flushStorage() {
	if (!tmpValid || !tmpDirty) return;
	sswrite(actualSector, tmpData);
	tmpDirty = false;
}
static void selectSector(const uint64 sector) {
	if (tmpValid && sector == actualSector) return;
	flushStorage();
	actualSector = sector;
	ssread(sector, tmpData);
	tmpValid = true;
}
static state setByte(const uint8 val, const uint64 sector, const uint16 byte) {
	if (byte >= 512) return false;
	selectSector(sector);
	tmpData[byte] = val;
	tmpDirty = true;
	return true;
}
static uint8 getByte(const uint64 sector, const uint16 byte) {
	if (byte >= 512) return 0;
	selectSector(sector);
	return tmpData[byte];
}
static void setAddr(const uint64 sector, const uint16 byte, const uint64 v) {
	if (byte >= 504) return;
	for (uint8 i = 0; i < 8; i++) setByte((uint8)(v >> (8 * (7 - i))), sector, byte + i);
}
static uint64 getAddr(const uint64 sector, const uint16 byte) {
	if (byte >= 504) return 0;
	uint64 v = 0;
	for (uint8 i = 0; i < 8; i++) v = (v << 8) | getByte(sector, byte + i);
	return v;
}
static void setNext(const uint64 sector, const uint64 next) {
	if (!sector) return;
	setAddr(sector, SEC_NEXT, next);
}
static uint64 getNext(const uint64 sector) {
	if (!sector) return 0;
	return getAddr(sector, SEC_NEXT);
}
static uint16 getSize(const uint64 sector) { return (uint16)((getByte(sector, SEC_SIZE) << 8) | getByte(sector, SEC_SIZE + 1)); }
static void setSize(const uint64 sector, const uint16 v) {
	setByte((uint8)(v >> 8), sector, SEC_SIZE);
	setByte((uint8)v, sector, SEC_SIZE + 1);
}
static void clearSector(const uint64 sector) {
	selectSector(sector);
	for (uint16 i = 0; i < 512; i++) tmpData[i] = 0;
	tmpDirty = true;
}
static void initSector(const uint64 sector) {
	clearSector(sector);
	setAddr(sector, SEC_LAST, sector);
}

static uint64 getFileCount() { return getAddr(SECTOR_META, 1); }
static void setFileCount(const uint64 v) { setAddr(SECTOR_META, 1, v); }

static void formatStorage() {
	clearSector(SECTOR_META);
	setByte(QUNETRA_OS_META, SECTOR_META, 0);
	setFileCount(0);
	initSector(SECTOR_ROOT);
	initSector(SECTOR_FREE);
	setAddr(SECTOR_FREE, SEC_DATA, SECTOR_FIRST_DATA);
	flushStorage();
}
static uint64 allocSector() {
	uint64 s = getNext(SECTOR_FREE);
	if (s) {
		const uint64 after = getNext(s);
		setNext(SECTOR_FREE, after);
	} else {
		s = getAddr(SECTOR_FREE, SEC_DATA);
		if (s >= totalSectors) return 0;
		setAddr(SECTOR_FREE, SEC_DATA, s + 1);
	}
	initSector(s);
	return s;
}
static void freeChain(const uint64 first) {
	const uint64 last = getAddr(first, SEC_LAST), head = getNext(SECTOR_FREE);
	setNext(last, head);
	setNext(SECTOR_FREE, first);
}
static uint64 appendSector(const uint64 first) {
	const uint64 n = allocSector();
	if (!n) return 0;
	const uint64 last = getAddr(first, SEC_LAST);
	setNext(last, n);
	setAddr(first, SEC_LAST, n);
	return n;
}
char* initStorage() {
	char* type = " ERROR ";
	scanPci();
	storageType = 0;
	if (haveAhci && ahci_init(ahciBus, ahciDevice, ahciFunction)) {
		storageType = 2;
		type = " AHCI / SATA ";
		sswrite = ahci_writeSector;
		ssread = ahci_readSector;
		totalSectors = ahci_getSectorCount();
	} else if (haveIde) {
		storageType = 1;
		type = " IDE / ATA ";
		sswrite = ata_writeSector;
		ssread = ata_readSector;
		totalSectors = ata_getSectorCount();
	} else if (haveAhci) return " AHCI: NO DISK ";
	if (storageType == 0 || totalSectors <= SECTOR_FIRST_DATA) {
		ssread = 0;
		sswrite = 0;
		return " ERROR ";
	}
	if (getByte(SECTOR_META, 0) != QUNETRA_OS_META) formatStorage();
	return type;
}
uint64 storageSectors() { return totalSectors; }
static state nameIs(const uint64 sector, const uint16 off, const char* name) {
	for (uint8 i = 0; i <= NAME_MAX; i++) {
		const uint8 c = getByte(sector, off + i);
		if (c != (uint8)name[i]) return false;
		if (c == '\0') return true;
	}
	return false;
}
static state findEntry(const uint64 catalog, const char* name, uint64* eSec, uint16* eOff) {
	for (uint64 s = catalog; s; s = getNext(s)) {
		for (uint8 i = 0; i < ENTRIES_PER_SECTOR; i++) {
			const uint16 off = SEC_DATA + i * ENTRY_SIZE;
			if (!getAddr(s, off)) continue;
			if (nameIs(s, off + 8, name)) {
				*eSec = s;
				*eOff = off;
				return true;
			}
		}
	}
	return false;
}
static state findFreeSlot(const uint64 catalog, uint64* eSec, uint16* eOff) {
	uint64 s = catalog;
	while (true) {
		for (uint8 i = 0; i < ENTRIES_PER_SECTOR; i++) {
			const uint16 off = SEC_DATA + i * ENTRY_SIZE;
			if (!getAddr(s, off)) {
				*eSec = s;
				*eOff = off;
				return true;
			}
		}
		const uint64 next = getNext(s);
		if (!next) break;
		s = next;
	}
	const uint64 n = appendSector(catalog);
	if (!n) return false;
	*eSec = n;
	*eOff = SEC_DATA;
	return true;
}
static uint64 realCatalog(const uint64 catalog) { return catalog ? catalog : SECTOR_ROOT; }

uint64 createFile(const uint64 catalog, const char* name) {
	if (!ssread || !name) return 0;
	const uint64 cat = realCatalog(catalog);
	const uint len = strLen(name);
	if (len == 0 || len > NAME_MAX) return 0;
	uint64 sec;
	uint16 off;
	if (findEntry(cat, name, &sec, &off)) return 0;
	if (!findFreeSlot(cat, &sec, &off)) return 0;
	const uint64 file = allocSector();
	if (!file) return 0;
	setAddr(sec, off, file);
	for (uint8 i = 0; i <= NAME_MAX; i++) setByte(i < len ? (uint8)name[i] : 0, sec, off + 8 + i);
	setFileCount(getFileCount() + 1);
	flushStorage();
	return file;
}
uint64 findFile(const uint64 catalog, const char* name) {
	if (!ssread || !name) return 0;
	uint64 sec;
	uint16 off;
	if (!findEntry(realCatalog(catalog), name, &sec, &off)) return 0;
	return getAddr(sec, off);
}
state deleteFile(const uint64 catalog, const char* name) {
	if (!ssread || !name) return false;
	uint64 sec;
	uint16 off;
	if (!findEntry(realCatalog(catalog), name, &sec, &off)) return false;
	const uint64 file = getAddr(sec, off);
	freeChain(file);
	setAddr(sec, off, 0);
	setFileCount(getFileCount() - 1);
	flushStorage();
	return true;
}
state fwriteb(const uint64 file, const uint8 v) {
	if (!ssread || file < SECTOR_FIRST_DATA) return false;
	uint64 last = getAddr(file, SEC_LAST);
	uint16 size = getSize(last);
	if (size >= SEC_DATA_MAX) {
		last = appendSector(file);
		if (!last) return false;
		size = 0;
	}
	setByte(v, last, SEC_DATA + size);
	setSize(last, size + 1);
	flushStorage();
	return true;
}
state kfwrite(const uint64 file, const char* value) {
	if (!ssread || file < SECTOR_FIRST_DATA || !value) return false;
	while (*value) {
		if (!fwriteb(file, *value)) return false;
		value++;
	}
	return true;
}
state freadb(const uint64 file, const uint64 pos, uint8* out) {
	if (!ssread || !out || file < SECTOR_FIRST_DATA) return false;
	uint64 s = file, skip = pos;
	while (s) {
		const uint16 size = getSize(s);
		if (skip < size) {
			*out = getByte(s, SEC_DATA + (uint16)skip);
			return true;
		}
		skip -= size;
		s = getNext(s);
	}
	return false;
}
uint64 fileSize(const uint64 file) {
	if (!ssread || file < SECTOR_FIRST_DATA) return 0;
	uint64 total = 0;
	for (uint64 s = file; s; s = getNext(s)) total += getSize(s);
	return total;
}
void listFiles(const uint64 catalog) {
	if (!ssread) return;
	const uint64 cat = realCatalog(catalog);
	char name[NAME_MAX + 1];
	for (uint64 s = cat; s; s = getNext(s)) {
		for (uint8 i = 0; i < ENTRIES_PER_SECTOR; i++) {
			const uint16 off = SEC_DATA + i * ENTRY_SIZE;
			const uint64 file_id = getAddr(s, off);
			name[0] = '\0';
			if (file_id != 0) {
				for (uint8 j = 0; j <= NAME_MAX; j++) name[j] = (char)getByte(s, off + 8 + j);
				name[NAME_MAX] = '\0';
				kprintf("> %s [ %p ]-[ %u B ]\n", name, file_id, fileSize(file_id));
			}
		}
	}
}
