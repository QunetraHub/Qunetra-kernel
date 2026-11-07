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
#include "ahci.h"

#define HBA_GHC 0x04
#define HBA_IS  0x08
#define HBA_PI  0x0C
#define GHC_AE  (1u << 31)

#define PX_CLB  0x00
#define PX_CLBU 0x04
#define PX_FB   0x08
#define PX_FBU  0x0C
#define PX_IS   0x10
#define PX_IE   0x14
#define PX_CMD  0x18
#define PX_TFD  0x20
#define PX_SIG  0x24
#define PX_SSTS 0x28
#define PX_SERR 0x30
#define PX_CI   0x38

#define CMD_ST  (1u << 0)
#define CMD_FRE (1u << 4)
#define CMD_FR  (1u << 14)
#define CMD_CR  (1u << 15)
#define TFD_ERR (1u << 0)
#define TFD_DRQ (1u << 3)
#define TFD_BSY (1u << 7)
#define IS_TFES (1u << 30)
#define SIG_SATA 0x00000101u

#define ATA_IDENTIFY 0xEC
#define ATA_READ_DMA_EXT 0x25
#define ATA_WRITE_DMA_EXT 0x35
#define ATA_FLUSH_EXT 0xEA

#ifndef AHCI_TIMEOUT
#define AHCI_TIMEOUT 500000
#endif
#ifndef AHCI_CMD_TIMEOUT
#define AHCI_CMD_TIMEOUT 50000000
#endif

typedef struct __attribute__((packed)) { uint32 dw0, prdbc, ctba, ctbau, reserved[4]; } cmdHeader;
typedef struct __attribute__((packed)) { uint32 dba, dbau, reserved, dbc; } prdtEntry;
typedef struct __attribute__((packed)) {
	uint8 cfis[64], acmd[16], reserved[48];
	prdtEntry prdt[1];
} cmdTable;

static cmdHeader cmdList[32] __attribute__((aligned(1024)));
static uint8 recvFis[256] __attribute__((aligned(256)));
static cmdTable table __attribute__((aligned(128)));
static uint8 dmaBuf[512] __attribute__((aligned(512)));

static volatile uint32 *hba = 0;
static uint8 port = 0;
static state ready = false;

#define HBA(off) (hba[(off) / 4])
#define PORTN(p, off) (hba[(0x100 + (p) * 0x80 + (off)) / 4])
#define PORT(off) PORTN(port, off)

static inline void barrier() { __asm__ volatile ("" : : : "memory"); }

static uint32 pciRead(const uint8 bus, const uint8 device, const uint8 function, const uint8 offset) {
	outl(0xCF8, 0x80000000u | ((uint32)bus << 16) | ((uint32)(device & 0x1F) << 11) | ((uint32)(function & 0x07) << 8) | (offset & 0xFC));
	return inl(0xCFC);
}
static void pciWrite(const uint8 bus, const uint8 device, const uint8 function, const uint8 offset, const uint32 v) {
	outl(0xCF8, 0x80000000u | ((uint32)bus << 16) | ((uint32)(device & 0x1F) << 11) | ((uint32)(function & 0x07) << 8) | (offset & 0xFC));
	outl(0xCFC, v);
}

static state waitClear(volatile uint32 *reg, const uint32 mask) {
	for (uint32 t = 0; t < AHCI_TIMEOUT; t++) if (!(*reg & mask)) return true;
	return false;
}
static state portStop() {
	PORT(PX_CMD) &= ~CMD_ST;
	if (!waitClear(&PORT(PX_CMD), CMD_CR)) return false;
	PORT(PX_CMD) &= ~CMD_FRE;
	return waitClear(&PORT(PX_CMD), CMD_FR);
}
static state portStart() {
	if (!waitClear(&PORT(PX_TFD), TFD_BSY | TFD_DRQ)) return false;
	PORT(PX_CMD) |= CMD_FRE;
	PORT(PX_CMD) |= CMD_ST;
	return true;
}
static void portClearStatus() {
	PORT(PX_SERR) = 0xFFFFFFFFu;
	PORT(PX_IS) = 0xFFFFFFFFu;
	waitClear(&PORT(PX_IS), 0xFFFFFFFFu);
}
static void portRecover() {
	if (!portStop()) {
		ready = false;
		return;
	}
	portClearStatus();
	if (!portStart()) ready = false;
}

