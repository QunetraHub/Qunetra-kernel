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

#ifndef QUNETRA_STRING_H
#define QUNETRA_STRING_H

#include "types.h"

uint strLen(const char* str);
state strIs(const char* strA, const char* strB);
void strCopy(char* dest, const char* src);
void strCopyN(char* dest, const char* src, const uint n);
void strShiftRight(char* str, const uint size, const uint shift);
void strShiftLeft(char* str, const uint size, const uint shift);
state strStartWith(const char* str, const char* prefix);
state strEndWith(const char* str, const char* suffix);
state strContains(const char* str, const char* sub);
int strFind(const char* str, const char c);
void strUpper(char* str);
void strLower(char* str);
void strTrimStartChar(char* str, const char c);
void strTrimEndChar(char* str, const char c);
void strTrimStart(char* str);
void strTrimEnd(char* str);
void strTrim(char* str);
void strAddChar(char* buffer, const uint size, uint *index, const char c);
void strAddStr(char* buffer, const uint size, uint *index, const char* str);
void strAddNum(char* buffor, const uint size,  uint* index, int value, const uint8 base);
uint strToInt(const char* str);
int strToUInt(const char* str);
void strConvert(char* buf, const uint size, const char* format, ...);

uint stringTest();

#endif
