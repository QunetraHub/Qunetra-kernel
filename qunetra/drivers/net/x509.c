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

#include "../../utility/types.h"
#include "../../crypto/cutil.h"
#include "../../crypto/sha2.h"
#include "../../crypto/rsa.h"
#include "../../crypto/ecdsa.h"
#include "x509.h"

typedef struct {
	const uint8* p;
	uint32 len;
} der;
static state derRead(der* in, uint8* tag, der* val, der* whole) {
	if (in->len < 2) return false;
	const uint8* s = in->p;
	uint32 l = s[1], hdr = 2;
	if ((s[0] & 0x1f) == 0x1f) return false;
	if (l & 0x80) {
		const uint32 n = l & 0x7f;
		if (n == 0 || n > 4 || in->len < 2 + n) return false;
		l = 0;
		for (uint32 i = 0; i < n; i++) l = (l << 8) | s[2 + i];
		hdr = 2 + n;
	}
	if (l > in->len - hdr) return false;
	*tag = s[0];
	val->p = s + hdr;
	val->len = l;
	if (whole) {
		whole->p = s;
		whole->len = hdr + l;
	}
	in->p += hdr + l;
	in->len -= hdr + l;
	return true;
}
static state derExpect(der* in, const uint8 tag, der* val, der* whole) {
	uint8 t;
	return derRead(in, &t, val, whole) && t == tag;
}
static state oidIs(const der* o, const uint8* bytes, const uint n) { return o->len == n && cEq(o->p, bytes, n); }

static const uint8
	OID_RSA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01},
	OID_EC[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01},
	OID_P256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07},
	OID_P384[] = {0x2b, 0x81, 0x04, 0x00, 0x22},
	OID_RSA_SHA256[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b},
	OID_RSA_SHA384[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c},
	OID_RSA_SHA512[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d},
	OID_ECDSA_SHA256[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02},
	OID_ECDSA_SHA384[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03},
	OID_ECDSA_SHA512[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04},
	OID_BASIC[] = {0x55, 0x1d, 0x13},
	OID_KEYUSAGE[] = {0x55, 0x1d, 0x0f},
	OID_SAN[] = {0x55, 0x1d, 0x11},
	OID_EKU[] = {0x55, 0x1d, 0x25},
	OID_NAMECONSTRAINTS[] = {0x55, 0x1d, 0x1e},
	OID_SERVERAUTH[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01},
	OID_ANYEKU[] = {0x55, 0x1d, 0x25, 0x00};

static state parseSigAlg(const der* oid, uint8* family, uint16* bits) {
	if (oidIs(oid, OID_RSA_SHA256, 9)) { *family = X509_SIG_RSA; *bits = 256; }
	else if (oidIs(oid, OID_RSA_SHA384, 9)) { *family = X509_SIG_RSA; *bits = 384; }
	else if (oidIs(oid, OID_RSA_SHA512, 9)) { *family = X509_SIG_RSA; *bits = 512; }
	else if (oidIs(oid, OID_ECDSA_SHA256, 8)) { *family = X509_SIG_ECDSA; *bits = 256; }
	else if (oidIs(oid, OID_ECDSA_SHA384, 8)) { *family = X509_SIG_ECDSA; *bits = 384; }
	else if (oidIs(oid, OID_ECDSA_SHA512, 8)) { *family = X509_SIG_ECDSA; *bits = 512; }
	else return false;
	return true;
}

