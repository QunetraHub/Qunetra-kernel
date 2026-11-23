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

#ifndef QUNETRA_MOUSE_H
#define QUNETRA_MOUSE_H

#include "../utility/types.h"

#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04
#define MOUSE_BTN4   0x08
#define MOUSE_BTN5   0x10

typedef struct {
	int16 dx, dy;
	int8 wheel;
	uint8 buttons;
} mouseEvent;

state initMouse();
state mouseHasWheel();
int mouseGet(mouseEvent* e);
uint8 mouseButtons();
void mouseSetBounds(uint32 width, uint32 height);
void mouseGetPosition(int32* x, int32* y);
void mouseSetPosition(int32 x, int32 y);
int32 mouseTakeWheel();

#endif
