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

#ifndef QUNETRA_NET_H
#define QUNETRA_NET_H

#include "../../utility/types.h"

state netInit();
state netLinkUp();
const uint8* netMac();
const char* netMacString();
const uint8* netIp();
void netSetIp(const uint8 ip[4]);
const uint8* netGateway();
void netSetGateway(const uint8 ip[4]);
const uint8* netDns();
void netSetDns(const uint8 ip[4]);

void netPoll();

state netArp(const uint8 ip[4], uint8 mac[6]);
state netPing(const uint8 ip[4], uint8* ttl);
state netUdpBind(const uint16 port);
void netUdpUnbind();
state netUdpSend(const uint8 dst[4], const uint16 srcPort, const uint16 dstPort, const uint8* data, const uint16 len);
uint16 netUdpRecv(uint8 srcIp[4], uint16* srcPort, uint8* buf, const uint16 maxLen);
state netDnsResolve(const char* name, uint8 ip[4]);

state netParseIp(const char* text, uint8 ip[4]);

#endif
