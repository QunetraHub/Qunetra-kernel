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

#ifndef QUNETRA_TLS_H
#define QUNETRA_TLS_H

#include "../../utility/types.h"

state tlsConnect(const uint8 ip[4], uint16 port, const char* host);
state tlsSend(const uint8* data, uint32 len);
int tlsRecv(uint8* buf, uint16 maxLen);
void tlsClose();
void tlsAbort();
void tlsSetVerify(state on);
void tlsSetClock(uint64 now);
const char* tlsErrorText();
state tlsClockWasChecked();

#endif
