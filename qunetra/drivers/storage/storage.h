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

#ifndef QUNETRA_STORAGE_H
#define QUNETRA_STORAGE_H

#include "../../utility/types.h"

uint64 getFileCount();
char* initStorage();
uint64 storageSectors();
uint64 createFile(const uint64 catalog, const char* name);
uint64 realCatalog(const uint64 catalog);
uint64 findFile(const uint64 catalog, const char* name);
state deleteFile(const uint64 catalog, const char* name);
state fwriteb(const uint64 file, const uint8 v);
state kfwrite(const uint64 file, const char* value);
state freadb(const uint64 file, const uint64 pos, uint8* out);
uint64 fileSize(const uint64 file);
void listFiles(const uint64 catalog);

#endif
