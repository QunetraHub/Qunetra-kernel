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
#include "e1000.h"
#include "net.h"

#ifndef NET_WAIT_POLLS
#define NET_WAIT_POLLS 20000000
#endif
#define NET_ARP_TRIES 3
#define NET_DNS_TRIES 3

#define ETH_HEADER 14
#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IP  0x0800
#define ARP_SIZE 28
#define IP_HEADER 20
#define IP_MAX_PAYLOAD (E1000_MAX_FRAME - ETH_HEADER - IP_HEADER)
#define UDP_HEADER 8
#define UDP_MAX_PAYLOAD (IP_MAX_PAYLOAD - UDP_HEADER)
#define PROTO_ICMP 1
#define PROTO_UDP 17
#define ARP_CACHE_SIZE 8

static uint8 myIp[4] = {10, 0, 2, 15}, myMask[4] = {255, 255, 255, 0}, myGateway[4] = {10, 0, 2, 2}, myDns[4] = {10, 0, 2, 3};
static char macText[18] = "00:00:00:00:00:00";
static const uint8 broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, zeroMac[6] = {0, 0, 0, 0, 0, 0};
static uint8 rxFrame[E1000_MAX_FRAME], txFrame[E1000_MAX_FRAME];

typedef struct {
	uint8 ip[4], mac[6];
	state valid;
} arpEntry;
static arpEntry arpCache[ARP_CACHE_SIZE];
static uint8 arpNext = 0;
static uint16 ipId = 1;

static state pingWaiting = false, pingGot = false;
static uint8 pingTarget[4], pingTtl = 0;
static uint16 pingSeq = 0;
#define PING_ID 0x5155
static uint16 udpPort = 0, udpLen = 0, udpSrcPort = 0;
static state udpHave = false;
static uint8 udpSrcIp[4], udpBuf[UDP_MAX_PAYLOAD];

