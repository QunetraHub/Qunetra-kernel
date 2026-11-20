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
#include "ecdsa.h"

static const uint8 P256_P[32] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
}, P256_N[32] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51
}, P256_B[32] = {
	0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb, 0xbd, 0x55, 0x76, 0x98, 0x86, 0xbc,
	0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53, 0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b
}, P256_GX[32] = {
	0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
	0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96
}, P256_GY[32] = {
	0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
	0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
}, P384_P[48] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
}, P384_N[48] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc7, 0x63, 0x4d, 0x81, 0xf4, 0x37, 0x2d, 0xdf,
	0x58, 0x1a, 0x0d, 0xb2, 0x48, 0xb0, 0xa7, 0x7a, 0xec, 0xec, 0x19, 0x6a, 0xcc, 0xc5, 0x29, 0x73
}, P384_B[48] = {
	 0xb3, 0x31, 0x2f, 0xa7, 0xe2, 0x3e, 0xe7, 0xe4, 0x98, 0x8e, 0x05, 0x6b, 0xe3, 0xf8, 0x2d, 0x19,
	 0x18, 0x1d, 0x9c, 0x6e, 0xfe, 0x81, 0x41, 0x12, 0x03, 0x14, 0x08, 0x8f, 0x50, 0x13, 0x87, 0x5a,
	 0xc6, 0x56, 0x39, 0x8d, 0x8a, 0x2e, 0xd1, 0x9d, 0x2a, 0x85, 0xc8, 0xed, 0xd3, 0xec, 0x2a, 0xef
}, P384_GX[48] = {
	0xaa, 0x87, 0xca, 0x22, 0xbe, 0x8b, 0x05, 0x37, 0x8e, 0xb1, 0xc7, 0x1e, 0xf3, 0x20, 0xad, 0x74,
	0x6e, 0x1d, 0x3b, 0x62, 0x8b, 0xa7, 0x9b, 0x98, 0x59, 0xf7, 0x41, 0xe0, 0x82, 0x54, 0x2a, 0x38,
	0x55, 0x02, 0xf2, 0x5d, 0xbf, 0x55, 0x29, 0x6c, 0x3a, 0x54, 0x5e, 0x38, 0x72, 0x76, 0x0a, 0xb7
}, P384_GY[48] = {
	0x36, 0x17, 0xde, 0x4a, 0x96, 0x26, 0x2c, 0x6f, 0x5d, 0x9e, 0x98, 0xbf, 0x92, 0x92, 0xdc, 0x29,
	0xf8, 0xf4, 0x1d, 0xbd, 0x28, 0x9a, 0x14, 0x7c, 0xe9, 0xda, 0x31, 0x13, 0xb5, 0xf0, 0xb8, 0xc0,
	0x0a, 0x60, 0xb1, 0xce, 0x1d, 0x7e, 0x81, 0x9d, 0x7a, 0x43, 0x1d, 0x7c, 0x90, 0xea, 0x0e, 0x5f
};

#define LIMBS 12
typedef struct {
	bnMod p, n;
	uint32 b[LIMBS], gx[LIMBS], gy[LIMBS];
	uint16 k, bytes;
	state ready;
} curve;
static curve curve256, curve384;

