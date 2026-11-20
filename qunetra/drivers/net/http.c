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
#include "net.h"
#include "tcp.h"
#include "tls.h"
#include "../timer.h"
#include "http.h"

#ifndef HTTP_IDLE_TIMEOUT_MS
#define HTTP_IDLE_TIMEOUT_MS 20000
#endif
#define HEADER_MAX 2048
#define LOCATION_MAX 200

static char location[LOCATION_MAX + 1];
static uint32 bodyBytes = 0;
static state skipBody = false, followMode = false;
static const char* tlsDetail = "";

typedef struct {
	state (*connect)(const uint8 ip[4], uint16 port, const char* host), (*send)(const uint8* data, uint32 len);
	int (*recv)(uint8* buf, uint16 max);
	void (*close)(), (*abort)();
} transport;
static state plainConnect(const uint8 ip[4], const uint16 port, const char* host) {
	(void)host;
	return tcpConnect(ip, port);
}

static const transport plainTransport = {plainConnect, tcpSend, tcpRecv, tcpClose, tcpAbort}, tlsTransport = {tlsConnect, tlsSend, tlsRecv, tlsClose, tlsAbort};

static uint slen(const char* s) {
	uint n = 0;
	while (s[n] != '\0') n++;
	return n;
}
static char lower(const char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static const char* headerValue(const char* line, const char* name) {
	while (*name != '\0') {
		if (lower(*line) != *name) return 0;
		line++;
		name++;
	}
	while (*line == ' ' || *line == '\t') line++;
	return line;
}
static state hasToken(const char* v, const char* token) {
	for (; *v != '\0'; v++) {
		const char *a = v, *b = token;
		while (*b != '\0' && lower(*a) == *b) {
			a++;
			b++;
		}
		if (*b == '\0') return true;
	}
	return false;
}
static state put(char* buf, uint* pos, const uint max, const char* s) {
	while (*s != '\0') {
		if (*pos + 1 >= max) return false;
		buf[(*pos)++] = *s++;
	}
	buf[*pos] = '\0';
	return true;
}
static state safeText(const char* s) {
	if (*s == '\0') return false;
	for (; *s != '\0'; s++) if ((uint8)*s <= ' ' || *s == 0x7F) return false;
	return true;
}

enum { BODY_CLOSE, BODY_LENGTH, BODY_CHUNKED };
enum { CH_SIZE, CH_EXT, CH_SIZE_LF, CH_DATA, CH_DATA_CR, CH_DATA_LF, CH_TRAILER, CH_DONE };
static uint8 bodyMode, chState, sizeDigits, trailerLine;
static uint32 remaining, chunkLeft;
static state bodyDone, bodyError;
static void (*bodySink)(const uint8* data, const uint16 len);

static void deliver(const uint8* d, const uint16 n) {
	if (n == 0 || skipBody) return;
	bodyBytes += n;
	if (bodySink) bodySink(d, n);
}
static int hexValue(const char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}
static void feed(const uint8* d, uint16 n) {
	if (bodyDone || bodyError) return;
	if (bodyMode == BODY_CLOSE) {
		deliver(d, n);
		return;
	}
	if (bodyMode == BODY_LENGTH) {
		const uint16 take = remaining < n ? (uint16)remaining : n;
		deliver(d, take);
		remaining -= take;
		if (remaining == 0) bodyDone = true;
		return;
	}
	uint16 i = 0;
	while (i < n && !bodyDone && !bodyError) {
		const char c = (char)d[i];
		switch (chState) {
			case CH_SIZE:
				if (c == '\r') {
					if (sizeDigits == 0) bodyError = true;
					chState = CH_SIZE_LF;
				} else if (c == ';') chState = CH_EXT;
				else {
					const int h = hexValue(c);
					if (h < 0 || sizeDigits >= 8) bodyError = true;
					else {
						chunkLeft = (chunkLeft << 4) | (uint32)h;
						sizeDigits++;
					}
				}
				i++;
				break;
			case CH_EXT:
				if (c == '\r') chState = CH_SIZE_LF;
				i++;
				break;
			case CH_SIZE_LF:
				if (c != '\n') bodyError = true;
				else if (chunkLeft == 0) {
					chState = CH_TRAILER;
					trailerLine = 0;
				} else chState = CH_DATA;
				i++;
				break;
			case CH_DATA: {
				const uint16 avail = (uint16)(n - i), take = chunkLeft < avail ? (uint16)chunkLeft : avail;
				deliver(d + i, take);
				chunkLeft -= take;
				i += take;
				if (chunkLeft == 0) chState = CH_DATA_CR;
				break;
			}
			case CH_DATA_CR:
				if (c != '\r') bodyError = true;
				else chState = CH_DATA_LF;
				i++;
				break;
			case CH_DATA_LF:
				if (c != '\n') bodyError = true;
				else {
					chState = CH_SIZE;
					chunkLeft = 0;
					sizeDigits = 0;
				}
				i++;
				break;
			case CH_TRAILER:
				if (c == '\n') {
					if (trailerLine == 0) {
						bodyDone = true;
						chState = CH_DONE;
					}
					trailerLine = 0;
				} else if (c != '\r') trailerLine = 1;
				i++;
				break;
			default: i++; break;
		}
	}
}
static int parseHeaders(char* h, const uint len, state* chunked, state* haveLength, uint32* length) {
	uint pos = 0;
	if (len < 12 || h[0] != 'H' || h[1] != 'T' || h[2] != 'T' || h[3] != 'P' || h[4] != '/') return HTTP_ERR_PROTOCOL;
	while (pos < len && h[pos] != ' ') pos++;
	pos++;
	if (pos + 3 > len || h[pos] < '0' || h[pos] > '9' || h[pos + 1] < '0' || h[pos + 1] > '9' || h[pos + 2] < '0' || h[pos + 2] > '9') return HTTP_ERR_PROTOCOL;
	const int status = (h[pos] - '0') * 100 + (h[pos + 1] - '0') * 10 + (h[pos + 2] - '0');
	while (pos < len && h[pos] != '\n') pos++;
	pos++;
	while (pos < len) {
		char* line = h + pos;
		while (pos < len && h[pos] != '\r' && h[pos] != '\n') pos++;
		if (pos >= len) break;
		const char end = h[pos];
		h[pos] = '\0';
		if (end == '\r') pos++;
		pos++;
		const char* v;
		if ((v = headerValue(line, "content-length:")) != 0) {
			uint32 n = 0;
			state ok = *v != '\0';
			for (; *v >= '0' && *v <= '9'; v++) n = n * 10 + (uint32)(*v - '0');
			if (!ok) return HTTP_ERR_PROTOCOL;
			*haveLength = true;
			*length = n;
		} else if ((v = headerValue(line, "transfer-encoding:")) != 0) {
			if (hasToken(v, "chunked")) *chunked = true;
		} else if ((v = headerValue(line, "location:")) != 0) {
			uint k = 0;
			while (v[k] != '\0' && k < LOCATION_MAX) {
				location[k] = v[k];
				k++;
			}
			location[k] = '\0';
		}
	}
	return status;
}
static int request(const transport* t, const state secure, const char* host, const uint16 port, const char* path, void (*sink)(const uint8* data, const uint16 len)) {
	bodyBytes = 0;
	location[0] = '\0';
	tlsDetail = "";
	skipBody = false;
	if (!host || !path || !safeText(host) || !safeText(path) || path[0] != '/' || slen(host) > 100 || slen(path) > 400) return HTTP_ERR_PROTOCOL;
	uint8 ip[4];
	if (!netDnsResolve(host, ip)) return HTTP_ERR_DNS;
	if (!t->connect(ip, port, host)) {
		if (!secure) return HTTP_ERR_CONNECT;
		tlsDetail = tlsErrorText();
		const char* cc = "cannot connect";
		uint i = 0;
		while (cc[i] != '\0' && tlsDetail[i] == cc[i]) i++;
		return cc[i] == '\0' ? HTTP_ERR_CONNECT : HTTP_ERR_TLS;
	}
	char req[700];
	uint rl = 0;
	state ok = put(req, &rl, sizeof(req), "GET ") && put(req, &rl, sizeof(req), path) && put(req, &rl, sizeof(req), " HTTP/1.1\r\nHost: ") && put(req, &rl, sizeof(req), host);
	if (ok && port != (secure ? 443 : 80)) {
		char num[8];
		uint n = 0, v = port;
		char tmp[6];
		do {
			tmp[n++] = (char)('0' + v % 10);
			v /= 10;
		} while (v > 0);
		for (uint i = 0; i < n; i++) num[i] = tmp[n - 1 - i];
		num[n] = '\0';
		ok = put(req, &rl, sizeof(req), ":") && put(req, &rl, sizeof(req), num);
	}
	ok = ok && put(req, &rl, sizeof(req), "\r\nUser-Agent: Qunetra\r\nAccept: */*\r\nConnection: close\r\n\r\n");
	if (!ok) {
		t->abort();
		return HTTP_ERR_PROTOCOL;
	}
	if (!t->send((const uint8*)req, rl)) {
		t->abort();
		return HTTP_ERR_SEND;
	}
	static char hdr[HEADER_MAX + 1];
	uint hdrLen = 0;
	state headersDone = false;
	int status = 0;
	bodySink = sink;
	bodyDone = bodyError = false;
	bodyMode = BODY_CLOSE;
	chState = CH_SIZE;
	chunkLeft = 0;
	sizeDigits = 0;
	remaining = 0;
	int result = HTTP_ERR_TIMEOUT;
	uint8 buf[1024];
	uint64 idle = timerDeadline(HTTP_IDLE_TIMEOUT_MS);
	while (true) {
		const int n = t->recv(buf, sizeof(buf));
		if (n < 0) {
			if (!headersDone) result = HTTP_ERR_PROTOCOL;
			else if (bodyDone || bodyMode == BODY_CLOSE) result = status;
			else result = HTTP_ERR_TRUNCATED;
			break;
		}
		if (n == 0) {
			if (timerExpired(idle)) {
				result = HTTP_ERR_TIMEOUT;
				break;
			}
			continue;
		}
		idle = timerDeadline(HTTP_IDLE_TIMEOUT_MS);
		const uint8* data = buf;
		uint16 len = (uint16)n;
		const uint8* extra = 0;
		uint16 extraLen = 0;
		if (!headersDone) {
			const uint start = hdrLen > 3 ? hdrLen - 3 : 0;
			uint16 used = 0;
			while (used < len && hdrLen < HEADER_MAX) hdr[hdrLen++] = (char)data[used++];
			int end = -1;
			for (uint i = start; i + 3 < hdrLen; i++) {
				if (hdr[i] == '\r' && hdr[i + 1] == '\n' && hdr[i + 2] == '\r' && hdr[i + 3] == '\n') {
					end = (int)i + 4;
					break;
				}
			}
			if (end < 0) {
				if (hdrLen >= HEADER_MAX) {
					result = HTTP_ERR_PROTOCOL;
					break;
				}
				continue;
			}
			state chunked = false, haveLength = false;
			uint32 length = 0;
			status = parseHeaders(hdr, (uint)end, &chunked, &haveLength, &length);
			if (status < 0) {
				result = status;
				break;
			}
			headersDone = true;
			skipBody = followMode && (status == 301 || status == 302 || status == 303 || status == 307 || status == 308);
			if (status == 204 || status == 304 || (status >= 100 && status < 200)) { // no body by definition
				bodyMode = BODY_LENGTH;
				remaining = 0;
				bodyDone = true;
			} else if (chunked) bodyMode = BODY_CHUNKED;
			else if (haveLength) {
				bodyMode = BODY_LENGTH;
				remaining = length;
				if (length == 0) bodyDone = true;
			} else bodyMode = BODY_CLOSE;
			data = (const uint8*)hdr + end;
			len = (uint16)(hdrLen - (uint)end);
			if (used < (uint16)n) {
				extra = buf + used;
				extraLen = (uint16)(n - used);
			}
		}
		feed(data, len);
		if (extra) feed(extra, extraLen);
		if (bodyError) {
			result = HTTP_ERR_PROTOCOL;
			break;
		}
		if (bodyDone) {
			result = status;
			break;
		}
	}
	if (result < 0) t->abort();
	else t->close();
	return result;
}

static int isRedirect(const int status) { return status == 301 || status == 302 || status == 303 || status == 307 || status == 308; }

int httpGet(const char* host, const uint16 port, const char* path, void (*sink)(const uint8* data, const uint16 len)) {
	followMode = false;
	return request(&plainTransport, false, host, port, path, sink);
}
int httpsGet(const char* host, const uint16 port, const char* path, void (*sink)(const uint8* data, const uint16 len)) {
	followMode = false;
	return request(&tlsTransport, true, host, port, path, sink);
}
static state parseUrl(const char* url, state* secure, char* host, uint hostMax, uint16* port, char* path, uint pathMax) {
	const char *p = url, *https = "https://", *http = "http://";
	uint i = 0;
	while (https[i] != '\0' && p[i] == https[i]) i++;
	if (https[i] == '\0') {
		*secure = true;
		p += 8;
	} else {
		i = 0;
		while (http[i] != '\0' && p[i] == http[i]) i++;
		if (http[i] != '\0') return false;
		*secure = false;
		p += 7;
	}
	uint n = 0;
	while (*p != '\0' && *p != ':' && *p != '/') {
		if (n + 1 >= hostMax) return false;
		host[n++] = *p++;
	}
	host[n] = '\0';
	if (n == 0) return false;
	uint v = 0;
	if (*p == ':') {
		p++;
		state digits = false;
		while (*p >= '0' && *p <= '9') {
			v = v * 10 + (uint)(*p++ - '0');
			digits = true;
			if (v > 65535) return false;
		}
		if (!digits || v == 0) return false;
	}
	*port = v ? (uint16)v : (*secure ? 443 : 80);
	n = 0;
	if (*p == '\0') path[n++] = '/';
	while (*p != '\0') {
		if (n + 1 >= pathMax) return false;
		path[n++] = *p++;
	}
	path[n] = '\0';
	return true;
}
int httpGetUrl(const char* url, void (*sink)(const uint8* data, const uint16 len)) {
	static char current[520], host[130], path[420];
	uint len = 0;
	while (url[len] != '\0' && len + 1 < sizeof(current)) {
		current[len] = url[len];
		len++;
	}
	if (url[len] != '\0') return HTTP_ERR_PROTOCOL;
	current[len] = '\0';
	for (uint hop = 0; hop < 5; hop++) {
		state secure;
		uint16 port;
		if (!parseUrl(current, &secure, host, sizeof(host), &port, path, sizeof(path))) return HTTP_ERR_PROTOCOL;
		followMode = true;
		const int status = request(secure ? &tlsTransport : &plainTransport, secure, host, port, path, sink);
		followMode = false;
		if (!isRedirect(status) || location[0] == '\0') return status;
		static char next[520];
		uint n = 0;
		if (location[0] == '/') {
			const char* scheme = secure ? "https://" : "http://";
			for (uint i = 0; scheme[i] != '\0' && n + 1 < sizeof(next); i++) next[n++] = scheme[i];
			for (uint i = 0; host[i] != '\0' && n + 1 < sizeof(next); i++) next[n++] = host[i];
			if (port != (secure ? 443 : 80)) {
				char num[8];
				uint k = 0, v = port;
				char tmp[6];
				do {
					tmp[k++] = (char)('0' + v % 10);
					v /= 10;
				} while (v > 0);
				if (n + 1 < sizeof(next)) next[n++] = ':';
				for (uint i = 0; i < k; i++) num[i] = tmp[k - 1 - i];
				for (uint i = 0; i < k && n + 1 < sizeof(next); i++) next[n++] = num[i];
			}
		} else if (location[0] != 'h') return status;
		for (uint i = 0; location[i] != '\0' && n + 1 < sizeof(next); i++) next[n++] = location[i];
		next[n] = '\0';
		for (uint i = 0; i <= n; i++) current[i] = next[i];
	}
	return HTTP_ERR_PROTOCOL;
}

const char* httpErrorText(const int code) {
	switch (code) {
		case HTTP_ERR_DNS: return "name not found";
		case HTTP_ERR_CONNECT: return "cannot connect";
		case HTTP_ERR_SEND: return "send failed";
		case HTTP_ERR_TIMEOUT: return "timeout";
		case HTTP_ERR_PROTOCOL: return "bad response";
		case HTTP_ERR_TRUNCATED: return "connection closed too early";
		case HTTP_ERR_TLS: return "TLS error";
		default: return code > 0 ? "ok" : "error";
	}
}
const char* httpErrorDetail() { return tlsDetail; }
const char* httpLocation() { return location; }
uint32 httpBodyBytes() { return bodyBytes; }
