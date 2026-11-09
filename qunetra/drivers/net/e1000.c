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
#include "../pci.h"
#include "e1000.h"

#ifndef E1000_TIMEOUT
#define E1000_TIMEOUT 5000000
#endif
#ifndef E1000_LINK_TIMEOUT
#define E1000_LINK_TIMEOUT 3000000
#endif
#ifndef E1000_RESET_DELAY
#define E1000_RESET_DELAY 10000
#endif

#define REG_CTRL   0x0000
#define REG_STATUS 0x0008
#define REG_EERD   0x0014
#define REG_ICR    0x00C0
#define REG_IMC    0x00D8
#define REG_RCTL   0x0100
#define REG_TCTL   0x0400
#define REG_TIPG   0x0410
#define REG_RDBAL  0x2800
#define REG_RDBAH  0x2804
#define REG_RDLEN  0x2808
#define REG_RDH    0x2810
#define REG_RDT    0x2818
#define REG_TDBAL  0x3800
#define REG_TDBAH  0x3804
#define REG_TDLEN  0x3808
#define REG_TDH    0x3810
#define REG_TDT    0x3818
#define REG_MTA    0x5200
#define REG_RAL0   0x5400
#define REG_RAH0   0x5404

#define CTRL_ASDE   (1u << 5)
#define CTRL_SLU    (1u << 6)
#define CTRL_RST    (1u << 26)
#define STATUS_LU   (1u << 1)
#define RCTL_EN     (1u << 1)
#define RCTL_BAM    (1u << 15)
#define RCTL_SECRC  (1u << 26)
#define TCTL_EN     (1u << 1)
#define TCTL_PSP    (1u << 3)
#define TX_CMD_EOP  (1u << 0)
#define TX_CMD_IFCS (1u << 1)
#define TX_CMD_RS   (1u << 3)
#define DESC_DD     (1u << 0)
#define RX_EOP      (1u << 1)

#define RX_COUNT 16
#define TX_COUNT 8
#define BUF_SIZE 2048

typedef struct __attribute__((packed)) {
	uint32 addrLow, addrHigh;
	uint16 length, checksum;
	uint8 status, errors;
	uint16 special;
} rxDesc;
typedef struct __attribute__((packed)) {
	uint32 addrLow, addrHigh;
	uint16 length;
	uint8 cso, cmd, status, css;
	uint16 special;
} txDesc;

static volatile rxDesc rxRing[RX_COUNT] __attribute__((aligned(16)));
static volatile txDesc txRing[TX_COUNT] __attribute__((aligned(16)));
static uint8 rxBuf[RX_COUNT][BUF_SIZE] __attribute__((aligned(16))),
	txBuf[TX_COUNT][BUF_SIZE] __attribute__((aligned(16)));

static volatile uint32 *regs = 0;
static uint8 macAddr[6];
static state ready = false;
static uint16 rxCur = 0, txCur = 0;

#define REG(off) (regs[(off) / 4])

