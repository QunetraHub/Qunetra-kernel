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
#include "../timer.h"
#include "tcp.h"

#ifndef TCP_CONNECT_TIMEOUT_MS
#define TCP_CONNECT_TIMEOUT_MS 5000
#endif
#ifndef TCP_SYN_RETRY_MS
#define TCP_SYN_RETRY_MS 1000
#endif
#ifndef TCP_RTO_MS
#define TCP_RTO_MS 500
#endif
#ifndef TCP_MAX_RETRIES
#define TCP_MAX_RETRIES 8
#endif
#ifndef TCP_CLOSE_TIMEOUT_MS
#define TCP_CLOSE_TIMEOUT_MS 2000
#endif
#ifndef TCP_ZERO_WINDOW_TIMEOUT_MS
#define TCP_ZERO_WINDOW_TIMEOUT_MS 10000
#endif

#define TCP_HEADER 20
#define PROTO_TCP 6
#define F_FIN 0x01
#define F_SYN 0x02
#define F_RST 0x04
#define F_PSH 0x08
#define F_ACK 0x10
#define MSS_LOCAL 1460
#define RX_SIZE 8192

enum { ST_CLOSED, ST_SYN_SENT, ST_ESTABLISHED, ST_FIN_WAIT_1, ST_FIN_WAIT_2, ST_CLOSE_WAIT, ST_CLOSING, ST_LAST_ACK };

static uint8 st = ST_CLOSED, peerIp[4], rxBuf[RX_SIZE];
static uint16 lport = 0, rport = 0, sndWnd = 0, peerMss = 536, rxHead = 0, rxTail = 0, rxCount = 0;
static uint32 sndUna = 0, sndNxt = 0, rcvNxt = 0;
static state finSent = false, finReceived = false;

static void put16(uint8* p, const uint16 v) {
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)v;
}
static void put32(uint8* p, const uint32 v) {
	p[0] = (uint8)(v >> 24);
	p[1] = (uint8)(v >> 16);
	p[2] = (uint8)(v >> 8);
	p[3] = (uint8)v;
}
static uint16 get16(const uint8* p) { return (uint16)((p[0] << 8) | p[1]); }
static uint32 get32(const uint8* p) { return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3]; }
static int32 seqDiff(const uint32 a, const uint32 b) { return (int32)(a - b); }
static state sameIp(const uint8* a, const uint8* b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]; }
static uint16 windowSize() { return (uint16)(RX_SIZE - rxCount); }
static state sendRaw(const uint8* dstIp, const uint16 sport, const uint16 dport, const uint32 seq, const uint32 ack, const uint8 flags, const uint16 wnd, const uint8* data, const uint16 len) {
	if (len > MSS_LOCAL) return false;
	uint8 seg[TCP_HEADER + 4 + MSS_LOCAL];
	const uint16 hlen = (flags & F_SYN) ? TCP_HEADER + 4 : TCP_HEADER;
	put16(seg, sport);
	put16(seg + 2, dport);
	put32(seg + 4, seq);
	put32(seg + 8, ack);
	seg[12] = (uint8)((hlen / 4) << 4);
	seg[13] = flags;
	put16(seg + 14, wnd);
	put16(seg + 16, 0);
	put16(seg + 18, 0);
	if (flags & F_SYN) {
		seg[20] = 2;
		seg[21] = 4;
		put16(seg + 22, MSS_LOCAL);
	}
	for (uint16 i = 0; i < len; i++) seg[hlen + i] = data[i];
	put16(seg + 16, netPseudoChecksum(netIp(), dstIp, PROTO_TCP, seg, hlen + len));
	return netIpSend(dstIp, PROTO_TCP, seg, hlen + len);
}
static state sendSegment(const uint8 flags, const uint32 seq, const uint8* data, const uint16 len) {
	return sendRaw(peerIp, lport, rport, seq, (flags & F_ACK) ? rcvNxt : 0, flags, windowSize(), data, len); }
