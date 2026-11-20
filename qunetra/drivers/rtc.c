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

#include "../utility/types.h"
#include "../utility/cpu.h"
#include "rtc.h"

static uint8 cmosRead(const uint8 reg) {
	outb(0x70, reg);
	return inb(0x71);
}
static uint8 bcd(const uint8 v) { return (uint8)((v & 0x0F) + (v >> 4) * 10); }

state rtcGet(uint16* outYear, uint8* outMonth, uint8* outDay, uint8* outHour, uint8* outMin, uint8* outSec) {
	uint8 sec, min, hour, day, month, year, century, last[7];
	for (uint8 tries = 0; tries < 10; tries++) {
		for (uint32 t = 0; t < 100000 && (cmosRead(0x0A) & 0x80); t++) {}
		sec = cmosRead(0x00);
		min = cmosRead(0x02);
		hour = cmosRead(0x04);
		day = cmosRead(0x07);
		month = cmosRead(0x08);
		year = cmosRead(0x09);
		century = cmosRead(0x32);
		const uint8 now[7] = {sec, min, hour, day, month, year, century};
		state same = tries > 0;
		for (uint8 i = 0; i < 7; i++) {
			if (tries > 0 && last[i] != now[i]) same = false;
			last[i] = now[i];
		}
		if (same) break;
	}
	const uint8 statusB = cmosRead(0x0B);
	const state binary = (statusB & 0x04) != 0, pm = (hour & 0x80) != 0;
	if (!binary) {
		sec = bcd(sec);
		min = bcd(min);
		hour = bcd(hour & 0x7F);
		day = bcd(day);
		month = bcd(month);
		year = bcd(year);
		century = bcd(century);
	} else hour &= 0x7F;
	if (!(statusB & 0x02) && pm) hour = (uint8)((hour % 12) + 12);
	uint32 fullYear = 2000u + year;
	if (century >= 19 && century <= 30) fullYear = (uint32)century * 100 + year;
	if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 60) return false;
	*outYear = (uint16)fullYear;
	*outMonth = month;
	*outDay = day;
	*outHour = hour;
	*outMin = min;
	*outSec = sec;
	return true;
}
uint64 rtcNow() {
	uint16 y = 0;
	uint8 mo = 0, d = 0, h = 0, mi = 0, s = 0;
	if (!rtcGet(&y, &mo, &d, &h, &mi, &s)) return 0;
	return (uint64)y * 10000000000ull + (uint64)mo * 100000000ull + (uint64)d * 1000000ull + (uint64)h * 10000ull + (uint64)mi * 100ull + s;
}
