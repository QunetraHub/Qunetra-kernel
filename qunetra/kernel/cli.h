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

#ifndef QUNETRA_CLI_H
#define QUNETRA_CLI_H

#include "../utility/types.h"

// new qunetra command
#define __nqc__(cmd) static void cmd(const int argc, char** argv)
// add qunetra command
#define __aqc__(cmd, desc) addCmd(#cmd, desc, cmd)
// qunetra command correct use
#define __qccu__(min, arg) if (argc <= min) { kprintf("%qPlease use '%s " arg "'\n", 20, argv[0]); return; }
// qunetra command disable count
#define __qcdc__ (void)argc;
// qunetra command disable values
#define __qcdv__ (void)argv;
// qunetra command disable count & values
#define __qcdb__ (void)argc; (void)argv;

typedef void (*cliAction)(const int argc, char** argv);

state initCLI();
void setCLIState(const state _state);
void setCharCLI(const char c);

state addCmd(const char* name, const char* help, cliAction action);
state runCmd(const char* line);

void addQunetraBaseCommands();

#endif
