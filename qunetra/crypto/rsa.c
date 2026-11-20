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
#include "bignum.h"
#include "sha2.h"
#include "rsa.h"

#define RSA_MAX_BYTES (BN_MAX_LIMBS * 4)

static void hashData(const uint16 bits, const uint8* data, const uint32 len, uint8* out) {
	if (bits == 256) sha256(data, len, out);
	else if (bits == 384) sha384(data, len, out);
	else sha512(data, len, out);
}
static state rsaPublic(const uint8* n, uint32 nLen, const uint8* e, uint32 eLen, const uint8* sig, const uint32 sigLen, uint8* em, uint32* emLen, uint32* modBits) {
	while (nLen > 0 && n[0] == 0) {
		n++;
		nLen--;
	}
	while (eLen > 0 && e[0] == 0) {
		e++;
		eLen--;
	}
	if (nLen < 128 || nLen > RSA_MAX_BYTES || eLen == 0 || eLen > 8) return false;
	if (sigLen != nLen) return false;
	static bnMod mod;
	if (!bnModInit(&mod, n, nLen)) return false;
	uint32 ex[2], base[BN_MAX_LIMBS], r[BN_MAX_LIMBS];
	if (!bnFromBytes(ex, 2, e, eLen) || !(ex[0] & 1) || (ex[1] == 0 && ex[0] < 3)) return false;
	if (!bnFromBytes(base, mod.k, sig, sigLen) || bnCmp(base, mod.n, mod.k) >= 0) return false;
	bnModExp(&mod, r, base, ex, 2);
	bnToBytes(r, mod.k, em, nLen);
	*emLen = nLen;
	*modBits = bnBitLength(mod.n, mod.k);
	return true;
}

static const uint8 prefix256[19] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60,
	0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
	0x01, 0x05, 0x00, 0x04, 0x20
}, prefix384[19] = {
	0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60,
	0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
	0x02, 0x05, 0x00, 0x04, 0x30
}, prefix512[19] = {
	0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60,
	0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
	0x03, 0x05, 0x00, 0x04, 0x40
};

state rsaVerifyPkcs1(const uint8* n, const uint32 nLen, const uint8* e, const uint32 eLen, const uint8* sig, const uint32 sigLen, const uint16 hashBits, const uint8* hash) {
	if (hashBits != 256 && hashBits != 384 && hashBits != 512) return false;
	uint8 em[RSA_MAX_BYTES], want[RSA_MAX_BYTES];
	uint32 len, bits;
	if (!rsaPublic(n, nLen, e, eLen, sig, sigLen, em, &len, &bits)) return false;
	const uint32 hLen = hashBits / 8, tLen = 19 + hLen;
	if (len < tLen + 11) return false;
	want[0] = 0;
	want[1] = 1;
	for (uint32 i = 2; i < len - tLen - 1; i++) want[i] = 0xff;
	want[len - tLen - 1] = 0;
	cCopy(want + len - tLen, hashBits == 256 ? prefix256 : hashBits == 384 ? prefix384 : prefix512, 19);
	cCopy(want + len - hLen, hash, hLen);
	return cEq(em, want, len);
}
static void mgf1(const uint16 hashBits, const uint8* seed, const uint32 seedLen, uint8* out, const uint32 outLen) {
	uint8 in[64 + 4], digest[64];
	const uint32 hLen = hashBits / 8;
	cCopy(in, seed, seedLen);
	for (uint32 counter = 0, done = 0; done < outLen; counter++) {
		cPut32(in + seedLen, counter);
		hashData(hashBits, in, seedLen + 4, digest);
		for (uint32 i = 0; i < hLen && done < outLen; i++) out[done++] = digest[i];
	}
}
state rsaVerifyPss(const uint8* n, const uint32 nLen, const uint8* e, const uint32 eLen, const uint8* sig, const uint32 sigLen, const uint16 hashBits, const uint8* hash) {
	if (hashBits != 256 && hashBits != 384 && hashBits != 512) return false;
	uint8 em[RSA_MAX_BYTES], db[RSA_MAX_BYTES], mPrime[8 + 64 + 64], h2[64];
	uint32 len, modBits;
	if (!rsaPublic(n, nLen, e, eLen, sig, sigLen, em, &len, &modBits)) return false;
	const uint32 hLen = hashBits / 8, emBits = modBits - 1, emLen = (emBits + 7) / 8;
	const uint8* block = em;
	if (emLen == len - 1) {
		if (em[0] != 0) return false;
		block = em + 1;
	} else if (emLen != len) return false;
	if (emLen < 2 * hLen + 2 || block[emLen - 1] != 0xbc) return false;
	const uint32 dbLen = emLen - hLen - 1, topBits = 8 * emLen - emBits;
	const uint8 keep = (uint8)(0xff >> topBits);
	if (block[0] & (uint8)~keep) return false;
	const uint8* hh = block + dbLen;
	mgf1(hashBits, hh, hLen, db, dbLen);
	for (uint32 i = 0; i < dbLen; i++) db[i] ^= block[i];
	db[0] &= keep;
	const uint32 padLen = dbLen - hLen - 1;
	for (uint32 i = 0; i < padLen; i++) if (db[i] != 0) return false;
	if (db[padLen] != 1) return false;
	cZero(mPrime, 8);
	cCopy(mPrime + 8, hash, hLen);
	cCopy(mPrime + 8 + hLen, db + dbLen - hLen, hLen);
	hashData(hashBits, mPrime, 8 + 2 * hLen, h2);
	return cEq(h2, hh, hLen);
}
