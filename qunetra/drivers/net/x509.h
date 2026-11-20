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

#ifndef QUNETRA_X509_H
#define QUNETRA_X509_H

#include "../../utility/types.h"

#define X509_MAX_CHAIN 8

enum { X509_KEY_NONE = 0, X509_KEY_RSA, X509_KEY_EC256, X509_KEY_EC384 };
enum { X509_SIG_NONE = 0, X509_SIG_RSA, X509_SIG_ECDSA };

typedef struct {
	uint8 type;
	const uint8 *n, *e;
	uint32 nLen, eLen;
	const uint8 *x, *y;
} x509Key;

typedef struct {
	const uint8 *tbs, *issuer, *subject, *sig, *san;
	uint32 tbsLen, issuerLen, subjectLen, sigLen, sanLen;
	uint8 sigFamily;
	uint16 sigHashBits;
	uint64 notBefore, notAfter;
	x509Key key;
	state hasBasicConstraints, isCa, hasPathLen, hasKeyUsage, keyCertSign, hasEku, serverAuth;
	uint8 pathLen;
} x509Cert;

typedef struct {
	const uint8* subject;
	uint16 subjectLen;
	const uint8* spki;
	uint16 spkiLen;
} x509Root;
extern const x509Root x509Roots[];
extern const uint16 x509RootCount;

typedef enum {
	X509_OK = 0,
	X509_E_PARSE,
	X509_E_HOSTNAME,
	X509_E_EXPIRED,
	X509_E_NOT_YET,
	X509_E_UNTRUSTED,
	X509_E_SIGNATURE,
	X509_E_NOT_CA,
	X509_E_KEY,
	X509_E_USAGE
} x509Error;

state x509Parse(const uint8* der, uint32 len, x509Cert* cert);
state x509ParseSpki(const uint8* der, uint32 len, x509Key* key);
state x509MatchHost(const x509Cert* cert, const char* host);
state x509VerifyCertSignature(const x509Key* issuerKey, const x509Cert* cert);
x509Error x509VerifyChain(const uint8* const* certs, const uint32* lens, uint8 count, const char* host, uint64 now);
const char* x509ErrorText(x509Error e);

#endif