static inline void barrier() { __asm__ volatile ("" : : : "memory"); }
static state isSupported(const uint16 id) { return id == 0x100E || id == 0x100F || id == 0x1004 || id == 0x1008 || id == 0x1013 || id == 0x1019; }
static state readMac() {
	const uint32 ral = REG(REG_RAL0), rah = REG(REG_RAH0);
	if (ral != 0 || (rah & 0xFFFF) != 0) {
		macAddr[0] = (uint8)ral;
		macAddr[1] = (uint8)(ral >> 8);
		macAddr[2] = (uint8)(ral >> 16);
		macAddr[3] = (uint8)(ral >> 24);
		macAddr[4] = (uint8)rah;
		macAddr[5] = (uint8)(rah >> 8);
		return true;
	}
	for (uint8 i = 0; i < 3; i++) {
		REG(REG_EERD) = 1u | ((uint32)i << 8);
		uint32 v = 0;
		state done = false;
		for (uint32 t = 0; t < E1000_TIMEOUT; t++) {
			v = REG(REG_EERD);
			if (v & (1u << 4)) {
				done = true;
				break;
			}
		}
		if (!done) return false;
		macAddr[i * 2] = (uint8)(v >> 16);
		macAddr[i * 2 + 1] = (uint8)(v >> 24);
	}
	return true;
}
state e1000_init() {
	ready = false;
	state found = false;
	uint8 fBus = 0, fDev = 0, fFn = 0;
	for (uint8 bus = 0; bus < 8 && !found; bus++) {
		for (uint8 device = 0; device < 32 && !found; device++) {
			for (uint8 function = 0; function < 8; function++) {
				const uint32 id = pciRead32(bus, device, function, 0x00);
				if ((id & 0xFFFF) != 0x8086 || !isSupported((uint16)(id >> 16))) continue;
				fBus = bus;
				fDev = device;
				fFn = function;
				found = true;
				break;
			}
		}
	}
	if (!found) return false;
	const uint32 cmdReg = pciRead32(fBus, fDev, fFn, 0x04);
	pciWrite32(fBus, fDev, fFn, 0x04, (cmdReg & 0xFFFFu) | 0x6u);
	const uint32 bar0 = pciRead32(fBus, fDev, fFn, 0x10);
	if ((bar0 & 1) || (bar0 & ~0xFu) == 0) return false;
	regs = (volatile uint32 *)(uiptr)(bar0 & ~0xFu);
	REG(REG_IMC) = 0xFFFFFFFFu;
	REG(REG_RCTL) = 0;
	REG(REG_TCTL) = 0;
	REG(REG_CTRL) |= CTRL_RST;
	for (uint32 i = 0; i < E1000_RESET_DELAY; i++) ioWait();
	state resetDone = false;
	for (uint32 t = 0; t < E1000_TIMEOUT; t++) {
		if (!(REG(REG_CTRL) & CTRL_RST)) {
			resetDone = true;
			break;
		}
	}
	if (!resetDone) return false;
	REG(REG_IMC) = 0xFFFFFFFFu;
	(void)REG(REG_ICR);
	REG(REG_CTRL) = (REG(REG_CTRL) | CTRL_SLU | CTRL_ASDE) & ~((1u << 3) | (1u << 7));

	if (!readMac()) return false;
	REG(REG_RAL0) = (uint32)macAddr[0] | ((uint32)macAddr[1] << 8) | ((uint32)macAddr[2] << 16) | ((uint32)macAddr[3] << 24);
	REG(REG_RAH0) = (uint32)macAddr[4] | ((uint32)macAddr[5] << 8) | (1u << 31);
	for (uint8 i = 0; i < 128; i++) REG(REG_MTA + i * 4) = 0;
	for (uint8 i = 0; i < RX_COUNT; i++) {
		rxRing[i].addrLow = (uint32)(uiptr)rxBuf[i];
		rxRing[i].addrHigh = 0;
		rxRing[i].length = 0;
		rxRing[i].checksum = 0;
		rxRing[i].status = 0;
		rxRing[i].errors = 0;
		rxRing[i].special = 0;
	}
	REG(REG_RDBAL) = (uint32)(uiptr)rxRing;
	REG(REG_RDBAH) = 0;
	REG(REG_RDLEN) = RX_COUNT * sizeof(rxDesc);
	REG(REG_RDH) = 0;
	REG(REG_RDT) = RX_COUNT - 1;
	rxCur = 0;
	for (uint8 i = 0; i < TX_COUNT; i++) {
		txRing[i].addrLow = (uint32)(uiptr)txBuf[i];
		txRing[i].addrHigh = 0;
		txRing[i].length = 0;
		txRing[i].cso = 0;
		txRing[i].cmd = 0;
		txRing[i].status = DESC_DD;
		txRing[i].css = 0;
		txRing[i].special = 0;
	}
	REG(REG_TDBAL) = (uint32)(uiptr)txRing;
	REG(REG_TDBAH) = 0;
	REG(REG_TDLEN) = TX_COUNT * sizeof(txDesc);
	REG(REG_TDH) = 0;
	REG(REG_TDT) = 0;
	txCur = 0;
	barrier();
	REG(REG_TIPG) = 0x0060200Au;
	REG(REG_TCTL) = TCTL_EN | TCTL_PSP | (0x0Fu << 4) | (0x40u << 12);
	REG(REG_RCTL) = RCTL_EN | RCTL_BAM | RCTL_SECRC;
	ready = true;
	for (uint32 t = 0; t < E1000_LINK_TIMEOUT; t++) if (REG(REG_STATUS) & STATUS_LU) break;
	return true;
}
const uint8* e1000_mac() { return macAddr; }
state e1000_linkUp() { return ready && (REG(REG_STATUS) & STATUS_LU); }
state e1000_send(const uint8* frame, const uint16 len) {
	if (!ready || !frame || len < 14 || len > E1000_MAX_FRAME) return false;
	volatile txDesc *d = &txRing[txCur];
	state free = false;
	for (uint32 t = 0; t < E1000_TIMEOUT; t++) {
		if (d->status & DESC_DD) {
			free = true;
			break;
		}
	}
	if (!free) return false;
	for (uint16 i = 0; i < len; i++) txBuf[txCur][i] = frame[i];
	d->addrLow = (uint32)(uiptr)txBuf[txCur];
	d->addrHigh = 0;
	d->length = len;
	d->cso = 0;
	d->css = 0;
	d->special = 0;
	d->cmd = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;
	d->status = 0;
	txCur = (txCur + 1) % TX_COUNT;
	barrier();
	REG(REG_TDT) = txCur;
	for (uint32 t = 0; t < E1000_TIMEOUT; t++) if (d->status & DESC_DD) return true;
	return false;
}
uint16 e1000_recv(uint8* buf, const uint16 maxLen) {
	if (!ready || !buf) return 0;
	volatile rxDesc *d = &rxRing[rxCur];
	if (!(d->status & DESC_DD)) return 0;
	barrier();
	const uint16 len = d->length;
	uint16 out = 0;
	if ((d->status & RX_EOP) && d->errors == 0 && len > 0 && len <= BUF_SIZE) {
		out = len < maxLen ? len : maxLen;
		for (uint16 i = 0; i < out; i++) buf[i] = rxBuf[rxCur][i];
	}
	d->status = 0;
	d->errors = 0;
	barrier();
	REG(REG_RDT) = rxCur;
	rxCur = (rxCur + 1) % RX_COUNT;
	return out;
}