static void put16(uint8* p, const uint16 v) {
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)v;
}
static uint16 get16(const uint8* p) { return (uint16)((p[0] << 8) | p[1]); }
static state sameIp(const uint8* a, const uint8* b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]; }
static void copyIp(uint8* dst, const uint8* src) { for (uint8 i = 0; i < 4; i++) dst[i] = src[i]; }
static void copyMac(uint8* dst, const uint8* src) { for (uint8 i = 0; i < 6; i++) dst[i] = src[i]; }
static state isLimitedBroadcast(const uint8* a) { return a[0] == 255 && a[1] == 255 && a[2] == 255 && a[3] == 255; }
static state sameSubnet(const uint8* a, const uint8* b) {
	for (uint8 i = 0; i < 4; i++) if ((a[i] & myMask[i]) != (b[i] & myMask[i])) return false;
	return true;
}
static uint32 csumAdd(const uint8* d, uint16 len, uint32 acc) {
	while (len > 1) {
		acc += ((uint32)d[0] << 8) | d[1];
		d += 2;
		len -= 2;
	}
	if (len) acc += (uint32)d[0] << 8;
	return acc;
}
static uint16 csumFinish(uint32 acc) {
	while (acc >> 16) acc = (acc & 0xFFFF) + (acc >> 16);
	return (uint16)~acc;
}
static uint16 udpChecksum(const uint8* src, const uint8* dst, const uint8* udp, const uint16 len) {
	uint8 pseudo[12];
	copyIp(pseudo, src);
	copyIp(pseudo + 4, dst);
	pseudo[8] = 0;
	pseudo[9] = PROTO_UDP;
	put16(pseudo + 10, len);
	return csumFinish(csumAdd(udp, len, csumAdd(pseudo, 12, 0)));
}
state netInit() {
	if (!e1000_init()) return false;
	const char* hex = "0123456789abcdef";
	const uint8* m = e1000_mac();
	for (uint8 i = 0; i < 6; i++) {
		macText[i * 3] = hex[m[i] >> 4];
		macText[i * 3 + 1] = hex[m[i] & 0xF];
		macText[i * 3 + 2] = i < 5 ? ':' : '\0';
	}
	for (uint8 i = 0; i < ARP_CACHE_SIZE; i++) arpCache[i].valid = false;
	udpPort = 0;
	udpHave = false;
	return true;
}
state netLinkUp() { return e1000_linkUp(); }
const uint8* netMac() { return e1000_mac(); }
const char* netMacString() { return macText; }
const uint8* netIp() { return myIp; }
void netSetIp(const uint8 ip[4]) { copyIp(myIp, ip); }
const uint8* netGateway() { return myGateway; }
void netSetGateway(const uint8 ip[4]) { copyIp(myGateway, ip); }
const uint8* netDns() { return myDns; }
void netSetDns(const uint8 ip[4]) { copyIp(myDns, ip); }
state netParseIp(const char* text, uint8 out[4]) {
	if (!text) return false;
	uint8 part = 0;
	uint value = 0;
	state digits = false;
	uint8 tmp[4];
	for (const char* p = text;; p++) {
		if (*p >= '0' && *p <= '9') {
			value = value * 10 + (uint)(*p - '0');
			if (value > 255) return false;
			digits = true;
		} else if (*p == '.' || *p == '\0') {
			if (!digits || part > 3) return false;
			tmp[part++] = (uint8)value;
			value = 0;
			digits = false;
			if (*p == '\0') break;
		} else return false;
	}
	if (part != 4) return false;
	for (uint8 i = 0; i < 4; i++) out[i] = tmp[i];
	return true;
}
static state ethSend(const uint8* dstMac, const uint16 type, const uint8* payload, const uint16 len) {
	if (len > E1000_MAX_FRAME - ETH_HEADER) return false;
	copyMac(txFrame, dstMac);
	copyMac(txFrame + 6, e1000_mac());
	put16(txFrame + 12, type);
	for (uint16 i = 0; i < len; i++) txFrame[ETH_HEADER + i] = payload[i];
	return e1000_send(txFrame, ETH_HEADER + len);
}
static void arpAdd(const uint8* ip, const uint8* mac) {
	if ((ip[0] | ip[1] | ip[2] | ip[3]) == 0 || isLimitedBroadcast(ip)) return;
	for (uint8 i = 0; i < ARP_CACHE_SIZE; i++) {
		if (arpCache[i].valid && sameIp(arpCache[i].ip, ip)) {
			copyMac(arpCache[i].mac, mac);
			return;
		}
	}
	copyIp(arpCache[arpNext].ip, ip);
	copyMac(arpCache[arpNext].mac, mac);
	arpCache[arpNext].valid = true;
	arpNext = (arpNext + 1) % ARP_CACHE_SIZE;
}
static state arpLookup(const uint8* ip, uint8* mac) {
	for (uint8 i = 0; i < ARP_CACHE_SIZE; i++) {
		if (arpCache[i].valid && sameIp(arpCache[i].ip, ip)) {
			copyMac(mac, arpCache[i].mac);
			return true;
		}
	}
	return false;
}
static state arpSend(const uint16 oper, const uint8* dstMac, const uint8* targetMac, const uint8* targetIp) {
	uint8 a[ARP_SIZE];
	put16(a, 1);
	put16(a + 2, ETHERTYPE_IP);
	a[4] = 6;
	a[5] = 4;
	put16(a + 6, oper);
	copyMac(a + 8, e1000_mac());
	copyIp(a + 14, myIp);
	copyMac(a + 18, targetMac);
	copyIp(a + 24, targetIp);
	return ethSend(dstMac, ETHERTYPE_ARP, a, ARP_SIZE);
}
static void handleArp(const uint8* frame, const uint16 len) {
	if (len < ETH_HEADER + ARP_SIZE) return;
	const uint8* a = frame + ETH_HEADER;
	if (get16(a) != 1 || get16(a + 2) != ETHERTYPE_IP || a[4] != 6 || a[5] != 4) return;
	const uint16 oper = get16(a + 6);
	const uint8 *sha = a + 8, *spa = a + 14, *tpa = a + 24;
	if (oper == 2) arpAdd(spa, sha);
	else if (oper == 1 && sameIp(tpa, myIp)) {
		arpAdd(spa, sha);
		arpSend(2, sha, sha, spa);
	}
}
state netArp(const uint8 target[4], uint8 mac[6]) {
	if (!target || !mac) return false;
	if (arpLookup(target, mac)) return true;
	for (uint8 attempt = 0; attempt < NET_ARP_TRIES; attempt++) {
		if (!arpSend(1, broadcastMac, zeroMac, target)) return false;
		for (uint32 t = 0; t < NET_WAIT_POLLS; t++) {
			netPoll();
			if (arpLookup(target, mac)) return true;
		}
	}
	return false;
}
static state ipSendRaw(const uint8* dstMac, const uint8* dstIp, const uint8 proto, const uint8* payload, const uint16 len) {
	if (len > IP_MAX_PAYLOAD) return false;
	uint8 pkt[E1000_MAX_FRAME - ETH_HEADER];
	pkt[0] = 0x45;
	pkt[1] = 0;
	put16(pkt + 2, IP_HEADER + len);
	put16(pkt + 4, ipId++);
	put16(pkt + 6, 0x4000);
	pkt[8] = 64;
	pkt[9] = proto;
	put16(pkt + 10, 0);
	copyIp(pkt + 12, myIp);
	copyIp(pkt + 16, dstIp);
	put16(pkt + 10, csumFinish(csumAdd(pkt, IP_HEADER, 0)));
	for (uint16 i = 0; i < len; i++) pkt[IP_HEADER + i] = payload[i];
	return ethSend(dstMac, ETHERTYPE_IP, pkt, IP_HEADER + len);
}
static state ipSend(const uint8* dstIp, const uint8 proto, const uint8* payload, const uint16 len) {
	uint8 mac[6];
	if (isLimitedBroadcast(dstIp)) copyMac(mac, broadcastMac);
	else if (!netArp(sameSubnet(dstIp, myIp) ? dstIp : myGateway, mac)) return false;
	return ipSendRaw(mac, dstIp, proto, payload, len);
}
static void handleIcmp(const uint8* srcMac, const uint8* srcIp, const uint8 ttl, const uint8* m, const uint16 len) {
	if (len < 8 || csumFinish(csumAdd(m, len, 0)) != 0) return;
	if (m[0] == 8 && m[1] == 0) {
		uint8 reply[IP_MAX_PAYLOAD];
		for (uint16 i = 0; i < len; i++) reply[i] = m[i];
		reply[0] = 0;
		put16(reply + 2, 0);
		put16(reply + 2, csumFinish(csumAdd(reply, len, 0)));
		ipSendRaw(srcMac, srcIp, PROTO_ICMP, reply, len);
	} else if (m[0] == 0 && m[1] == 0 && pingWaiting && get16(m + 4) == PING_ID && get16(m + 6) == pingSeq && sameIp(srcIp, pingTarget)) {
		pingTtl = ttl;
		pingGot = true;
	}
}
state netPing(const uint8 target[4], uint8* ttl) {
	if (!target) return false;
	uint8 msg[8 + 32];
	msg[0] = 8;
	msg[1] = 0;
	put16(msg + 2, 0);
	put16(msg + 4, PING_ID);
	put16(msg + 6, ++pingSeq);
	for (uint8 i = 0; i < 32; i++) msg[8 + i] = (uint8)('a' + i % 23);
	put16(msg + 2, csumFinish(csumAdd(msg, sizeof(msg), 0)));
	copyIp(pingTarget, target);
	pingGot = false;
	pingWaiting = true;
	state ok = ipSend(target, PROTO_ICMP, msg, sizeof(msg));
	for (uint32 t = 0; ok && !pingGot && t < NET_WAIT_POLLS; t++) netPoll();
	pingWaiting = false;
	if (pingGot && ttl) *ttl = pingTtl;
	return pingGot;
}
static void handleUdp(const uint8* srcIp, const uint8* dstIp, const uint8* u, const uint16 len) {
	if (len < UDP_HEADER) return;
	const uint16 ulen = get16(u + 4);
	if (ulen < UDP_HEADER || ulen > len) return;
	if (get16(u + 6) != 0 && udpChecksum(srcIp, dstIp, u, ulen) != 0) return;
	if (udpPort == 0 || get16(u + 2) != udpPort || udpHave) return;
	udpLen = ulen - UDP_HEADER;
	for (uint16 i = 0; i < udpLen; i++) udpBuf[i] = u[UDP_HEADER + i];
	copyIp(udpSrcIp, srcIp);
	udpSrcPort = get16(u);
	udpHave = true;
}
state netUdpBind(const uint16 port) {
	if (port == 0) return false;
	udpPort = port;
	udpHave = false;
	return true;
}
void netUdpUnbind() {
	udpPort = 0;
	udpHave = false;
}
state netUdpSend(const uint8 dst[4], const uint16 srcPort, const uint16 dstPort, const uint8* data, const uint16 len) {
	if (!dst || (!data && len) || len > UDP_MAX_PAYLOAD) return false;
	uint8 seg[UDP_HEADER + UDP_MAX_PAYLOAD];
	put16(seg, srcPort);
	put16(seg + 2, dstPort);
	put16(seg + 4, UDP_HEADER + len);
	put16(seg + 6, 0);
	for (uint16 i = 0; i < len; i++) seg[UDP_HEADER + i] = data[i];
	uint16 c = udpChecksum(myIp, dst, seg, UDP_HEADER + len);
	put16(seg + 6, c == 0 ? 0xFFFF : c);
	return ipSend(dst, PROTO_UDP, seg, UDP_HEADER + len);
}
uint16 netUdpRecv(uint8 srcIp[4], uint16* srcPort, uint8* buf, const uint16 maxLen) {
	if (!udpHave) netPoll();
	if (!udpHave) return 0;
	const uint16 n = udpLen < maxLen ? udpLen : maxLen;
	for (uint16 i = 0; i < n; i++) buf[i] = udpBuf[i];
	if (srcIp) copyIp(srcIp, udpSrcIp);
	if (srcPort) *srcPort = udpSrcPort;
	udpHave = false;
	return n == 0 ? 1 : n;
}
static void handleIp(const uint8* frame, const uint16 len) {
	if (len < ETH_HEADER + IP_HEADER) return;
	const uint8* p = frame + ETH_HEADER;
	const uint16 ihl = (uint16)((p[0] & 0x0F) * 4), total = get16(p + 2);
	if ((p[0] >> 4) != 4 || ihl < IP_HEADER || total < ihl || ETH_HEADER + total > len) return;
	if (csumFinish(csumAdd(p, ihl, 0)) != 0) return;
	if ((get16(p + 6) & 0x3FFF) != 0) return;
	const uint8 *src = p + 12, *dst = p + 16;
	if (!sameIp(dst, myIp) && !isLimitedBroadcast(dst)) return;
	const uint8* payload = p + ihl;
	const uint16 plen = total - ihl;
	if (p[9] == PROTO_ICMP) handleIcmp(frame + 6, src, p[8], payload, plen);
	else if (p[9] == PROTO_UDP) handleUdp(src, dst, payload, plen);
}
void netPoll() {
	const uint16 len = e1000_recv(rxFrame, sizeof(rxFrame));
	if (len < ETH_HEADER) return;
	const uint16 type = get16(rxFrame + 12);
	if (type == ETHERTYPE_ARP) handleArp(rxFrame, len);
	else if (type == ETHERTYPE_IP) handleIp(rxFrame, len);
}
static int dnsBuildQuery(uint8* q, const uint16 id, const char* name) {
	put16(q, id);
	put16(q + 2, 0x0100);
	put16(q + 4, 1);
	put16(q + 6, 0);
	put16(q + 8, 0);
	put16(q + 10, 0);
	int pos = 12;
	const char* p = name;
	while (*p != '\0') {
		const char* label = p;
		while (*p != '\0' && *p != '.') {
			if ((uint8)*p <= ' ') return -1;
			p++;
		}
		const uint len = (uint)(p - label);
		if (len == 0 || len > 63 || pos + 1 + (int)len > 12 + 254) return -1;
		q[pos++] = (uint8)len;
		for (uint i = 0; i < len; i++) q[pos++] = (uint8)label[i];
		if (*p == '.') {
			p++;
			if (*p == '\0') break;
		}
	}
	if (pos == 12) return -1;
	q[pos++] = 0;
	put16(q + pos, 1);
	put16(q + pos + 2, 1);
	return pos + 4;
}
static int dnsSkipName(const uint8* r, const int len, int pos) {
	while (pos < len) {
		const uint8 l = r[pos];
		if (l == 0) return pos + 1;
		if ((l & 0xC0) == 0xC0) return pos + 2 <= len ? pos + 2 : -1;
		if (l & 0xC0) return -1;
		pos += 1 + l;
	}
	return -1;
}
static int dnsParse(const uint8* r, const int len, const uint16 id, uint8 out[4]) {
	if (len < 12 || get16(r) != id || !(get16(r + 2) & 0x8000)) return 0;
	if ((get16(r + 2) & 0x000F) != 0) return -1;
	const uint16 qd = get16(r + 4), an = get16(r + 6);
	int pos = 12;
	for (uint16 i = 0; i < qd; i++) {
		pos = dnsSkipName(r, len, pos);
		if (pos < 0 || pos + 4 > len) return -1;
		pos += 4;
	}
	for (uint16 i = 0; i < an; i++) {
		pos = dnsSkipName(r, len, pos);
		if (pos < 0 || pos + 10 > len) return -1;
		const uint16 type = get16(r + pos), cls = get16(r + pos + 2), rdlen = get16(r + pos + 8);
		pos += 10;
		if (pos + rdlen > len) return -1;
		if (type == 1 && cls == 1 && rdlen == 4) {
			copyIp(out, r + pos);
			return 1;
		}
		pos += rdlen;
	}
	return -1;
}
static uint16 dnsId = 0x1234;
state netDnsResolve(const char* name, uint8 out[4]) {
	if (!name || !out) return false;
	if (netParseIp(name, out)) return true;
	uint8 query[12 + 256 + 4], reply[512];
	for (uint8 attempt = 0; attempt < NET_DNS_TRIES; attempt++) {
		const uint16 id = dnsId++, port = (uint16)(49152 + (id % 1000));
		const int qlen = dnsBuildQuery(query, id, name);
		if (qlen < 0) return false;
		if (!netUdpBind(port)) return false;
		if (!netUdpSend(myDns, port, 53, query, (uint16)qlen)) {
			netUdpUnbind();
			return false;
		}
		for (uint32 t = 0; t < NET_WAIT_POLLS; t++) {
			uint8 from[4];
			const uint16 n = netUdpRecv(from, 0, reply, sizeof(reply));
			if (n == 0 || !sameIp(from, myDns)) continue;
			const int r = dnsParse(reply, n, id, out);
			if (r == 0) continue;
			netUdpUnbind();
			return r == 1;
		}
		netUdpUnbind();
	}
	return false;
}
