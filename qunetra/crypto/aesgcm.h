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

#ifndef QUNETRA_AESGCM_H
#define QUNETRA_AESGCM_H

#include "../utility/types.h"

typedef struct { uint8 rk[240], rounds, h[16]; } aesGcmCtx;

state aesGcmInit(aesGcmCtx* c, const uint8* key, uint8 keyLen);
void aesGcmSeal(const aesGcmCtx* c, const uint8 iv[12], const uint8* aad, uint32 aadLen, const uint8* plain, uint32 len, uint8* cipher, uint8 tag[16]);
state aesGcmOpen(const aesGcmCtx* c, const uint8 iv[12], const uint8* aad, uint32 aadLen, const uint8* cipher, uint32 len, const uint8 tag[16], uint8* plain);

#endif