static state ahciCommand(const uint8 cmd, const uint64 lba, const uint16 count, const uint8 dir) {
	if (!ready) return false;
	if (!waitClear(&PORT(PX_TFD), TFD_BSY | TFD_DRQ)) return false;
	cmdList[0].dw0 = 5u | (dir == 2 ? (1u << 6) : 0u) | (dir ? (1u << 16) : 0u);
	cmdList[0].prdbc = 0;
	cmdList[0].ctba = (uint32)(uiptr)&table;
	cmdList[0].ctbau = 0;
	for (uint8 i = 0; i < 64; i++) table.cfis[i] = 0;
	table.cfis[0] = 0x27;
	table.cfis[1] = 0x80;
	table.cfis[2] = cmd;
	table.cfis[4] = (uint8)lba;
	table.cfis[5] = (uint8)(lba >> 8);
	table.cfis[6] = (uint8)(lba >> 16);
	table.cfis[7] = 1 << 6;
	table.cfis[8] = (uint8)(lba >> 24);
	table.cfis[9] = (uint8)(lba >> 32);
	table.cfis[10] = (uint8)(lba >> 40);
	table.cfis[12] = (uint8)count;
	table.cfis[13] = (uint8)(count >> 8);
	if (dir) {
		table.prdt[0].dba = (uint32)(uiptr)dmaBuf;
		table.prdt[0].dbau = 0;
		table.prdt[0].reserved = 0;
		table.prdt[0].dbc = 512 - 1;
	}
	portClearStatus();
	barrier();
	PORT(PX_CI) = 1;
	state done = false;
	for (uint32 t = 0; t < AHCI_CMD_TIMEOUT; t++) {
		if (PORT(PX_IS) & IS_TFES) break;
		if (!(PORT(PX_CI) & 1)) {
			done = true;
			break;
		}
	}
	barrier();
	if (!done || (PORT(PX_IS) & IS_TFES) || (PORT(PX_TFD) & TFD_ERR)) {
		portRecover();
		return false;
	}
	return true;
}

state ahci_init(const uint8 bus, const uint8 device, const uint8 function) {
	ready = false;
	const uint32 cmdReg = pciRead(bus, device, function, 0x04);
	pciWrite(bus, device, function, 0x04, (cmdReg & 0xFFFFu) | 0x6u);
	const uint32 bar5 = pciRead(bus, device, function, 0x24);
	if ((bar5 & 1) || (bar5 & ~0xFu) == 0) return false;
	hba = (volatile uint32 *)(uiptr)(bar5 & ~0xFu);
	HBA(HBA_GHC) |= GHC_AE;
	const uint32 pi = HBA(HBA_PI);
	state found = false;
	for (uint8 p = 0; p < 32; p++) {
		if (!(pi & (1u << p))) continue;
		const uint32 ssts = PORTN(p, PX_SSTS);
		if ((ssts & 0xF) != 3 || ((ssts >> 8) & 0xF) != 1) continue;
		if (PORTN(p, PX_SIG) != SIG_SATA) continue;
		port = p;
		found = true;
		break;
	}
	if (!found) return false;
	if (!portStop()) return false;
	for (uint8 i = 0; i < 32; i++) {
		cmdList[i].dw0 = 0;
		cmdList[i].prdbc = 0;
		cmdList[i].ctba = 0;
		cmdList[i].ctbau = 0;
	}
	PORT(PX_CLB) = (uint32)(uiptr)cmdList;
	PORT(PX_CLBU) = 0;
	PORT(PX_FB) = (uint32)(uiptr)recvFis;
	PORT(PX_FBU) = 0;
	PORT(PX_IE) = 0;
	portClearStatus();
	HBA(HBA_IS) = 0xFFFFFFFFu;
	if (!portStart()) return false;
	ready = true;
	return true;
}

void ahci_readSector(const uint64 lba, uint8 *buf) {
	if (!ahciCommand(ATA_READ_DMA_EXT, lba, 1, 1)) return;
	for (uint16 i = 0; i < 512; i++) buf[i] = dmaBuf[i];
}
void ahci_writeSector(const uint64 lba, const uint8 *buf) {
	for (uint16 i = 0; i < 512; i++) dmaBuf[i] = buf[i];
	if (!ahciCommand(ATA_WRITE_DMA_EXT, lba, 1, 2)) return;
	ahciCommand(ATA_FLUSH_EXT, 0, 0, 0);
}
static uint16 idWord(const uint8 i) { return (uint16)(dmaBuf[i * 2] | (dmaBuf[i * 2 + 1] << 8)); }
uint64 ahci_getSectorCount() {
	if (!ahciCommand(ATA_IDENTIFY, 0, 0, 1)) return 0;
	if (!(idWord(83) & (1u << 10))) return 0; // no 48-bit LBA
	return ((uint64)idWord(103) << 48) | ((uint64)idWord(102) << 32) | ((uint64)idWord(101) << 16) | idWord(100);
}
