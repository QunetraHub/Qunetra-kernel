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

#ifndef QUNETRA_SHA2_H
#define QUNETRA_SHA2_H

#include "../utility/types.h"

typedef struct {
	uint32 h[8];
	uint8 buf[64];
	uint32 used;
	uint64 total;
} sha256Ctx;
typedef struct {
	uint64 h[8];
	uint8 buf[128];
	uint32 used;
	uint64 total;
} sha512Ctx;
typedef struct {
	sha256Ctx inner, outer;
} hmacSha256Ctx;

void sha256Init(sha256Ctx* c);
void sha256Update(sha256Ctx* c, const uint8* data, uint32 len);
void sha256Final(sha256Ctx* c, uint8 out[32]);
void sha256(const uint8* data, uint32 len, uint8 out[32]);

void sha384Init(sha512Ctx* c);
void sha512Init(sha512Ctx* c);
void sha512Update(sha512Ctx* c, const uint8* data, uint32 len);
void sha384Final(sha512Ctx* c, uint8 out[48]);
void sha512Final(sha512Ctx* c, uint8 out[64]);
void sha384(const uint8* data, uint32 len, uint8 out[48]);
void sha512(const uint8* data, uint32 len, uint8 out[64]);

void hmacSha256Init(hmacSha256Ctx* c, const uint8* key, uint32 keyLen);
void hmacSha256Update(hmacSha256Ctx* c, const uint8* data, uint32 len);
void hmacSha256Final(hmacSha256Ctx* c, uint8 out[32]);
void hmacSha256(const uint8* key, uint32 keyLen, const uint8* data, uint32 len, uint8 out[32]);
void hkdfExtract(const uint8* salt, uint32 saltLen, const uint8* ikm, uint32 ikmLen, uint8 out[32]);
void hkdfExpandLabel(const uint8 secret[32], const char* label, const uint8* context, uint8 contextLen, uint8* out, uint8 outLen);

#endif
