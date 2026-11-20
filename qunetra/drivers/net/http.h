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

#ifndef QUNETRA_HTTP_H
#define QUNETRA_HTTP_H

#include "../../utility/types.h"

#define HTTP_ERR_DNS       -1
#define HTTP_ERR_CONNECT   -2
#define HTTP_ERR_SEND      -3
#define HTTP_ERR_TIMEOUT   -4
#define HTTP_ERR_PROTOCOL  -5
#define HTTP_ERR_TRUNCATED -6
#define HTTP_ERR_TLS       -7

int httpGet(const char* host, const uint16 port, const char* path, void (*sink)(const uint8* data, const uint16 len));
int httpsGet(const char* host, const uint16 port, const char* path, void (*sink)(const uint8* data, const uint16 len));
int httpGetUrl(const char* url, void (*sink)(const uint8* data, const uint16 len));
const char* httpErrorText(const int code);
const char* httpErrorDetail();
const char* httpLocation();
uint32 httpBodyBytes();

#endif
