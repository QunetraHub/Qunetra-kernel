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

#ifndef QUNETRA_LFB_H
#define QUNETRA_LFB_H

#include "../utility/types.h"

state initLFB(const uint32 addr);
uint32 getWidth();
uint32 getHeight();
void scrollUp();
void scrollUpN(uint32 n);
void putPx(const uint32 x, const uint32 y, const uint32 color);
void drawRect(const uint32 x, const uint32 y, const uint32 width, const uint32 height, const uint32 color);
void lfbFill(const uint32 color);

#endif
