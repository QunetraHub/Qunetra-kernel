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
#include "cutil.h"
#include "x25519.h"

typedef int64 gf[16];
static const gf gf121665 = {0xDB41, 1};

static void car25519(gf o) {
	for (uint8 i = 0; i < 16; i++) {
		o[i] += (int64)1 << 16;
		const int64 c = o[i] >> 16;
		o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
		o[i] -= c << 16;
	}
}
static void sel25519(gf p, gf q, const int b) {
	const int64 c = ~((int64)b - 1);
	for (uint8 i = 0; i < 16; i++) {
		const int64 t = c & (p[i] ^ q[i]);
		p[i] ^= t;
		q[i] ^= t;
	}
}
static void pack25519(uint8* o, const gf n) {
	gf m, t;
	for (uint8 i = 0; i < 16; i++) t[i] = n[i];
	car25519(t);
	car25519(t);
	car25519(t);
	for (uint8 j = 0; j < 2; j++) {
		m[0] = t[0] - 0xffed;
		for (uint8 i = 1; i < 15; i++) {
			m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
			m[i - 1] &= 0xffff;
		}
		m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
		const int b = (int)((m[15] >> 16) & 1);
		m[14] &= 0xffff;
		sel25519(t, m, 1 - b);
	}
	for (uint8 i = 0; i < 16; i++) {
		o[2 * i] = (uint8)(t[i] & 0xff);
		o[2 * i + 1] = (uint8)(t[i] >> 8);
	}
}
static void unpack25519(gf o, const uint8* n) {
	for (uint8 i = 0; i < 16; i++) o[i] = n[2 * i] + ((int64)n[2 * i + 1] << 8);
	o[15] &= 0x7fff;
}
static void fadd(gf o, const gf a, const gf b) { for (uint8 i = 0; i < 16; i++) o[i] = a[i] + b[i]; }
static void fsub(gf o, const gf a, const gf b) { for (uint8 i = 0; i < 16; i++) o[i] = a[i] - b[i]; }
static void fmul(gf o, const gf a, const gf b) {
	int64 t[31];
	for (uint8 i = 0; i < 31; i++) t[i] = 0;
	for (uint8 i = 0; i < 16; i++) for (uint j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
	for (uint8 i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
	for (uint8 i = 0; i < 16; i++) o[i] = t[i];
	car25519(o);
	car25519(o);
}
static void fsqr(gf o, const gf a) { fmul(o, a, a); }
static void finv(gf o, const gf in) {
	gf c;
	for (uint8 i = 0; i < 16; i++) c[i] = in[i];
	for (int16 a = 253; a >= 0; a--) {
		fsqr(c, c);
		if (a != 2 && a != 4) fmul(c, c, in);
	}
	for (uint8 i = 0; i < 16; i++) o[i] = c[i];
}

state x25519(uint8 out[32], const uint8 scalar[32], const uint8 point[32]) {
	uint8 z[32];
	gf x, a, b, c, d, e, f;
	cCopy(z, scalar, 32);
	z[31] = (uint8)((z[31] & 127) | 64);
	z[0] &= 248;
	unpack25519(x, point);
	for (uint8 i = 0; i < 16; i++) {
		b[i] = x[i];
		d[i] = a[i] = c[i] = 0;
	}
	a[0] = d[0] = 1;
	for (int16 i = 254; i >= 0; i--) {
		const int r = (z[i >> 3] >> (i & 7)) & 1;
		sel25519(a, b, r);
		sel25519(c, d, r);
		fadd(e, a, c);
		fsub(a, a, c);
		fadd(c, b, d);
		fsub(b, b, d);
		fsqr(d, e);
		fsqr(f, a);
		fmul(a, c, a);
		fmul(c, b, e);
		fadd(e, a, c);
		fsub(a, a, c);
		fsqr(b, a);
		fsub(c, d, f);
		fmul(a, c, gf121665);
		fadd(a, a, d);
		fmul(c, c, a);
		fmul(a, d, f);
		fmul(d, b, x);
		fsqr(b, e);
		sel25519(a, b, r);
		sel25519(c, d, r);
	}
	finv(c, c);
	fmul(a, a, c);
	pack25519(out, a);
	cZero(z, 32);
	uint8 acc = 0;
	for (uint8 i = 0; i < 32; i++) acc |= out[i];
	return acc != 0;
}
void x25519Base(uint8 out[32], const uint8 scalar[32]) {
	uint8 nine[32];
	cZero(nine, 32);
	nine[0] = 9;
	x25519(out, scalar, nine);
}
