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

#include "types.h"
#include "math.h"
#include <stdarg.h>

uint strLen(const char* str) {
	if (!str) return 0;
	uint i = 0;
	while (str[i] != '\0') i++;
	return i;
}
state strIs(const char* strA, const char* strB) {
	if (!strA || !strB) return strA == strB;
	while (*strA != '\0' && (*strA == *strB)) {
		strA++;
		strB++;
	}
	return *strA == *strB;
}
void strCopy(char* dest, const char* src) {
	if (!dest || !src) return;
	while (*src != '\0') {
		*dest = *src;
		dest++;
		src++;
	}
	*dest = '\0';
}
void strShiftRight(char* str, const uint size, const uint shift) {
	if (!str || size == 0 || shift == 0) return;
	uint len = strLen(str);
	if (len > size - 1) len = size - 1;
	for (int i = len; i >= 0; i--) {
		const uint new_pos = i + shift;
		if (new_pos < size - 1) str[new_pos] = str[i];
	}
	for (uint i = 0; i < shift && i < size - 1; i++) str[i] = ' ';
	str[size - 1] = '\0';
}
void strShiftLeft(char* str, const uint size, const uint shift) {
	if (!str || size == 0 || shift == 0) return;
	uint len = strLen(str);
	if (len > size - 1) len = size - 1;
	if (shift >= len) {
		str[0] = '\0';
		return;
	}
	uint idx = 0;
	for (uint i = shift; i < len; i++) str[idx++] = str[i];
	str[idx] = '\0';
}
state strStartWith(const char* str, const char* prefix) {
	if (!str || !prefix) return false;
	while (*prefix != '\0') {
		if (*str != *prefix) return false;
		str++;
		prefix++;
	}
	return true;
}
state strEndWith(const char* str, const char* suffix) {
	if (!str || !suffix) return false;
	uint lenStr = strLen(str), lenSuff = strLen(suffix);
	if (lenSuff > lenStr) return false;
	const char* pStr = str + (lenStr - lenSuff);
	return strIs(pStr, suffix);
}
state strContains(const char* str, const char* sub) {
	if (!str || !sub) return false;
	if (*sub == '\0') return true;
	while (*str != '\0') {
		const char *h = str, *n = sub;
		while (*h && *n && *h == *n) {
			h++;
			n++;
		}
		if (*n == '\0') return true;
		str++;
	}
	return false;
}
int strFind(const char* str, const char c) {
	if (!str) return -1;
	for (uint i = 0; str[i] != '\0'; i++) if (str[i] == c) return i;
	return -1;
}
void strUpper(char* str) {
	if (!str) return;
	for (uint i = 0; str[i] != '\0'; i++) if (str[i] >= 'a' && str[i] <= 'z') str[i] -= 32;
}
void strLower(char* str) {
	if (!str) return;
	for (uint i = 0; str[i] != '\0'; i++) if (str[i] >= 'A' && str[i] <= 'Z') str[i] += 32;
}
void strTrimStartChar(char* str, const char c) {
	if (!str) return;
	uint i = 0;
	while (str[i] != '\0' && str[i] == c) i++;
	if (i == 0) return;
	uint j = 0;
	while (str[i + j] != '\0') {
		str[j] = str[i + j];
		j++;
	}
	str[j] = '\0';
}
void strTrimEndChar(char* str, const char c) {
	if (!str) return;
	uint len = strLen(str);
	while (len > 0 && str[len - 1] == c) len--;
	str[len] = '\0';
}
void strTrimStart(char* str) {
	if (!str) return;
	uint i = 0;
	while (str[i] != '\0' && (str[i] == ' ' || str[i] == '\n' || str[i] == '\t' || str[i] == '\r')) i++;
	if (i == 0) return;
	uint j = 0;
	while (str[i + j] != '\0') {
		str[j] = str[i + j];
		j++;
	}
	str[j] = '\0';
}
void strTrimEnd(char* str) {
	if (!str) return;
	uint len = strLen(str);
	while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\n' || str[len - 1] == '\t' || str[len - 1] == '\r')) len--;
	str[len] = '\0';
}
void strTrim(char* str) {
	strTrimStart(str);
	strTrimEnd(str);
}
void strAddChar(char* buf, const uint size, uint *index, const char c) {
	if (!buf || size == 0 || !index) return;
	if (*index < size - 1) {
		buf[(*index)++] = c;
		buf[*index] = '\0';
	}
}
void strAddStr(char *buf, const uint size, uint *index, const char* str) {
	if (!buf || size == 0 || !index) return;
	if (!str) str = "(null)";
	while (*str != '\0') strAddChar(buf, size, index, *str++);
}
static char tmp[65];
static const char *digits = "0123456789abcdef", *Digits = "0123456789ABCDEF";
static state du = false;
void strAddNum(char* buf, const uint size,  uint* index, int value, const uint8 base) {
	if (!buf || size == 0 || !index) return;
	if (base != 2 && base != 10 && base != 16) return;
	if (value == 0) {
		strAddChar(buf, size, index, '0');
		return;
	}
	if (value < 0) strAddChar(buf, size, index, '-');
	value = kabs(value);
	uint i = 0;
	while (value > 0) {
		tmp[i++] = (du ? Digits : digits)[value % base];
		value /= base;
	}
	while (i > 0) strAddChar(buf, size, index, tmp[--i]);
}
void strConvertV(char* buf, const uint size, const char* format, va_list args) {
	if (!buf || size == 0 || !format) return;
	uint i = 0;
	buf[0] = '\0';
	for (const char* p = format; *p != '\0'; p++) {
		if (*p != '%') {
			strAddChar(buf, size, &i, *p);
			continue;
		}
		p++;
		if (*p == '\0') break;
		switch (*p) {
			case 's': {
				const char* s = va_arg(args, const char*);
				strAddStr(buf, size, &i, s);
				break;
			}
			case 'c': {
				const char v = (char)va_arg(args, int);
				strAddChar(buf, size, &i, v);
				break;
			}
			case 'd': case 'i': {
				const int v = va_arg(args, int);
				strAddNum(buf, size, &i, v, 10);
				break;
			}
			case 'u': {
				const int v = kabs(va_arg(args, int));
				strAddNum(buf, size, &i, v, 10);
				break;
			}
			case 'x': {
				du = false;
				const int v = va_arg(args, int);
				strAddNum(buf, size, &i, v, 16);
				break;
			}
			case 'X': {
				du = true;
				const int v = va_arg(args, int);
				strAddNum(buf, size, &i, v, 16);
				break;
			}
			case 'q': {
				const uint8 v = kclamp(va_arg(args, int), 0, 255);
				strAddStr(buf, size, &i, "\xff[");
				if (v >= 100) strAddChar(buf, size, &i, ' ');
				if (v >= 10) strAddChar(buf, size, &i, ' ');
				strAddNum(buf, size, &i, v, 10);
				break;
			}
			case '%': { strAddChar(buf, size, &i, '%'); break; }
			default: {
				strAddChar(buf, size, &i, '%');
				strAddChar(buf, size, &i, *p);
				break;
			}
		}
	}
}
void strConvert(char* buf, const uint size, const char* format, ...) {
	va_list args;
	va_start(args, format);
	strConvertV(buf, size, format, args);
	va_end(args);
}
uint stringTest() {
	uint x = 0;
	x += strLen("Hello, World!") == 13;
	x += strIs("Hello", "Hello") && !strIs("Hello", "World");
	char temp[16];
	strCopy(temp, "This is Qunetra");
	x += strIs(temp, "This is Qunetra");
	strShiftRight(temp, 16, 5);
	x += strIs(temp, "     This is Qu");
	strShiftLeft(temp, 16, 7);
	x += strIs(temp, "is is Qu");
	x += strStartWith(temp, "is is");
	x += strEndWith(temp, "s Qu");
	x += strContains(temp, "is Q");
	x += strFind(temp, 'Q') == 6;
	strUpper(temp);
	x += strIs(temp, "IS IS QU");
	strLower(temp);
	x += strIs(temp, "is is qu");
	strCopy(temp, ".. \tQunetra\n ..");
	strTrimStartChar(temp, '.');
	x += strIs(temp, " \tQunetra\n ..");
	strTrimEndChar(temp, '.');
	x += strIs(temp, " \tQunetra\n ");
	strTrimStart(temp);
	x += strIs(temp, "Qunetra\n ");
	strTrimEnd(temp);
	x += strIs(temp, "Qunetra");
	strCopy(temp, " \t. Qunetra..\r\n");
	strTrim(temp);
	x += strIs(temp, ". Qunetra..");
	uint i = 11;
	strAddChar(temp, 16, &i, 'a');
	x += strIs(temp, ". Qunetra..a");
	strAddStr(temp, 16, &i, "test");
	x += strIs(temp, ". Qunetra..ates");
	i = 0;
	strAddNum(temp, 16, &i, 23, 2);
	strAddNum(temp, 16, &i, 23, 10);
	strAddNum(temp, 16, &i, 23, 16);
	x += strIs(temp, "101112317");
	strConvert(temp, 16, "%s%c%d%u%x%X%%%t", "xD", 'd', 12, -5, 13, 14);
	x += strIs(temp, "xDd125dE%%t");
	return x;
}
