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

#include "../drivers/timer.h"
#include "../utility/types.h"
#include "../utility/cpu.h"
#include "../drivers/lfb.h"
#include "interrupts.h"
#include "mouse.h"

#define PS2_DATA 0x60
#define PS2_CMD  0x64
#define ST_OUT   0x01
#define ST_IN    0x02
#define ST_AUX   0x20
#define EVENTS   64

extern void irq12_stub();
static inline uint32 irqSave() {
	uint32 f;
	__asm__ volatile ("pushf; pop %0; cli" : "=r"(f) : : "memory");
	return f;
}
static inline void irqRestore(const uint32 f) { __asm__ volatile ("push %0; popf" : : "r"(f) : "memory", "cc"); }
static inline void barrier() { __asm__ volatile ("" : : : "memory"); }

static mouseEvent queue[EVENTS];
static volatile uint8 qHead = 0, qTail = 0;
static uint8 deviceId = 0, pkt[4], pktLen = 0, pktNeed = 3;
static uint64 pktDeadline = 0;
static volatile uint8 buttons = 0;
static volatile uint32 posX = 0, posY = 0;
static volatile int16 wheelSum = 0;
static uint32 boundW = 0, boundH = 0;

static state waitInputFree(const uint32 ms) {
	const uint64 dl = timerDeadline(ms);
	do { if (!(inb(PS2_CMD) & ST_IN)) return true;
	} while (!timerExpired(dl));
	return false;
}
static state readByte(uint8* v, const uint32 ms, const state fromMouse) {
	const uint64 dl = timerDeadline(ms);
	do {
		const uint8 st = inb(PS2_CMD);
		if (st & ST_OUT) {
			const uint8 b = inb(PS2_DATA);
			if (fromMouse && !(st & ST_AUX)) continue;
			*v = b;
			return true;
		}
	} while (!timerExpired(dl));
	return false;
}
static void flushOutput() { for (uint8 i = 0; i < 32 && (inb(PS2_CMD) & ST_OUT); i++) inb(PS2_DATA); }
static state ctrlCommand(const uint8 cmd) {
	if (!waitInputFree(100)) return false;
	outb(PS2_CMD, cmd);
	return true;
}
static state ctrlData(const uint8 v) {
	if (!waitInputFree(100)) return false;
	outb(PS2_DATA, v);
	return true;
}
static state mouseSend(const uint8 b) {
	for (uint8 attempt = 0; attempt < 3; attempt++) {
		if (!ctrlCommand(0xD4) || !ctrlData(b)) return false;
		uint8 r;
		if (!readByte(&r, 200, true)) return false;
		if (r == 0xFA) return true;
		if (r != 0xFE) return false;
	}
	return false;
}
static state setRate(const uint8 rate) { return mouseSend(0xF3) && mouseSend(rate); }
static state readId(uint8* id) { return mouseSend(0xF2) && readByte(id, 200, true); }
state initMouse() {
	initTimer();
	const uint32 flags = irqSave();
	state ok = false;
	uint8 config = 0, original = 0, id = 0, b;
	flushOutput();
	if (!ctrlCommand(0xA8)) goto done;
	if (!ctrlCommand(0x20) || !readByte(&original, 100, false)) goto done;
	config = (uint8)((original | 0x02) & ~0x20);
	if (!ctrlCommand(0x60) || !ctrlData(config)) goto done;
	flushOutput();
	if (!mouseSend(0xFF)) goto restore;
	if (!readByte(&b, 1000, true) || b != 0xAA) goto restore;
	if (!readByte(&b, 300, true)) goto restore;
	if (!mouseSend(0xF6)) goto restore;
	if (!setRate(200) || !setRate(100) || !setRate(80) || !readId(&id)) goto restore;
	if (id == 3) {
		uint8 id2 = 0;
		if (setRate(200) && setRate(200) && setRate(80) && readId(&id2) && id2 == 4) id = 4;
	}
	if (!setRate(100) || !mouseSend(0xF4)) goto restore;
	flushOutput();
	deviceId = id;
	pktNeed = id >= 3 ? 4 : 3;
	pktLen = 0;
	qHead = qTail = 0;
	buttons = 0;
	wheelSum = 0;
	idtSetGate(0x2C, (uint32)(uiptr)irq12_stub);
	outb(0x21, (uint8)(inb(0x21) & ~0x04));
	outb(0xA1, (uint8)(inb(0xA1) & ~0x10));
	ok = true;
	goto done;
restore:
	ctrlCommand(0x60);
	ctrlData(original);
	ctrlCommand(0xA7);
done:
	irqRestore(flags);
	return ok;
}
state mouseHasWheel() { return deviceId >= 3; }
static void pushEvent(const int16 dx, const int16 dy, const int16 wheel) {
	const uint8 next = (uint8)((qHead + 1) % EVENTS);
	if (next == qTail) return;
	queue[qHead].dx = dx;
	queue[qHead].dy = dy;
	queue[qHead].wheel = (int8)wheel;
	queue[qHead].buttons = buttons;
	barrier();
	qHead = next;
}
static void handlePacket() {
	const uint8 flags = pkt[0];
	int16 dx = 0, dy = 0, wheel = 0;
	if (!(flags & 0xC0)) {
		dx = pkt[1] - ((flags & 0x10) ? 256 : 0);
		dy = -(pkt[2] - ((flags & 0x20) ? 256 : 0));
	}
	uint8 b = flags & 0x07;
	if (deviceId == 3) wheel = -(int8)pkt[3];
	else if (deviceId == 4) {
		int z = pkt[3] & 0x0F;
		if (z >= 8) z -= 16;
		wheel = -z;
		b |= (uint8)(((pkt[3] >> 4) & 0x03) << 3);
	}
	buttons = b;
	int32 x = posX + dx, y = posY + dy;
	if (boundW) x = x < 0 ? 0 : x > (int32)boundW - 1 ? (int32)boundW - 1 : x;
	if (boundH) y = y < 0 ? 0 : y > (int32)boundH - 1 ? (int32)boundH - 1 : y;
	posX = x;
	posY = y;
	wheelSum += wheel;
	pushEvent(dx, dy, wheel);
}
void mouseIrqHandler() {
	const uint8 st = inb(PS2_CMD);
	if ((st & (ST_OUT | ST_AUX)) == (ST_OUT | ST_AUX)) {
		const uint8 b = inb(PS2_DATA);
		if (pktLen > 0 && timerExpired(pktDeadline)) pktLen = 0;
		if (pktLen == 0 && !(b & 0x08)) {}
		else {
			if (pktLen == 0) pktDeadline = timerDeadline(100);
			pkt[pktLen++] = b;
			if (pktLen >= pktNeed) {
				pktLen = 0;
				handlePacket();
			}
		}
	}
	outb(0xA0, 0x20);
	outb(0x20, 0x20);
}
int mouseGet(mouseEvent* e) {
	if (qHead == qTail) return 0;
	*e = queue[qTail];
	barrier();
	qTail = (uint8)((qTail + 1) % EVENTS);
	return 1;
}
uint8 mouseButtons() { return buttons; }
void mouseSetBounds(const uint32 width, const uint32 height) {
	const uint32 f = irqSave();
	boundW = width;
	boundH = height;
	posX = width ? (int32)width / 2 : 0;
	posY = height ? (int32)height / 2 : 0;
	irqRestore(f);
}
void mouseGetPosition(int32* x, int32* y) {
	const uint32 f = irqSave();
	*x = posX;
	*y = posY;
	irqRestore(f);
}
void mouseSetPosition(const int32 x, const int32 y) {
	const uint32 f = irqSave();
	posX = x;
	posY = y;
	irqRestore(f);
}
int32 mouseTakeWheel() {
	const uint32 f = irqSave();
	const int16 v = wheelSum;
	wheelSum = 0;
	irqRestore(f);
	return v;
}
#define CURSOR_W 11
#define CURSOR_H 18
static const int8 cursor[] = {
	1, -1,
	1, -2,
	1, -3,
	1, -4,
	1, -5,
	1, -6,
	1, -7,
	1, -8,
	1, -9,
	1, -10,
	1, -11,
	1, -11,
	1, -8,
	3, -4,1,-4,
	3, -3,2,-4,
	3, -2,4,-4,
	2, 6,-4,
	2, 7,-2
};
static uint32 oldPx[CURSOR_W * CURSOR_H], oldX = 0, oldY = 0;
static state isFirst = true, drawCur = false;
static void savePx() { for (uint32 y = 0; y < CURSOR_H; y++) for (uint32 x = 0; x < CURSOR_W; x++) oldPx[y * CURSOR_W + x] = getPx(posX + x, posY + y); }
void setDrawCursor(const state draw) { drawCur = draw; }
void eraseCursor() { for (uint32 y = 0; y < CURSOR_H; y++) for (uint32 x = 0; x < CURSOR_W; x++) putPx(oldX + x, oldY + y, oldPx[y * CURSOR_W + x]); }
void drawCursorRaw() {
	if (!drawCur) return;
	eraseCursor();
	savePx();
	uint8 c = 1, l = cursor[0], y = 0;
	while (c < sizeof(cursor)) {
		uint8 x = 0;
		for (uint8 i = 0; i < l; i++) {
			if (cursor[c] < 0) {
				drawRect(posX + x, posY + y, -cursor[c], 1, 0xFFFFFF);
				x += -cursor[c];
			} else x += cursor[c];
			c++;
		}
		l = cursor[c];
		y++;
		c++;
	}
	oldX = posX;
	oldY = posY;
}
void drawCursor() {
	if (!drawCur) return;
	if (isFirst) {
		savePx();
		isFirst = false;
	}
	if (oldX != posX || oldY != posY) drawCursorRaw();
}
