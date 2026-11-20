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
#include "bignum.h"

state bnIsZero(const uint32* a, const uint16 k) {
	uint32 acc = 0;
	for (uint16 i = 0; i < k; i++) acc |= a[i];
	return acc == 0;
}
int bnCmp(const uint32* a, const uint32* b, const uint16 k) {
	for (uint16 i = k; i > 0; i--) {
		if (a[i - 1] > b[i - 1]) return 1;
		if (a[i - 1] < b[i - 1]) return -1;
	}
	return 0;
}
void bnSet(uint32* a, const uint16 k, const uint32 v) {
	for (uint16 i = 0; i < k; i++) a[i] = 0;
	a[0] = v;
}
uint32 bnBitLength(const uint32* a, const uint16 k) {
	for (uint16 i = k; i > 0; i--) {
		if (a[i - 1] != 0) {
			uint32 bits = 0, v = a[i - 1];
			while (v) {
				bits++;
				v >>= 1;
			}
			return (uint32)(i - 1) * 32 + bits;
		}
	}
	return 0;
}
uint32 bnSub(uint32* r, const uint32* a, const uint32* b, const uint16 k) {
	uint32 borrow = 0;
	for (uint16 i = 0; i < k; i++) {
		const uint64 d = (uint64)a[i] - b[i] - borrow;
		r[i] = (uint32)d;
		borrow = (uint32)(d >> 63);
	}
	return borrow;
}
uint32 bnAdd(uint32* r, const uint32* a, const uint32* b, const uint16 k) {
	uint32 carry = 0;
	for (uint16 i = 0; i < k; i++) {
		const uint64 s = (uint64)a[i] + b[i] + carry;
		r[i] = (uint32)s;
		carry = (uint32)(s >> 32);
	}
	return carry;
}
state bnFromBytes(uint32* a, const uint16 k, const uint8* be, uint32 len) {
	while (len > 0 && be[0] == 0) {
		be++;
		len--;
	}
	if (len > (uint32)k * 4) return false;
	for (uint16 i = 0; i < k; i++) a[i] = 0;
	for (uint32 i = 0; i < len; i++) a[i / 4] |= (uint32)be[len - 1 - i] << (8 * (i % 4));
	return true;
}
void bnToBytes(const uint32* a, const uint16 k, uint8* be, const uint32 len) {
	for (uint32 i = 0; i < len; i++) {
		const uint32 limb = i / 4;
		be[len - 1 - i] = limb < k ? (uint8)(a[limb] >> (8 * (i % 4))) : 0;
	}
}
state bnModInit(bnMod* m, const uint8* mod, uint32 len) {
	while (len > 0 && mod[0] == 0) {
		mod++;
		len--;
	}
	if (len == 0 || len > (uint32)BN_MAX_LIMBS * 4) return false;
	m->k = (uint16)((len + 3) / 4);
	for (uint16 i = 0; i < BN_MAX_LIMBS; i++) m->n[i] = m->rr[i] = 0;
	bnFromBytes(m->n, m->k, mod, len);
	if (!(m->n[0] & 1)) return false;
	if (m->k == 1 && m->n[0] == 1) return false;
	uint32 inv = m->n[0];
	for (uint8 i = 0; i < 5; i++) inv *= 2 - m->n[0] * inv;
	m->ni = (uint32)0 - inv;
	uint32 r[BN_MAX_LIMBS];
	bnSet(r, m->k, 1);
	for (uint32 step = 0; step < 64u * m->k; step++) {
		uint32 carry = 0;
		for (uint16 i = 0; i < m->k; i++) {
			const uint32 next = r[i] >> 31;
			r[i] = (r[i] << 1) | carry;
			carry = next;
		}
		if (carry || bnCmp(r, m->n, m->k) >= 0) bnSub(r, r, m->n, m->k);
	}
	for (uint16 i = 0; i < m->k; i++) m->rr[i] = r[i];
	return true;
}
void bnMontMul(const bnMod* m, uint32* r, const uint32* a, const uint32* b) {
	const uint16 k = m->k;
	uint32 t[BN_MAX_LIMBS + 2];
	for (uint16 i = 0; i < k + 2; i++) t[i] = 0;
	for (uint16 i = 0; i < k; i++) {
		uint64 c = 0;
		for (uint16 j = 0; j < k; j++) {
			const uint64 s = (uint64)t[j] + (uint64)a[j] * b[i] + c;
			t[j] = (uint32)s;
			c = s >> 32;
		}
		uint64 s = (uint64)t[k] + c;
		t[k] = (uint32)s;
		t[k + 1] = (uint32)(s >> 32);
		const uint32 q = t[0] * m->ni;
		s = (uint64)t[0] + (uint64)q * m->n[0];
		c = s >> 32;
		for (uint16 j = 1; j < k; j++) {
			s = (uint64)t[j] + (uint64)q * m->n[j] + c;
			t[j - 1] = (uint32)s;
			c = s >> 32;
		}
		s = (uint64)t[k] + c;
		t[k - 1] = (uint32)s;
		t[k] = t[k + 1] + (uint32)(s >> 32);
	}
	if (t[k] || bnCmp(t, m->n, k) >= 0) bnSub(t, t, m->n, k);
	for (uint16 i = 0; i < k; i++) r[i] = t[i];
}
void bnToMont(const bnMod* m, uint32* r, const uint32* a) { bnMontMul(m, r, a, m->rr); }
void bnFromMont(const bnMod* m, uint32* r, const uint32* a) {
	uint32 one[BN_MAX_LIMBS];
	bnSet(one, m->k, 1);
	bnMontMul(m, r, a, one);
}
void bnMulMod(const bnMod* m, uint32* r, const uint32* a, const uint32* b) {
	uint32 t[BN_MAX_LIMBS];
	bnMontMul(m, t, a, m->rr);
	bnMontMul(m, r, t, b);
}
void bnModExp(const bnMod* m, uint32* r, const uint32* base, const uint32* exp, const uint16 expLimbs) {
	uint32 b[BN_MAX_LIMBS], acc[BN_MAX_LIMBS];
	const uint32 bits = bnBitLength(exp, expLimbs);
	if (bits == 0) {
		bnSet(r, m->k, 1);
		return;
	}
	bnToMont(m, b, base);
	for (uint16 i = 0; i < m->k; i++) acc[i] = b[i];
	for (uint32 i = bits - 1; i > 0; i--) {
		bnMontMul(m, acc, acc, acc);
		if ((exp[(i - 1) / 32] >> ((i - 1) % 32)) & 1) bnMontMul(m, acc, acc, b);
	}
	bnFromMont(m, r, acc);
}