static state curveInit(curve* c, const uint8* p, const uint8* n, const uint8* b, const uint8* gx, const uint8* gy, const uint16 bytes) {
	c->bytes = bytes;
	if (!bnModInit(&c->p, p, bytes) || !bnModInit(&c->n, n, bytes)) return false;
	c->k = c->p.k;
	uint32 t[LIMBS];
	bnFromBytes(t, c->k, b, bytes);
	bnToMont(&c->p, c->b, t);
	bnFromBytes(t, c->k, gx, bytes);
	bnToMont(&c->p, c->gx, t);
	bnFromBytes(t, c->k, gy, bytes);
	bnToMont(&c->p, c->gy, t);
	c->ready = true;
	return true;
}
static curve* getCurve(const uint16 bits) {
	if (bits == 256) {
		if (!curve256.ready && !curveInit(&curve256, P256_P, P256_N, P256_B, P256_GX, P256_GY, 32)) return 0;
		return &curve256;
	}
	if (bits == 384) {
		if (!curve384.ready && !curveInit(&curve384, P384_P, P384_N, P384_B, P384_GX, P384_GY, 48)) return 0;
		return &curve384;
	}
	return 0;
}
static void fAdd(const curve* c, uint32* r, const uint32* a, const uint32* b) {
	const uint32 carry = bnAdd(r, a, b, c->k);
	if (carry || bnCmp(r, c->p.n, c->k) >= 0) bnSub(r, r, c->p.n, c->k);
}
static void fSub(const curve* c, uint32* r, const uint32* a, const uint32* b) { if (bnSub(r, a, b, c->k)) bnAdd(r, r, c->p.n, c->k); }
static void fMul(const curve* c, uint32* r, const uint32* a, const uint32* b) { bnMontMul(&c->p, r, a, b); }
typedef struct { uint32 x[LIMBS], y[LIMBS], z[LIMBS]; } jp;
static void jpSetInf(jp* r) { for (uint i = 0; i < LIMBS; i++) r->x[i] = r->y[i] = r->z[i] = 0; }
static void jpCopy(jp* r, const jp* p) {
	for (uint8 i = 0; i < LIMBS; i++) {
		r->x[i] = p->x[i];
		r->y[i] = p->y[i];
		r->z[i] = p->z[i];
	}
}
static void jpDouble(const curve* c, jp* r, const jp* p) {
	if (bnIsZero(p->z, c->k) || bnIsZero(p->y, c->k)) {
		jpSetInf(r);
		return;
	}
	uint32 delta[LIMBS], gamma[LIMBS], beta[LIMBS], alpha[LIMBS], t1[LIMBS], t2[LIMBS], x3[LIMBS], y3[LIMBS], z3[LIMBS];
	fMul(c, delta, p->z, p->z);
	fMul(c, gamma, p->y, p->y);
	fMul(c, beta, p->x, gamma);
	fSub(c, t1, p->x, delta);
	fAdd(c, t2, p->x, delta);
	fMul(c, alpha, t1, t2);
	fAdd(c, t1, alpha, alpha);
	fAdd(c, alpha, t1, alpha);
	fMul(c, t1, alpha, alpha);
	fAdd(c, t2, beta, beta);
	fAdd(c, t2, t2, t2);
	fAdd(c, t2, t2, t2);
	fSub(c, x3, t1, t2);
	fAdd(c, t1, p->y, p->z);
	fMul(c, t1, t1, t1);
	fSub(c, t1, t1, gamma);
	fSub(c, z3, t1, delta);
	fAdd(c, t1, beta, beta);
	fAdd(c, t1, t1, t1);
	fSub(c, t1, t1, x3);
	fMul(c, t1, alpha, t1);
	fMul(c, t2, gamma, gamma);
	fAdd(c, t2, t2, t2);
	fAdd(c, t2, t2, t2);
	fAdd(c, t2, t2, t2);
	fSub(c, y3, t1, t2);
	for (uint8 i = 0; i < LIMBS; i++) {
		r->x[i] = x3[i];
		r->y[i] = y3[i];
		r->z[i] = z3[i];
	}
}
static void jpAdd(const curve* c, jp* r, const jp* p, const jp* q) {
	if (bnIsZero(p->z, c->k)) {
		jpCopy(r, q);
		return;
	}
	if (bnIsZero(q->z, c->k)) {
		jpCopy(r, p);
		return;
	}
	uint32 z1z1[LIMBS], z2z2[LIMBS], u1[LIMBS], u2[LIMBS], s1[LIMBS], s2[LIMBS], h[LIMBS], rr[LIMBS], i2[LIMBS], j[LIMBS], v[LIMBS], t1[LIMBS], t2[LIMBS], x3[LIMBS], y3[LIMBS], z3[LIMBS];
	fMul(c, z1z1, p->z, p->z);
	fMul(c, z2z2, q->z, q->z);
	fMul(c, u1, p->x, z2z2);
	fMul(c, u2, q->x, z1z1);
	fMul(c, t1, q->z, z2z2);
	fMul(c, s1, p->y, t1);
	fMul(c, t1, p->z, z1z1);
	fMul(c, s2, q->y, t1);
	fSub(c, h, u2, u1);
	fSub(c, t1, s2, s1);
	fAdd(c, rr, t1, t1);
	if (bnIsZero(h, c->k)) {
		if (bnIsZero(rr, c->k)) jpDouble(c, r, p);
		else jpSetInf(r);
		return;
	}
	fAdd(c, t1, h, h);
	fMul(c, i2, t1, t1);
	fMul(c, j, h, i2);
	fMul(c, v, u1, i2);
	fMul(c, t1, rr, rr);
	fSub(c, t1, t1, j);
	fAdd(c, t2, v, v);
	fSub(c, x3, t1, t2);
	fSub(c, t1, v, x3);
	fMul(c, t1, rr, t1);
	fMul(c, t2, s1, j);
	fAdd(c, t2, t2, t2);
	fSub(c, y3, t1, t2);
	fAdd(c, t1, p->z, q->z);
	fMul(c, t1, t1, t1);
	fSub(c, t1, t1, z1z1);
	fSub(c, t1, t1, z2z2);
	fMul(c, z3, t1, h);
	for (uint8 i = 0; i < LIMBS; i++) {
		r->x[i] = x3[i];
		r->y[i] = y3[i];
		r->z[i] = z3[i];
	}
}
static void jpMul(const curve* c, jp* r, const uint32* scalar, const jp* p) {
	jp acc;
	jpSetInf(&acc);
	for (uint32 i = bnBitLength(scalar, c->k); i > 0; i--) {
		jpDouble(c, &acc, &acc);
		if ((scalar[(i - 1) / 32] >> ((i - 1) % 32)) & 1) jpAdd(c, &acc, &acc, p);
	}
	jpCopy(r, &acc);
}
static state parseInt(const uint8* p, const uint32 len, uint32* pos, const uint8** val, uint32* vlen) {
	if (*pos + 2 > len || p[*pos] != 0x02) return false;
	const uint32 l = p[*pos + 1];
	if (l == 0 || l >= 0x80 || *pos + 2 + l > len) return false;
	if (p[*pos + 2] & 0x80) return false;
	*val = p + *pos + 2;
	*vlen = l;
	*pos += 2 + l;
	while (*vlen > 0 && (*val)[0] == 0) {
		(*val)++;
		(*vlen)--;
	}
	return true;
}
state ecdsaVerify(const uint16 curveBits, const uint8* qx, const uint8* qy, const uint8* hash, const uint32 hashLen, const uint8* sig, const uint32 sigLen) {
	curve* c = getCurve(curveBits);
	if (!c) return false;
	const uint16 k = c->k, bytes = c->bytes;
	if (sigLen < 8 || sig[0] != 0x30 || sig[1] >= 0x80 || (uint32)sig[1] + 2 != sigLen) return false;
	const uint8 *rb, *sb;
	uint32 rl, sl, pos = 2;
	if (!parseInt(sig, sigLen, &pos, &rb, &rl) || !parseInt(sig, sigLen, &pos, &sb, &sl) || pos != sigLen) return false;
	uint32 r[LIMBS], s[LIMBS], e[LIMBS], w[LIMBS], u1[LIMBS], u2[LIMBS], nm2[LIMBS], two[LIMBS];
	if (!bnFromBytes(r, k, rb, rl) || !bnFromBytes(s, k, sb, sl)) return false;
	if (bnIsZero(r, k) || bnIsZero(s, k) || bnCmp(r, c->n.n, k) >= 0 || bnCmp(s, c->n.n, k) >= 0) return false;
	if (!bnFromBytes(e, k, hash, hashLen > bytes ? bytes : hashLen)) return false;
	if (bnCmp(e, c->n.n, k) >= 0) bnSub(e, e, c->n.n, k);
	bnSet(two, k, 2);
	bnSub(nm2, c->n.n, two, k);
	bnModExp(&c->n, w, s, nm2, k);
	bnMulMod(&c->n, u1, e, w);
	bnMulMod(&c->n, u2, r, w);
	uint32 x[LIMBS], y[LIMBS];
	if (!bnFromBytes(x, k, qx, bytes) || !bnFromBytes(y, k, qy, bytes)) return false;
	if (bnCmp(x, c->p.n, k) >= 0 || bnCmp(y, c->p.n, k) >= 0) return false;
	jp q, g, r1, r2, sum;
	bnToMont(&c->p, q.x, x);
	bnToMont(&c->p, q.y, y);
	bnSet(x, k, 1);
	bnToMont(&c->p, q.z, x);
	{
		uint32 lhs[LIMBS], rhs[LIMBS], t[LIMBS];
		fMul(c, lhs, q.y, q.y);
		fMul(c, t, q.x, q.x);
		fMul(c, rhs, t, q.x);
		fAdd(c, t, q.x, q.x);
		fAdd(c, t, t, q.x);
		fSub(c, rhs, rhs, t);
		fAdd(c, rhs, rhs, c->b);
		if (bnCmp(lhs, rhs, k) != 0) return false;
	}
	for (uint8 i = 0; i < LIMBS; i++) {
		g.x[i] = i < k ? c->gx[i] : 0;
		g.y[i] = i < k ? c->gy[i] : 0;
		g.z[i] = q.z[i];
	}
	for (uint8 i = k; i < LIMBS; i++) q.x[i] = q.y[i] = q.z[i] = g.z[i] = 0;
	jpMul(c, &r1, u1, &g);
	jpMul(c, &r2, u2, &q);
	jpAdd(c, &sum, &r1, &r2);
	if (bnIsZero(sum.z, k)) return false;
	uint32 zp[LIMBS], pm2[LIMBS], zi[LIMBS], zim[LIMBS], z2[LIMBS], xa[LIMBS];
	bnFromMont(&c->p, zp, sum.z);
	bnSub(pm2, c->p.n, two, k);
	bnModExp(&c->p, zi, zp, pm2, k);
	bnToMont(&c->p, zim, zi);
	fMul(c, z2, zim, zim);
	fMul(c, xa, sum.x, z2);
	bnFromMont(&c->p, xa, xa);
	if (bnCmp(xa, c->n.n, k) >= 0) bnSub(xa, xa, c->n.n, k);
	return bnCmp(xa, r, k) == 0;
}
