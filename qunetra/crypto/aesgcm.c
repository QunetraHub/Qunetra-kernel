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
#include "aesgcm.h"

static const uint8 sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static uint8 xtime(const uint8 x) { return (uint8)((x << 1) ^ ((x >> 7) * 0x1b)); }
static void encryptBlock(const aesGcmCtx* c, const uint8* in, uint8* out);

state aesGcmInit(aesGcmCtx* c, const uint8* key, const uint8 keyLen) {
	if (keyLen != 16 && keyLen != 32) return false;
	const uint nk = keyLen / 4, total = 4 * (nk + 7);
	c->rounds = (uint8)(nk + 6);
	cCopy(c->rk, key, keyLen);
	uint8 rcon = 1;
	for (uint i = nk; i < total; i++) {
		uint8 t[4];
		cCopy(t, c->rk + 4 * (i - 1), 4);
		if (i % nk == 0) {
			const uint8 t0 = t[0];
			t[0] = (uint8)(sbox[t[1]] ^ rcon);
			t[1] = sbox[t[2]];
			t[2] = sbox[t[3]];
			t[3] = sbox[t0];
			rcon = xtime(rcon);
		} else if (nk > 6 && i % nk == 4) for (uint8 k = 0; k < 4; k++) t[k] = sbox[t[k]];
		for (uint8 k = 0; k < 4; k++) c->rk[4 * i + k] = (uint8)(c->rk[4 * (i - nk) + k] ^ t[k]);
	}
	uint8 zero[16];
	cZero(zero, 16);
	encryptBlock(c, zero, c->h);
	return true;
}
static void encryptBlock(const aesGcmCtx* c, const uint8* in, uint8* out) {
	uint8 s[16];
	for (uint8 i = 0; i < 16; i++) s[i] = (uint8)(in[i] ^ c->rk[i]);
	for (uint8 round = 1; round <= c->rounds; round++) {
		uint8 t[16];
		for (uint8 col = 0; col < 4; col++) for (uint8 row = 0; row < 4; row++) t[row + 4 * col] = sbox[s[row + 4 * ((col + row) % 4)]];
		if (round < c->rounds) {
			for (uint8 col = 0; col < 4; col++) {
				const uint8 a0 = t[4 * col], a1 = t[4 * col + 1], a2 = t[4 * col + 2], a3 = t[4 * col + 3];
				s[4 * col] = (uint8)(xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3);
				s[4 * col + 1] = (uint8)(a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3);
				s[4 * col + 2] = (uint8)(a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3));
				s[4 * col + 3] = (uint8)((xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3));
			}
		} else cCopy(s, t, 16);
		for (uint8 i = 0; i < 16; i++) s[i] = (uint8)(s[i] ^ c->rk[16 * round + i]);
	}
	cCopy(out, s, 16);
}
static void ghashMul(uint8 x[16], const uint8 h[16]) {
	uint8 z[16], v[16];
	cZero(z, 16);
	cCopy(v, h, 16);
	for (uint8 i = 0; i < 128; i++) {
		if ((x[i / 8] >> (7 - i % 8)) & 1) for (uint k = 0; k < 16; k++) z[k] ^= v[k];
		const uint8 lsb = v[15] & 1;
		for (uint8 k = 15; k > 0; k--) v[k] = (uint8)((v[k] >> 1) | (v[k - 1] << 7));
		v[0] >>= 1;
		if (lsb) v[0] ^= 0xe1;
	}
	cCopy(x, z, 16);
}
static void ghashBlocks(uint8 y[16], const uint8 h[16], const uint8* data, uint32 len) {
	while (len > 0) {
		const uint8 n = len < 16 ? len : 16;
		for (uint8 i = 0; i < n; i++) y[i] ^= data[i];
		ghashMul(y, h);
		data += n;
		len -= n;
	}
}
static void computeTag(const aesGcmCtx* c, const uint8 iv[12], const uint8* aad, const uint32 aadLen, const uint8* cipher, const uint32 len, uint8 tag[16]) {
	uint8 y[16], lens[16], j0[16], mask[16];
	cZero(y, 16);
	ghashBlocks(y, c->h, aad, aadLen);
	ghashBlocks(y, c->h, cipher, len);
	cPut64(lens, (uint64)aadLen * 8);
	cPut64(lens + 8, (uint64)len * 8);
	ghashBlocks(y, c->h, lens, 16);
	cCopy(j0, iv, 12);
	cPut32(j0 + 12, 1);
	encryptBlock(c, j0, mask);
	for (uint8 i = 0; i < 16; i++) tag[i] = (uint8)(y[i] ^ mask[i]);
}
static void ctrCrypt(const aesGcmCtx* c, const uint8 iv[12], const uint8* in, uint32 len, uint8* out) {
	uint8 ctr[16], ks[16];
	cCopy(ctr, iv, 12);
	uint32 n = 2;
	while (len > 0) {
		cPut32(ctr + 12, n++);
		encryptBlock(c, ctr, ks);
		const uint8 k = len < 16 ? len : 16;
		for (uint8 i = 0; i < k; i++) out[i] = (uint8)(in[i] ^ ks[i]);
		in += k;
		out += k;
		len -= k;
	}
}
void aesGcmSeal(const aesGcmCtx* c, const uint8 iv[12], const uint8* aad, const uint32 aadLen, const uint8* plain, const uint32 len, uint8* cipher, uint8 tag[16]) {
	ctrCrypt(c, iv, plain, len, cipher);
	computeTag(c, iv, aad, aadLen, cipher, len, tag);
}
state aesGcmOpen(const aesGcmCtx* c, const uint8 iv[12], const uint8* aad, const uint32 aadLen, const uint8* cipher, const uint32 len, const uint8 tag[16], uint8* plain) {
	uint8 expected[16];
	computeTag(c, iv, aad, aadLen, cipher, len, expected);
	if (!cEq(expected, tag, 16)) return false;
	ctrCrypt(c, iv, cipher, len, plain);
	return true;
}
