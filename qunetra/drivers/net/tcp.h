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

#ifndef QUNETRA_TCP_H
#define QUNETRA_TCP_H

#include "../../utility/types.h"

state tcpConnect(const uint8 ip[4], const uint16 port);
state tcpSend(const uint8* data, const uint32 len);
int tcpRecv(uint8* buf, const uint16 maxLen);
void tcpClose();
void tcpAbort();
state tcpIsOpen();
void tcpInput(const uint8 src[4], const uint8 dst[4], const uint8* seg, const uint16 len);

#endif
