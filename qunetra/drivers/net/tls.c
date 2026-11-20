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
#include "../../crypto/aesgcm.h"
#include "../../crypto/x25519.h"
#include "../../crypto/rsa.h"
#include "../../crypto/ecdsa.h"
#include "../../crypto/rng.h"
#include "../rtc.h"
#include "../timer.h"
#include "tcp.h"
#include "x509.h"
#include "tls.h"

#ifndef TLS_HANDSHAKE_TIMEOUT_MS
#define TLS_HANDSHAKE_TIMEOUT_MS 15000
#endif
#define HS_MAX 20480
#define REC_MAX_PLAIN 16384
#define REC_MAX_CIPHER (REC_MAX_PLAIN + 256)

enum { CT_CCS = 20, CT_ALERT = 21, CT_HANDSHAKE = 22, CT_APPDATA = 23 };
enum { HS_CLIENT_HELLO = 1, HS_SERVER_HELLO = 2, HS_NEW_TICKET = 4, HS_ENC_EXT = 8, HS_CERT = 11, HS_CERT_REQ = 13, HS_CERT_VERIFY = 15, HS_FINISHED = 20, HS_KEY_UPDATE = 24 };
enum { ST_IDLE, ST_OPEN, ST_CLOSED };

static uint8
	st = ST_IDLE,
	wIv[12], rIv[12],
	wSecret[32], rSecret[32],
	recBuf[5 + REC_MAX_CIPHER],
	hsBuf[HS_MAX],
	sendBuf[5 + REC_MAX_PLAIN + 1 + 16],
	plain[REC_MAX_PLAIN + 1];
static state verifyCerts = true, clockChecked = false, decrypting = false, encrypting = false;
static uint32 recHave = 0, plainLen = 0, plainPos = 0, hsLen = 0;
static uint64 clockOverride = 0, wSeq, rSeq;
static const char* errText = "";
static aesGcmCtx wKey, rKey;
static sha256Ctx transcript;

static struct {
	uint8 type, eLen, n[512], e[8], x[48], y[48];
	uint16 nLen;
} leaf;

