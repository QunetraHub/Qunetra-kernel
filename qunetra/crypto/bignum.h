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

#ifndef QUNETRA_BIGNUM_H
#define QUNETRA_BIGNUM_H

#include "../utility/types.h"

#define BN_MAX_LIMBS 130

typedef struct {
	uint32 n[BN_MAX_LIMBS], rr[BN_MAX_LIMBS], ni;
	uint16 k;
} bnMod;

state bnModInit(bnMod* m, const uint8* mod, uint32 len);
state bnFromBytes(uint32* a, uint16 k, const uint8* be, uint32 len);
void bnToBytes(const uint32* a, uint16 k, uint8* be, uint32 len);
state bnIsZero(const uint32* a, uint16 k);
int bnCmp(const uint32* a, const uint32* b, uint16 k);
void bnSet(uint32* a, uint16 k, uint32 v);
uint32 bnBitLength(const uint32* a, uint16 k);
uint32 bnSub(uint32* r, const uint32* a, const uint32* b, uint16 k);
uint32 bnAdd(uint32* r, const uint32* a, const uint32* b, uint16 k);
void bnMontMul(const bnMod* m, uint32* r, const uint32* a, const uint32* b);
void bnToMont(const bnMod* m, uint32* r, const uint32* a);
void bnFromMont(const bnMod* m, uint32* r, const uint32* a);
void bnMulMod(const bnMod* m, uint32* r, const uint32* a, const uint32* b);
void bnModExp(const bnMod* m, uint32* r, const uint32* base, const uint32* exp, uint16 expLimbs);

#endif