static uint parseDigits(const uint8* p, const uint n) {
	uint v = 0;
	for (uint i = 0; i < n; i++) {
		if (p[i] < '0' || p[i] > '9') return 0xffffffffu;
		v = v * 10 + (p[i] - '0');
	}
	return v;
}
static state parseTime(der* in, uint64* out) {
	uint8 tag;
	der v;
	if (!derRead(in, &tag, &v, 0)) return false;
	uint year, off;
	if (tag == 0x17 && v.len == 13) {
		const uint yy = parseDigits(v.p, 2);
		year = yy >= 50 ? 1900 + yy : 2000 + yy;
		off = 2;
	} else if (tag == 0x18 && v.len == 15) {
		year = parseDigits(v.p, 4);
		off = 4;
	} else return false;
	if (v.p[v.len - 1] != 'Z') return false;
	const uint mo = parseDigits(v.p + off, 2), d = parseDigits(v.p + off + 2, 2), h = parseDigits(v.p + off + 4, 2), mi = parseDigits(v.p + off + 6, 2), s = parseDigits(v.p + off + 8, 2);
	if (year > 9999 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60) return false;
	*out = (uint64)year * 10000000000ull + (uint64)mo * 100000000ull + (uint64)d * 1000000ull + (uint64)h * 10000ull + (uint64)mi * 100ull + s;
	return true;
}
state x509ParseSpki(const uint8* data, const uint32 len, x509Key* k) {
	k->type = X509_KEY_NONE;
	der in = {data, len}, spki, alg, oid, bits;
	if (!derExpect(&in, 0x30, &spki, 0) || in.len != 0) return false;
	if (!derExpect(&spki, 0x30, &alg, 0) || !derExpect(&alg, 0x06, &oid, 0) || !derExpect(&spki, 0x03, &bits, 0) || spki.len != 0) return false;
	if (bits.len < 2 || bits.p[0] != 0) return false;
	const uint8* key = bits.p + 1;
	const uint32 keyLen = bits.len - 1;
	if (oidIs(&oid, OID_RSA, 9)) {
		der ki = {key, keyLen}, seq, n, e;
		if (!derExpect(&ki, 0x30, &seq, 0) || !derExpect(&seq, 0x02, &n, 0) || !derExpect(&seq, 0x02, &e, 0)) return false;
		while (n.len > 0 && n.p[0] == 0) {
			n.p++;
			n.len--;
		}
		while (e.len > 0 && e.p[0] == 0) {
			e.p++;
			e.len--;
		}
		if (n.len == 0 || e.len == 0) return false;
		k->type = X509_KEY_RSA;
		k->n = n.p;
		k->nLen = n.len;
		k->e = e.p;
		k->eLen = e.len;
		return true;
	}
	if (oidIs(&oid, OID_EC, 7)) {
		der curve;
		if (!derExpect(&alg, 0x06, &curve, 0)) return false;
		uint32 size;
		if (oidIs(&curve, OID_P256, 8)) { k->type = X509_KEY_EC256; size = 32; }
		else if (oidIs(&curve, OID_P384, 5)) { k->type = X509_KEY_EC384; size = 48; }
		else return true;
		if (keyLen != 1 + 2 * size || key[0] != 0x04) {
			k->type = X509_KEY_NONE;
			return false;
		}
		k->x = key + 1;
		k->y = key + 1 + size;
		return true;
	}
	return true;
}
state x509Parse(const uint8* data, const uint32 len, x509Cert* c) {
	cZero(c, sizeof(*c));
	der in = {data, len}, cert, tbs, tbsWhole, alg, oid, sigBits, f, whole;
	uint8 tag;
	if (!derExpect(&in, 0x30, &cert, 0) || in.len != 0) return false;
	if (!derExpect(&cert, 0x30, &tbs, &tbsWhole)) return false;
	if (!derExpect(&cert, 0x30, &alg, 0) || !derExpect(&alg, 0x06, &oid, 0) || !derExpect(&cert, 0x03, &sigBits, 0) || cert.len != 0) return false;
	if (!parseSigAlg(&oid, &c->sigFamily, &c->sigHashBits)) return false;
	if (sigBits.len < 2 || sigBits.p[0] != 0) return false;
	c->tbs = tbsWhole.p;
	c->tbsLen = tbsWhole.len;
	c->sig = sigBits.p + 1;
	c->sigLen = sigBits.len - 1;
	// tbsCertificate
	if (tbs.len > 0 && tbs.p[0] == 0xa0 && !derRead(&tbs, &tag, &f, 0)) return false;
	if (!derExpect(&tbs, 0x02, &f, 0)) return false;
	if (!derExpect(&tbs, 0x30, &f, 0)) return false;
	if (!derExpect(&tbs, 0x30, &f, &whole)) return false;
	c->issuer = whole.p;
	c->issuerLen = whole.len;
	if (!derExpect(&tbs, 0x30, &f, 0) || !parseTime(&f, &c->notBefore) || !parseTime(&f, &c->notAfter)) return false;
	if (!derExpect(&tbs, 0x30, &f, &whole)) return false;
	c->subject = whole.p;
	c->subjectLen = whole.len;
	if (!derExpect(&tbs, 0x30, &f, &whole) || !x509ParseSpki(whole.p, whole.len, &c->key)) return false;
	while (tbs.len > 0) {
		if (!derRead(&tbs, &tag, &f, 0)) return false;
		if (tag != 0xa3) continue;
		der exts, ext;
		if (!derExpect(&f, 0x30, &exts, 0)) return false;
		while (exts.len > 0) {
			der eoid, value;
			state critical = false;
			if (!derExpect(&exts, 0x30, &ext, 0) || !derExpect(&ext, 0x06, &eoid, 0)) return false;
			der peek = ext;
			if (peek.len > 0 && peek.p[0] == 0x01) {
				der b;
				if (!derExpect(&ext, 0x01, &b, 0) || b.len != 1) return false;
				critical = b.p[0] != 0;
			}
			if (!derExpect(&ext, 0x04, &value, 0)) return false;
			if (oidIs(&eoid, OID_BASIC, 3)) {
				der seq, b;
				if (!derExpect(&value, 0x30, &seq, 0)) return false;
				c->hasBasicConstraints = true;
				if (seq.len > 0 && seq.p[0] == 0x01) {
					if (!derExpect(&seq, 0x01, &b, 0) || b.len != 1) return false;
					c->isCa = b.p[0] != 0;
				}
				if (seq.len > 0 && seq.p[0] == 0x02) {
					if (!derExpect(&seq, 0x02, &b, 0) || b.len < 1 || b.len > 2 || (b.p[0] & 0x80)) return false;
					c->hasPathLen = true;
					c->pathLen = b.len == 2 ? 255 : b.p[0];
				}
			} else if (oidIs(&eoid, OID_KEYUSAGE, 3)) {
				der b;
				if (!derExpect(&value, 0x03, &b, 0) || b.len < 2) return false;
				c->hasKeyUsage = true;
				c->keyCertSign = (b.p[1] & 0x04) != 0;
			} else if (oidIs(&eoid, OID_SAN, 3)) {
				der seq;
				if (!derExpect(&value, 0x30, &seq, 0)) return false;
				c->san = seq.p;
				c->sanLen = seq.len;
			} else if (oidIs(&eoid, OID_EKU, 3)) {
				der seq, o;
				if (!derExpect(&value, 0x30, &seq, 0)) return false;
				c->hasEku = true;
				while (seq.len > 0) {
					if (!derExpect(&seq, 0x06, &o, 0)) return false;
					if (oidIs(&o, OID_SERVERAUTH, 8) || oidIs(&o, OID_ANYEKU, 4)) c->serverAuth = true;
				}
			} else if (critical && oidIs(&eoid, OID_NAMECONSTRAINTS, 3)) return false;
		}
	}
	return true;
}
static char lowerCase(const char ch) { return (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch; }
static state nameMatches(const uint8* pat, const uint32 plen, const char* host) {
	uint hl = 0;
	while (host[hl] != '\0') hl++;
	if (plen == 0 || hl == 0) return false;
	if (plen >= 3 && pat[0] == '*' && pat[1] == '.') {
		uint dot = 0;
		while (dot < hl && host[dot] != '.') dot++;
		if (dot == 0 || dot >= hl) return false;
		const uint rest = hl - dot, want = plen - 1;
		if (rest != want) return false;
		for (uint i = 0; i < rest; i++) if (lowerCase((char)pat[1 + i]) != lowerCase(host[dot + i])) return false;
		return true;
	}
	if (plen != hl) return false;
	for (uint i = 0; i < hl; i++) if (lowerCase((char)pat[i]) != lowerCase(host[i])) return false;
	return true;
}
state x509MatchHost(const x509Cert* c, const char* host) {
	der names = {c->san, c->sanLen}, v;
	uint8 tag;
	while (names.len > 0) {
		if (!derRead(&names, &tag, &v, 0)) return false;
		if (tag == 0x82 && nameMatches(v.p, v.len, host)) return true; // dNSName
	}
	return false;
}
state x509VerifyCertSignature(const x509Key* k, const x509Cert* c) {
	uint8 hash[64];
	const uint32 hLen = c->sigHashBits / 8;
	if (c->sigHashBits == 256) sha256(c->tbs, c->tbsLen, hash);
	else if (c->sigHashBits == 384) sha384(c->tbs, c->tbsLen, hash);
	else sha512(c->tbs, c->tbsLen, hash);
	if (c->sigFamily == X509_SIG_RSA) {
		if (k->type != X509_KEY_RSA) return false;
		return rsaVerifyPkcs1(k->n, k->nLen, k->e, k->eLen, c->sig, c->sigLen, c->sigHashBits, hash);
	}
	if (c->sigFamily == X509_SIG_ECDSA) {
		if (k->type != X509_KEY_EC256 && k->type != X509_KEY_EC384) return false;
		return ecdsaVerify(k->type == X509_KEY_EC256 ? 256 : 384, k->x, k->y, hash, hLen, c->sig, c->sigLen);
	}
	return false;
}
static state sameBytes(const uint8* a, const uint32 al, const uint8* b, const uint32 bl) { return al == bl && cEq(a, b, al); }
x509Error x509VerifyChain(const uint8* const* certs, const uint32* lens, const uint8 count, const char* host, const uint64 now) {
	static x509Cert c[X509_MAX_CHAIN];
	state used[X509_MAX_CHAIN];
	if (count == 0 || count > X509_MAX_CHAIN) return X509_E_PARSE;
	for (uint8 i = 0; i < count; i++) {
		if (!x509Parse(certs[i], lens[i], &c[i])) return X509_E_PARSE;
		used[i] = false;
	}
	used[0] = true;
	if (host && !x509MatchHost(&c[0], host)) return X509_E_HOSTNAME;
	if (c[0].hasEku && !c[0].serverAuth) return X509_E_USAGE;
	if (now != 0) {
		if (now < c[0].notBefore) return X509_E_NOT_YET;
		if (now > c[0].notAfter) return X509_E_EXPIRED;
	}
	uint8 cur = 0, below = 0;
	x509Error last = X509_E_UNTRUSTED;
	for (uint8 step = 0; step <= X509_MAX_CHAIN; step++) {
		for (uint16 r = 0; r < x509RootCount; r++) {
			if (!sameBytes(c[cur].issuer, c[cur].issuerLen, x509Roots[r].subject, x509Roots[r].subjectLen)) continue;
			x509Key rk;
			if (!x509ParseSpki(x509Roots[r].spki, x509Roots[r].spkiLen, &rk) || rk.type == X509_KEY_NONE) {
				last = X509_E_KEY;
				continue;
			}
			if (x509VerifyCertSignature(&rk, &c[cur])) return X509_OK;
			last = X509_E_SIGNATURE;
		}
		int found = -1;
		for (uint8 i = 1; i < count; i++) {
			if (used[i] || !sameBytes(c[cur].issuer, c[cur].issuerLen, c[i].subject, c[i].subjectLen)) continue;
			if (c[i].key.type == X509_KEY_NONE) {
				last = X509_E_KEY;
				continue;
			}
			if (!x509VerifyCertSignature(&c[i].key, &c[cur])) {
				last = X509_E_SIGNATURE;
				continue;
			}
			found = i;
			break;
		}
		if (found < 0) return last;
		const x509Cert* ca = &c[found];
		if (!ca->hasBasicConstraints || !ca->isCa || (ca->hasKeyUsage && !ca->keyCertSign)) return X509_E_NOT_CA;
		if (ca->hasPathLen && below > ca->pathLen) return X509_E_NOT_CA;
		if (now != 0) {
			if (now < ca->notBefore) return X509_E_NOT_YET;
			if (now > ca->notAfter) return X509_E_EXPIRED;
		}
		used[found] = true;
		cur = (uint8)found;
		below++;
		last = X509_E_UNTRUSTED;
	}
	return X509_E_UNTRUSTED;
}
const char* x509ErrorText(const x509Error e) {
	switch (e) {
		case X509_OK: return "ok";
		case X509_E_PARSE: return "bad certificate";
		case X509_E_HOSTNAME: return "certificate is for another host name";
		case X509_E_EXPIRED: return "certificate expired";
		case X509_E_NOT_YET: return "certificate not valid yet (is the clock right?)";
		case X509_E_UNTRUSTED: return "untrusted certificate chain";
		case X509_E_SIGNATURE: return "bad certificate signature";
		case X509_E_NOT_CA: return "issuer is not a CA";
		case X509_E_KEY: return "unsupported key or signature type";
		case X509_E_USAGE: return "certificate not meant for servers";
		default: return "certificate error";
	}
}
