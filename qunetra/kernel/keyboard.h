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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../utility/types.h"

#define KEY_ESC    '\x1b'
#define KEY_UP     '\x11'
#define KEY_DOWN   '\x12'
#define KEY_LEFT   '\x13'
#define KEY_RIGHT  '\x14'
#define KEY_HOME   '\x15'
#define KEY_END    '\x16'
#define KEY_DELETE '\x7f'

state initKeyboard();
char keyboardGet();

#endif