static state fail(const char* why) {
	if (errText[0] == '\0') errText = why;
	return false;
}
const char* tlsErrorText() { return errText; }
state tlsClockWasChecked() { return clockChecked; }
void tlsSetVerify(const state on) { verifyCerts = on; }
void tlsSetClock(const uint64 now) { clockOverride = now; }
static void deriveSecret(const uint8 secret[32], const char* label, const uint8 hash[32], uint8 out[32]) { hkdfExpandLabel(secret, label, hash, 32, out, 32); }
static void setKeys(aesGcmCtx* k, uint8 iv[12], const uint8 secret[32]) {
	uint8 key[16];
	hkdfExpandLabel(secret, "key", 0, 0, key, 16);
	hkdfExpandLabel(secret, "iv", 0, 0, iv, 12);
	aesGcmInit(k, key, 16);
	cZero(key, 16);
}
static void transcriptHash(uint8 out[32]) {
	sha256Ctx copy = transcript;
	sha256Final(&copy, out);
}
static void emptyHash(uint8 out[32]) { sha256(0, 0, out); }
static void makeNonce(uint8 nonce[12], const uint8 iv[12], const uint64 seq) {
	for (uint8 i = 0; i < 12; i++) nonce[i] = iv[i];
	for (uint8 i = 0; i < 8; i++) nonce[11 - i] ^= (uint8)(seq >> (8 * i));
}
static state sendRecord(const uint8 type, const uint8* data, const uint32 len) {
	if (len > REC_MAX_PLAIN) return false;
	uint32 n;
	if (!encrypting) {
		sendBuf[0] = type;
		cPut16(sendBuf + 1, 0x0303);
		cPut16(sendBuf + 3, (uint16)len);
		cCopy(sendBuf + 5, data, len);
		n = 5 + len;
	} else {
		uint8 nonce[12];
		const uint32 ctLen = len + 1 + 16;
		sendBuf[0] = CT_APPDATA;
		cPut16(sendBuf + 1, 0x0303);
		cPut16(sendBuf + 3, (uint16)ctLen);
		cCopy(sendBuf + 5, data, len);
		sendBuf[5 + len] = type;
		makeNonce(nonce, wIv, wSeq++);
		aesGcmSeal(&wKey, nonce, sendBuf, 5, sendBuf + 5, len + 1, sendBuf + 5, sendBuf + 5 + len + 1);
		n = 5 + ctLen;
	}
	return tcpSend(sendBuf, n);
}
static int readRecord(const uint64 deadline, uint8* type, const uint8** data, uint32* len) {
	while (true) {
		uint32 want = recHave < 5 ? 5 - recHave : 0;
		if (recHave >= 5) {
			const uint32 bodyLen = cGet16(recBuf + 3);
			if (bodyLen > REC_MAX_CIPHER) {
				fail("record too large");
				return -1;
			}
			if (5 + bodyLen == recHave) break;
			want = 5 + bodyLen - recHave;
		}
		const int n = tcpRecv(recBuf + recHave, (uint16)(want > 1024 ? 1024 : want));
		if (n < 0) {
			if (st == ST_OPEN) st = ST_CLOSED;
			fail("connection closed");
			return -1;
		}
		if (n > 0) {
			recHave += (uint32)n;
			continue;
		}
		if (deadline == 0) return 0;
		if (timerExpired(deadline)) {
			fail("timeout");
			return -1;
		}
	}
	const uint32 bodyLen = cGet16(recBuf + 3);
	recHave = 0;
	if (recBuf[0] != CT_APPDATA || !decrypting) {
		if (recBuf[0] == CT_APPDATA) {
			fail("unexpected encrypted record");
			return -1;
		}
		*type = recBuf[0];
		*data = recBuf + 5;
		*len = bodyLen;
		return 1;
	}
	if (bodyLen < 17) {
		fail("record too short");
		return -1;
	}
	uint8 nonce[12];
	makeNonce(nonce, rIv, rSeq++);
	if (!aesGcmOpen(&rKey, nonce, recBuf, 5, recBuf + 5, bodyLen - 16, recBuf + 5 + bodyLen - 16, recBuf + 5)) {
		fail("bad record MAC");
		return -1;
	}
	uint32 n = bodyLen - 16;
	while (n > 0 && recBuf[5 + n - 1] == 0) n--;
	if (n == 0) {
		fail("empty record");
		return -1;
	}
	*type = recBuf[5 + n - 1];
	*data = recBuf + 5;
	*len = n - 1;
	return 1;
}
static const char* alertText(const uint8 desc) {
	switch (desc) {
		case 0: return "server closed the connection";
		case 10: return "alert: unexpected message";
		case 20: return "alert: bad record MAC";
		case 40: return "alert: handshake failure (no common parameters)";
		case 42: return "alert: bad certificate";
		case 47: return "alert: illegal parameter";
		case 48: return "alert: unknown CA";
		case 50: return "alert: decode error";
		case 70: return "alert: protocol version not supported";
		case 80: return "alert: internal error on the server";
		case 109: return "alert: missing extension";
		default: return "alert from the server";
	}
}
static state hsAppend(const uint8* d, const uint32 n) {
	if (hsLen + n > HS_MAX) return fail("handshake message too large");
	cCopy(hsBuf + hsLen, d, n);
	hsLen += n;
	return true;
}
static state hsComplete() { return hsLen >= 4 && hsLen >= 4 + cGet24(hsBuf + 1); }
static void hsConsume() {
	const uint32 total = 4 + cGet24(hsBuf + 1);
	for (uint32 i = 0; i + total < hsLen; i++) hsBuf[i] = hsBuf[i + total];
	hsLen -= total;
}
static state hsWait(const uint64 deadline) {
	while (!hsComplete()) {
		uint8 type;
		const uint8* d;
		uint32 n;
		const int r = readRecord(deadline, &type, &d, &n);
		if (r <= 0) return false;
		if (type == CT_CCS) continue;
		if (type == CT_ALERT) return fail(alertText(n >= 2 ? d[1] : 80));
		if (type != CT_HANDSHAKE) return fail("unexpected record type");
		if (!hsAppend(d, n)) return false;
	}
	return true;
}
static uint8 clientPriv[32], clientPub[32], sessionId[32], clientRandom[32];
static uint32 buildClientHello(uint8* out, const char* host, const state sendSni) {
	uint32 p = 0;
	out[p++] = HS_CLIENT_HELLO;
	p += 3;
	cPut16(out + p, 0x0303);
	p += 2;
	cCopy(out + p, clientRandom, 32);
	p += 32;
	out[p++] = 32;
	cCopy(out + p, sessionId, 32);
	p += 32;
	cPut16(out + p, 2);
	cPut16(out + p + 2, 0x1301);
	p += 4;
	out[p++] = 1;
	out[p++] = 0;
	const uint32 extStart = p;
	p += 2;
	if (sendSni) {
		uint hl = 0;
		while (host[hl] != '\0') hl++;
		cPut16(out + p, 0);
		cPut16(out + p + 2, (uint16)(hl + 5));
		cPut16(out + p + 4, (uint16)(hl + 3));
		out[p + 6] = 0;
		cPut16(out + p + 7, (uint16)hl);
		cCopy(out + p + 9, host, hl);
		p += 9 + hl;
	}
	cPut16(out + p, 10);
	cPut16(out + p + 2, 4);
	cPut16(out + p + 4, 2);
	cPut16(out + p + 6, 0x001d);
	p += 8;
	{
		static const uint16 algs[] = {0x0403, 0x0503, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601};
		cPut16(out + p, 13);
		cPut16(out + p + 2, 2 + sizeof(algs));
		cPut16(out + p + 4, sizeof(algs));
		for (uint i = 0; i < sizeof(algs) / 2; i++) cPut16(out + p + 6 + 2 * i, algs[i]);
		p += 6 + sizeof(algs);
	}
	cPut16(out + p, 43);
	cPut16(out + p + 2, 3);
	out[p + 4] = 2;
	cPut16(out + p + 5, 0x0304);
	p += 7;
	cPut16(out + p, 45);
	cPut16(out + p + 2, 2);
	out[p + 4] = 1;
	out[p + 5] = 1;
	p += 6;
	cPut16(out + p, 51);
	cPut16(out + p + 2, 38);
	cPut16(out + p + 4, 36);
	cPut16(out + p + 6, 0x001d);
	cPut16(out + p + 8, 32);
	cCopy(out + p + 10, clientPub, 32);
	p += 42;
	cPut16(out + extStart, (uint16)(p - extStart - 2));
	cPut24(out + 1, p - 4);
	return p;
}
static state isIpLiteral(const char* s) {
	for (; *s != '\0'; s++) if ((*s < '0' || *s > '9') && *s != '.') return false;
	return true;
}
static uint64 currentTime() {
	clockChecked = false;
	uint64 now = clockOverride ? clockOverride : rtcNow();
	if (now < 20250101000000ull) return 0;
	clockChecked = true;
	return now;
}
static state handleCertificate(const uint8* body, uint32 len, const char* host) {
	if (len < 4 || body[0] != 0) return fail("bad Certificate message");
	const uint32 listLen = cGet24(body + 1);
	if (listLen + 4 != len) return fail("bad Certificate message");
	const uint8 *certs[X509_MAX_CHAIN];
	uint32 lens[X509_MAX_CHAIN];
	uint8 count = 0;
	const uint8* p = body + 4;
	uint32 left = listLen;
	while (left > 0) {
		if (left < 3) return fail("bad Certificate message");
		const uint32 cl = cGet24(p);
		if (cl == 0 || cl + 3 + 2 > left) return fail("bad Certificate message");
		if (count >= X509_MAX_CHAIN) return fail("too many certificates");
		certs[count] = p + 3;
		lens[count] = cl;
		count++;
		const uint32 extLen = cGet16(p + 3 + cl);
		if (3 + cl + 2 + extLen > left) return fail("bad Certificate message");
		p += 3 + cl + 2 + extLen;
		left -= 3 + cl + 2 + extLen;
	}
	if (count == 0) return fail("the server sent no certificate");
	x509Cert c;
	if (!x509Parse(certs[0], lens[0], &c)) return fail("unreadable server certificate");
	leaf.type = c.key.type;
	if (c.key.type == X509_KEY_RSA) {
		if (c.key.nLen > sizeof(leaf.n) || c.key.eLen > sizeof(leaf.e)) return fail("RSA key too large");
		cCopy(leaf.n, c.key.n, c.key.nLen);
		leaf.nLen = (uint16)c.key.nLen;
		cCopy(leaf.e, c.key.e, c.key.eLen);
		leaf.eLen = (uint8)c.key.eLen;
	} else if (c.key.type == X509_KEY_EC256 || c.key.type == X509_KEY_EC384) {
		const uint size = c.key.type == X509_KEY_EC256 ? 32 : 48;
		cCopy(leaf.x, c.key.x, size);
		cCopy(leaf.y, c.key.y, size);
	}
	if (!verifyCerts) return true;
	if (isIpLiteral(host)) return fail("certificate check needs a host name, not an address");
	const x509Error e = x509VerifyChain(certs, lens, count, host, currentTime());
	if (e != X509_OK) return fail(x509ErrorText(e));
	return true;
}
static state handleCertificateVerify(const uint8* body, const uint32 len, const uint8 th[32]) {
	if (len < 4) return fail("bad CertificateVerify");
	const uint16 scheme = cGet16(body), sigLen = cGet16(body + 2);
	if ((uint32)sigLen + 4 != len) return fail("bad CertificateVerify");
	const uint8* sig = body + 4;
	uint8 content[64 + 33 + 1 + 32], hash[64];
	for (uint i = 0; i < 64; i++) content[i] = 0x20;
	cCopy(content + 64, "TLS 1.3, server CertificateVerify", 33);
	content[97] = 0;
	cCopy(content + 98, th, 32);
	uint16 hashBits;
	switch (scheme) {
		case 0x0403: case 0x0804: hashBits = 256; break;
		case 0x0503: case 0x0805: hashBits = 384; break;
		case 0x0806: hashBits = 512; break;
		default: return fail("unsupported signature algorithm");
	}
	if (hashBits == 256) sha256(content, sizeof(content), hash);
	else if (hashBits == 384) sha384(content, sizeof(content), hash);
	else sha512(content, sizeof(content), hash);
	state ok = false;
	if (scheme == 0x0403 && leaf.type == X509_KEY_EC256) ok = ecdsaVerify(256, leaf.x, leaf.y, hash, 32, sig, sigLen);
	else if (scheme == 0x0503 && leaf.type == X509_KEY_EC384) ok = ecdsaVerify(384, leaf.x, leaf.y, hash, 48, sig, sigLen);
	else if ((scheme == 0x0804 || scheme == 0x0805 || scheme == 0x0806) && leaf.type == X509_KEY_RSA) ok = rsaVerifyPss(leaf.n, leaf.nLen, leaf.e, leaf.eLen, sig, sigLen, hashBits, hash);
	else return fail("signature type does not match the certificate key");
	return ok ? true : fail("bad CertificateVerify signature");
}
static void resetConnection() {
	st = ST_IDLE;
	recHave = 0;
	hsLen = 0;
	plainLen = plainPos = 0;
	wSeq = rSeq = 0;
	decrypting = encrypting = false;
	errText = "";
	clockChecked = false;
}
state tlsConnect(const uint8 ip[4], const uint16 port, const char* host) {
	if (st != ST_IDLE) tlsAbort();
	resetConnection();
	if (!host || host[0] == '\0') return fail("no host name");
	if (!tcpConnect(ip, port)) return fail("cannot connect");
	const uint64 deadline = timerDeadline(TLS_HANDSHAKE_TIMEOUT_MS);
	rngBytes(clientRandom, 32);
	rngBytes(sessionId, 32);
	rngBytes(clientPriv, 32);
	x25519Base(clientPub, clientPriv);
	static uint8 hello[600];
	const uint32 helloLen = buildClientHello(hello, host, !isIpLiteral(host));
	sha256Init(&transcript);
	sha256Update(&transcript, hello, helloLen);
	sendBuf[0] = 0;
	if (!sendRecord(CT_HANDSHAKE, hello, helloLen)) {
		tcpAbort();
		return fail("send failed");
	}
	if (!hsWait(deadline)) goto bad;
	if (hsBuf[0] != HS_SERVER_HELLO) {
		fail("unexpected message (expected ServerHello)");
		goto bad;
	}
	{
		const uint32 msgLen = 4 + cGet24(hsBuf + 1);
		const uint8* b = hsBuf + 4;
		uint32 n = msgLen - 4;
		static const uint8 hrr[32] = {
			0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
			0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E, 0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C
		};
		if (n < 2 + 32 + 1 + 32 + 2 + 1 + 2 || cGet16(b) != 0x0303) { fail("bad ServerHello"); goto bad; }
		if (cEq(b + 2, hrr, 32)) { fail("server asked for another key exchange (HelloRetryRequest), not supported"); goto bad; }
		if (b[34] != 32 || !cEq(b + 35, sessionId, 32)) { fail("bad ServerHello (session id)"); goto bad; }
		const uint16 suite = cGet16(b + 67);
		if (suite != 0x1301 || b[69] != 0) { fail("server chose an unsupported cipher suite"); goto bad; }
		const uint32 extLen = cGet16(b + 70);
		if (72 + extLen != n) { fail("bad ServerHello"); goto bad; }
		const uint8* e = b + 72;
		uint32 left = extLen;
		state haveVersion = false, haveKey = false;
		uint8 serverPub[32];
		while (left >= 4) {
			const uint16 type = cGet16(e), l = cGet16(e + 2);
			if ((uint32)l + 4 > left) { fail("bad ServerHello"); goto bad; }
			if (type == 43) {
				if (l != 2 || cGet16(e + 4) != 0x0304) { fail("server does not speak TLS 1.3"); goto bad; }
				haveVersion = true;
			} else if (type == 51) {
				if (l != 36 || cGet16(e + 4) != 0x001d || cGet16(e + 6) != 32) { fail("server chose an unsupported key exchange group"); goto bad; }
				cCopy(serverPub, e + 8, 32);
				haveKey = true;
			}
			e += 4 + l;
			left -= 4 + l;
		}
		if (!haveVersion || !haveKey || left != 0) { fail("server does not speak TLS 1.3"); goto bad; }
		sha256Update(&transcript, hsBuf, msgLen);
		hsConsume();
		uint8 shared[32];
		if (!x25519(shared, clientPriv, serverPub)) { fail("bad server key share"); goto bad; }
		cZero(clientPriv, 32);
		uint8 zeros[32], empty[32], early[32], derived[32], hsSecret[32], th[32], cHs[32], sHs[32];
		cZero(zeros, 32);
		emptyHash(empty);
		hkdfExtract(0, 0, zeros, 32, early);
		deriveSecret(early, "derived", empty, derived);
		hkdfExtract(derived, 32, shared, 32, hsSecret);
		cZero(shared, 32);
		transcriptHash(th);
		deriveSecret(hsSecret, "c hs traffic", th, cHs);
		deriveSecret(hsSecret, "s hs traffic", th, sHs);
		setKeys(&rKey, rIv, sHs);
		setKeys(&wKey, wIv, cHs);
		decrypting = true;
		rSeq = 0;
		deriveSecret(hsSecret, "derived", empty, derived);
		uint8 master[32];
		hkdfExtract(derived, 32, zeros, 32, master);
		cZero(hsSecret, 32);
		state gotEe = false, gotCert = false, gotCv = false, gotFin = false;
		uint8 thFin[32];
		while (!gotFin) {
			if (!hsWait(deadline)) goto bad;
			const uint8 type = hsBuf[0], *body = hsBuf + 4;
			const uint32 bodyLen = cGet24(hsBuf + 1), total = 4 + bodyLen;
			if (type == HS_ENC_EXT && !gotEe) {
				gotEe = true;
				if (bodyLen < 2 || cGet16(body) + 2u != bodyLen) { fail("bad EncryptedExtensions"); goto bad; }
			} else if (type == HS_CERT && gotEe && !gotCert) {
				gotCert = true;
				if (!handleCertificate(body, bodyLen, host)) goto bad;
			} else if (type == HS_CERT_VERIFY && gotCert && !gotCv) {
				gotCv = true;
				transcriptHash(th);
				if (verifyCerts && !handleCertificateVerify(body, bodyLen, th)) goto bad;
			} else if (type == HS_FINISHED && gotCv) {
				uint8 fkey[32], want[32];
				transcriptHash(th);
				hkdfExpandLabel(sHs, "finished", 0, 0, fkey, 32);
				hmacSha256(fkey, 32, th, 32, want);
				cZero(fkey, 32);
				if (bodyLen != 32 || !cEq(want, body, 32)) { fail("bad server Finished"); goto bad; }
				gotFin = true;
			} else if (type == HS_CERT_REQ) {
				fail("the server wants a client certificate (not supported)");
				goto bad;
			} else {
				fail("unexpected handshake message");
				goto bad;
			}
			sha256Update(&transcript, hsBuf, total);
			hsConsume();
		}
		transcriptHash(thFin);
		uint8 fkey[32], verify[36];
		hkdfExpandLabel(cHs, "finished", 0, 0, fkey, 32);
		verify[0] = HS_FINISHED;
		cPut24(verify + 1, 32);
		hmacSha256(fkey, 32, thFin, 32, verify + 4);
		cZero(fkey, 32);
		static const uint8 ccs[1] = {1};
		if (!sendRecord(CT_CCS, ccs, 1)) { fail("send failed"); goto bad; }
		encrypting = true;
		wSeq = 0;
		if (!sendRecord(CT_HANDSHAKE, verify, 36)) { fail("send failed"); goto bad; }
		deriveSecret(master, "c ap traffic", thFin, wSecret);
		deriveSecret(master, "s ap traffic", thFin, rSecret);
		cZero(master, 32);
		setKeys(&wKey, wIv, wSecret);
		setKeys(&rKey, rIv, rSecret);
		wSeq = rSeq = 0;
		cZero(cHs, 32);
		cZero(sHs, 32);
	}
	if (hsLen != 0) { fail("unexpected data after the server Finished"); goto bad; }
	st = ST_OPEN;
	return true;
bad:
	tcpAbort();
	st = ST_IDLE;
	decrypting = encrypting = false;
	return false;
}
state tlsSend(const uint8* data, uint32 len) {
	if (st != ST_OPEN) return false;
	while (len > 0) {
		const uint32 n = len > REC_MAX_PLAIN ? REC_MAX_PLAIN : len;
		if (!sendRecord(CT_APPDATA, data, n)) return false;
		data += n;
		len -= n;
	}
	return true;
}
static void updateKeys(uint8 secret[32], aesGcmCtx* k, uint8 iv[12], uint64* seq) {
	uint8 next[32];
	hkdfExpandLabel(secret, "traffic upd", 0, 0, next, 32);
	cCopy(secret, next, 32);
	cZero(next, 32);
	setKeys(k, iv, secret);
	*seq = 0;
}
static state postHandshake(const uint8* d, uint32 n) {
	while (n >= 4) {
		const uint32 l = cGet24(d + 1);
		if (4 + l > n) return fail("fragmented post-handshake message");
		if (d[0] == HS_KEY_UPDATE) {
			if (l != 1 || d[4] > 1) return fail("bad KeyUpdate");
			updateKeys(rSecret, &rKey, rIv, &rSeq);
			if (d[4] == 1) {
				static const uint8 msg[5] = {HS_KEY_UPDATE, 0, 0, 1, 0};
				if (!sendRecord(CT_HANDSHAKE, msg, 5)) return fail("send failed");
				updateKeys(wSecret, &wKey, wIv, &wSeq);
			}
		}
		d += 4 + l;
		n -= 4 + l;
	}
	return n == 0;
}
int tlsRecv(uint8* buf, const uint16 maxLen) {
	while (true) {
		if (plainPos < plainLen) {
			uint32 n = plainLen - plainPos;
			if (n > maxLen) n = maxLen;
			cCopy(buf, plain + plainPos, n);
			plainPos += n;
			return (int)n;
		}
		if (st != ST_OPEN) return -1;
		uint8 type;
		const uint8* d;
		uint32 n;
		const int r = readRecord(0, &type, &d, &n);
		if (r < 0) {
			st = ST_CLOSED;
			return -1;
		}
		if (r == 0) return 0;
		if (type == CT_APPDATA) {
			cCopy(plain, d, n);
			plainLen = n;
			plainPos = 0;
		} else if (type == CT_HANDSHAKE) {
			if (!postHandshake(d, n)) {
				st = ST_CLOSED;
				return -1;
			}
		} else if (type == CT_ALERT) {
			if (n >= 2 && d[1] != 0) fail(alertText(d[1]));
			st = ST_CLOSED;
			return -1;
		} else if (type != CT_CCS) {
			fail("unexpected record type");
			st = ST_CLOSED;
			return -1;
		}
	}
}
void tlsClose() {
	if (st == ST_OPEN) {
		static const uint8 closeNotify[2] = {1, 0};
		sendRecord(CT_ALERT, closeNotify, 2);
	}
	tcpClose();
	st = ST_IDLE;
	decrypting = encrypting = false;
}
void tlsAbort() {
	tcpAbort();
	st = ST_IDLE;
	decrypting = encrypting = false;
}