static void sendAck() { sendSegment(F_ACK, sndNxt, 0, 0); }
static uint16 segMax() { return peerMss < MSS_LOCAL ? peerMss : MSS_LOCAL; }
state tcpIsOpen() { return st == ST_ESTABLISHED || st == ST_CLOSE_WAIT; }
void tcpInput(const uint8 src[4], const uint8 dst[4], const uint8* seg, const uint16 len) {
	if (len < TCP_HEADER) return;
	const uint16 doff = (uint16)((seg[12] >> 4) * 4);
	if (doff < TCP_HEADER || doff > len) return;
	if (netPseudoChecksum(src, dst, PROTO_TCP, seg, len) != 0) return;
	const uint8 flags = seg[13], *data = seg + doff;
	const uint16 sport = get16(seg), dport = get16(seg + 2), wnd = get16(seg + 14), dlen = (uint16)(len - doff);
	const uint32 seq = get32(seg + 4), ack = get32(seg + 8);
	if (st == ST_CLOSED || dport != lport || sport != rport || !sameIp(src, peerIp)) {
		if (!(flags & F_RST)) {
			if (flags & F_ACK) sendRaw(src, dport, sport, ack, 0, F_RST, 0, 0, 0);
			else sendRaw(src, dport, sport, 0, seq + dlen + ((flags & F_SYN) ? 1 : 0) + ((flags & F_FIN) ? 1 : 0), F_RST | F_ACK, 0, 0, 0);
		}
		return;
	}
	if (st == ST_SYN_SENT) {
		if ((flags & F_ACK) && ack != sndNxt) {
			if (!(flags & F_RST)) sendRaw(src, dport, sport, ack, 0, F_RST, 0, 0, 0);
			return;
		}
		if (flags & F_RST) {
			if (flags & F_ACK) st = ST_CLOSED;
			return;
		}
		if ((flags & F_SYN) && (flags & F_ACK)) {
			peerMss = 536;
			for (uint16 i = TCP_HEADER; i + 1 < doff;) {
				const uint8 kind = seg[i];
				if (kind == 0) break;
				if (kind == 1) {
					i++;
					continue;
				}
				const uint8 olen = seg[i + 1];
				if (olen < 2 || i + olen > doff) break;
				if (kind == 2 && olen == 4) peerMss = get16(seg + i + 2);
				i += olen;
			}
			if (peerMss == 0) peerMss = 536;
			rcvNxt = seq + 1;
			sndUna = ack;
			sndWnd = wnd;
			st = ST_ESTABLISHED;
			sendAck();
		}
		return;
	}
	if (flags & F_RST) {
		const int32 d = seqDiff(seq, rcvNxt);
		if (d >= 0 && d < (int32)(windowSize() ? windowSize() : 1)) st = ST_CLOSED;
		return;
	}
	if (flags & F_SYN) {
		sendAck();
		return;
	}
	if (!(flags & F_ACK)) return;
	if (seqDiff(ack, sndNxt) > 0) {
		sendAck();
		return;
	}
	if (seqDiff(ack, sndUna) > 0) sndUna = ack;
	sndWnd = wnd;
	if (finSent && sndUna == sndNxt) {
		if (st == ST_FIN_WAIT_1) st = ST_FIN_WAIT_2;
		else if (st == ST_CLOSING || st == ST_LAST_ACK) {
			st = ST_CLOSED;
			return;
		}
	}
	state needAck = false;
	const state canReceive = st == ST_ESTABLISHED || st == ST_FIN_WAIT_1 || st == ST_FIN_WAIT_2;
	if (canReceive && dlen > 0) {
		const int32 d = seqDiff(rcvNxt, seq);
		if (d >= 0 && d < (int32)dlen) {
			const uint16 skip = (uint16)d;
			uint16 n = (uint16)(dlen - skip);
			const uint16 room = (uint16)(RX_SIZE - rxCount);
			if (n > room) n = room;
			for (uint16 i = 0; i < n; i++) {
				rxBuf[rxHead] = data[skip + i];
				rxHead = (uint16)((rxHead + 1) % RX_SIZE);
			}
			rxCount = (uint16)(rxCount + n);
			rcvNxt += n;
		}
		needAck = true;
	}
	if (canReceive && (flags & F_FIN) && seqDiff(seq + dlen, rcvNxt) == 0) {
		rcvNxt++;
		finReceived = true;
		needAck = true;
		if (st == ST_ESTABLISHED) st = ST_CLOSE_WAIT;
		else if (st == ST_FIN_WAIT_1) st = ST_CLOSING;
		else if (st == ST_FIN_WAIT_2) st = ST_CLOSED;
	}
	if (needAck) sendAck();
}
state tcpConnect(const uint8 ip[4], const uint16 port) {
	if (st != ST_CLOSED) tcpAbort();
	for (uint8 i = 0; i < 4; i++) peerIp[i] = ip[i];
	const uint32 seed = timerSeed();
	rport = port;
	lport = (uint16)(49152 + (seed % 16000));
	rxHead = rxTail = rxCount = 0;
	finSent = finReceived = false;
	sndUna = seed * 2654435761u;
	sndNxt = sndUna + 1;
	rcvNxt = 0;
	sndWnd = 0;
	peerMss = 536;
	st = ST_SYN_SENT;
	const uint32 isn = sndUna;
	const uint64 deadline = timerDeadline(TCP_CONNECT_TIMEOUT_MS);
	uint64 retry = timerDeadline(TCP_SYN_RETRY_MS);
	if (!sendSegment(F_SYN, isn, 0, 0)) {
		st = ST_CLOSED;
		return false;
	}
	while (st == ST_SYN_SENT && !timerExpired(deadline)) {
		netPoll();
		if (st == ST_SYN_SENT && timerExpired(retry)) {
			retry = timerDeadline(TCP_SYN_RETRY_MS);
			sendSegment(F_SYN, isn, 0, 0);
		}
	}
	if (st != ST_ESTABLISHED) {
		st = ST_CLOSED;
		return false;
	}
	return true;
}
state tcpSend(const uint8* data, const uint32 len) {
	uint32 sent = 0;
	while (sent < len) {
		if (!tcpIsOpen()) return false;
		if (sndWnd == 0) {
			const uint64 dl = timerDeadline(TCP_ZERO_WINDOW_TIMEOUT_MS);
			while (sndWnd == 0 && tcpIsOpen() && !timerExpired(dl)) netPoll();
			if (sndWnd == 0) return false;
			continue;
		}
		uint16 chunk = segMax();
		if (chunk > sndWnd) chunk = sndWnd;
		if (len - sent < chunk) chunk = (uint16)(len - sent);
		const uint32 seq = sndNxt;
		if (!sendSegment(F_ACK | F_PSH, seq, data + sent, chunk)) return false;
		sndNxt += chunk;
		uint32 rto = TCP_RTO_MS;
		uint8 tries = 0;
		uint64 deadline = timerDeadline(rto);
		while (seqDiff(sndUna, seq + chunk) < 0) {
			if (st == ST_CLOSED || st == ST_SYN_SENT) return false;
			netPoll();
			if (timerExpired(deadline)) {
				if (++tries > TCP_MAX_RETRIES) {
					tcpAbort();
					return false;
				}
				rto = rto < 4000 ? rto * 2 : rto;
				deadline = timerDeadline(rto);
				sendSegment(F_ACK | F_PSH, seq, data + sent, chunk);
			}
		}
		sent += chunk;
	}
	return true;
}
int tcpRecv(uint8* buf, const uint16 maxLen) {
	if (rxCount == 0) netPoll();
	if (rxCount == 0) return (finReceived || st == ST_CLOSED) ? -1 : 0;
	const uint16 freeBefore = windowSize();
	uint16 n = rxCount < maxLen ? rxCount : maxLen;
	for (uint16 i = 0; i < n; i++) {
		buf[i] = rxBuf[rxTail];
		rxTail = (uint16)((rxTail + 1) % RX_SIZE);
	}
	rxCount = (uint16)(rxCount - n);
	if (freeBefore < MSS_LOCAL && windowSize() >= MSS_LOCAL && st != ST_CLOSED && st != ST_SYN_SENT) sendAck();
	return n;
}
void tcpClose() {
	if (st == ST_ESTABLISHED || st == ST_CLOSE_WAIT) {
		const uint32 seq = sndNxt;
		finSent = true;
		sndNxt++;
		st = st == ST_ESTABLISHED ? ST_FIN_WAIT_1 : ST_LAST_ACK;
		sendSegment(F_FIN | F_ACK, seq, 0, 0);
		const uint64 deadline = timerDeadline(TCP_CLOSE_TIMEOUT_MS);
		uint64 retry = timerDeadline(TCP_RTO_MS);
		while (st != ST_CLOSED && seqDiff(sndUna, sndNxt) < 0 && !timerExpired(deadline)) {
			netPoll();
			if (timerExpired(retry)) {
				retry = timerDeadline(TCP_RTO_MS);
				sendSegment(F_FIN | F_ACK, seq, 0, 0);
			}
		}
	}
	st = ST_CLOSED;
}
void tcpAbort() {
	if (st == ST_ESTABLISHED || st == ST_FIN_WAIT_1 || st == ST_FIN_WAIT_2 || st == ST_CLOSE_WAIT || st == ST_CLOSING || st == ST_LAST_ACK) sendSegment(F_RST | F_ACK, sndNxt, 0, 0);
	st = ST_CLOSED;
}
