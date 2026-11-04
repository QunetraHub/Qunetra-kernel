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

#define ATA_PRIMARY_IO      0x1F0
#define ATA_REG_DATA        (ATA_PRIMARY_IO + 0)
#define ATA_REG_FEATURES    (ATA_PRIMARY_IO + 1)
#define ATA_REG_SECCOUNT    (ATA_PRIMARY_IO + 2)
#define ATA_REG_LBA_LOW     (ATA_PRIMARY_IO + 3)
#define ATA_REG_LBA_MID     (ATA_PRIMARY_IO + 4)
#define ATA_REG_LBA_HIGH    (ATA_PRIMARY_IO + 5)
#define ATA_REG_DEV_SEL     (ATA_PRIMARY_IO + 6)
#define ATA_REG_COMMAND     (ATA_PRIMARY_IO + 7)
#define ATA_REG_STATUS      (ATA_PRIMARY_IO + 7)
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_READ_EXT    0x24
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_WRITE_EXT   0x34
#define ATA_CMD_CACHE_FLUSH 0xE7

static state ata_supports_lba48 = false;
static int8_t ata_wait(uint8 bit_mask, uint8 expected, uint32 timeout) {
	while (timeout > 0) {
		uint8 status = inb(ATA_REG_STATUS);
		if (status & 0x01 || status & 0x20) return -1;
		if (!(status & 0x80) && (status & bit_mask) == expected) return 0;
		timeout--;
	}
	return -1;
}
void ata_readSector(const uint64 lba, uint8 *buf) {
	if (ata_supports_lba48) {
		outb(ATA_REG_DEV_SEL, 0x40);
		for (uint8_t i = 0; i < 4; i++) inb(ATA_REG_STATUS);
		if (ata_wait(0x40, 0x40, 100000) != 0) return;
		outb(ATA_REG_FEATURES, 0x00);
		outb(ATA_REG_FEATURES, 0x00);
		outb(ATA_REG_SECCOUNT, 0x00);
		outb(ATA_REG_SECCOUNT, 0x01);
		outb(ATA_REG_LBA_LOW,  (uint8)(lba >> 24));
		outb(ATA_REG_LBA_LOW,  (uint8)(lba));
		outb(ATA_REG_LBA_MID,  (uint8)(lba >> 32));
		outb(ATA_REG_LBA_MID,  (uint8)(lba >> 8));
		outb(ATA_REG_LBA_HIGH, (uint8)(lba >> 40));
		outb(ATA_REG_LBA_HIGH, (uint8)(lba >> 16));
		outb(ATA_REG_COMMAND, ATA_CMD_READ_EXT);
	} else {
		outb(ATA_REG_DEV_SEL, 0xE0 | ((lba >> 24) & 0x0F));
		for (uint8_t i = 0; i < 4; i++) inb(ATA_REG_STATUS);
		if (ata_wait(0x40, 0x40, 100000) != 0) return;
		outb(ATA_REG_FEATURES, 0x00);
		outb(ATA_REG_SECCOUNT, 1);
		outb(ATA_REG_LBA_LOW,  (uint8)(lba));
		outb(ATA_REG_LBA_MID,  (uint8)(lba >> 8));
		outb(ATA_REG_LBA_HIGH, (uint8)(lba >> 16));
		outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);
	}
	if (ata_wait(0x08, 0x08, 100000) != 0) return;
	insw(ATA_REG_DATA, buf, 256);
}
void ata_writeSector(const uint64 lba, const uint8 *buf) {
	if (ata_supports_lba48) {
		outb(ATA_REG_DEV_SEL, 0x40);
		for (uint8 i = 0; i < 4; i++) inb(ATA_REG_STATUS);
		if (ata_wait(0x40, 0x40, 100000) != 0) return;
		outb(ATA_REG_FEATURES, 0x00);
		outb(ATA_REG_FEATURES, 0x00);
		outb(ATA_REG_SECCOUNT, 0x00);
		outb(ATA_REG_SECCOUNT, 0x01);
		outb(ATA_REG_LBA_LOW,  (uint8)(lba >> 24));
		outb(ATA_REG_LBA_LOW,  (uint8)(lba));
		outb(ATA_REG_LBA_MID,  (uint8)(lba >> 32));
		outb(ATA_REG_LBA_MID,  (uint8)(lba >> 8));
		outb(ATA_REG_LBA_HIGH, (uint8)(lba >> 40));
		outb(ATA_REG_LBA_HIGH, (uint8)(lba >> 16));
		outb(ATA_REG_COMMAND, ATA_CMD_WRITE_EXT);
	} else {
		outb(ATA_REG_DEV_SEL, 0xE0 | ((lba >> 24) & 0x0F));
		for (uint8 i = 0; i < 4; i++) inb(ATA_REG_STATUS);
		if (ata_wait(0x40, 0x40, 100000) != 0) return;
		outb(ATA_REG_FEATURES, 0x00);
		outb(ATA_REG_SECCOUNT, 1);
		outb(ATA_REG_LBA_LOW,  (uint8)(lba));
		outb(ATA_REG_LBA_MID,  (uint8)(lba >> 8));
		outb(ATA_REG_LBA_HIGH, (uint8)(lba >> 16));
		outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
	}
	if (ata_wait(0x08, 0x08, 100000) != 0) return;
	outsw(ATA_REG_DATA, buf, 256);
	outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
	ata_wait(0x40, 0x40, 100000);
}
uint64 ata_getSectorCount() {
	outb(ATA_REG_DEV_SEL, 0xE0);
	for (uint8 i = 0; i < 4; i++) inb(ATA_REG_STATUS);
	outb(ATA_REG_SECCOUNT, 0);
	outb(ATA_REG_LBA_LOW, 0);
	outb(ATA_REG_LBA_MID, 0);
	outb(ATA_REG_LBA_HIGH, 0);
	outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
	if (inb(ATA_REG_STATUS) == 0) return 0;
	if (ata_wait(0x08, 0x08, 100000) != 0) return 0;
	uint16 buf[256];
	insw(ATA_REG_DATA, buf, 256);
	if (buf[83] & (1 << 10)) {
		ata_supports_lba48 = true;
		uint64 total_sectors = ((uint64)buf[103] << 48) |
		((uint64)buf[102] << 32) |
		((uint64)buf[101] << 16) |
		(uint64)buf[100];
		return total_sectors;
	} else {
		ata_supports_lba48 = false;
		uint32 total_sectors = ((uint32)buf[61] << 16) | (uint32)buf[60];
		return (uint64)total_sectors;
	}
}
