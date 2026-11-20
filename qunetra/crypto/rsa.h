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

#ifndef QUNETRA_RSA_H
#define QUNETRA_RSA_H

#include "../utility/types.h"

state rsaVerifyPkcs1(const uint8* n, uint32 nLen, const uint8* e, uint32 eLen, const uint8* sig, uint32 sigLen, uint16 hashBits, const uint8* hash);
state rsaVerifyPss(const uint8* n, uint32 nLen, const uint8* e, uint32 eLen, const uint8* sig, uint32 sigLen, uint16 hashBits, const uint8* hash);

#endif
