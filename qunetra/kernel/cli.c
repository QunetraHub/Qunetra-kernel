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

#include "../utility/types.h"
#include "console.h"

#ifndef QUNETRA_CMD_MAX_LEN
#define QUNETRA_CMD_MAX_LEN 128
#endif
#define CMDL_STEP 256

static uint16 inputLength = 0;
static char input[QUNETRA_CMD_MAX_LEN + 1];
static state CLIDisabled = false;

void clearCLI() {
	input[0] = '\0';
	while (inputLength > 0) {
		putChar('\b');
		inputLength--;
	}
}
void setCLIState(const state _state) {
	CLIDisabled = !_state;
	if (CLIDisabled && inputLength > 0) clearCLI();
}
void addCmd(const char* cmd) {
	(void)cmd;
}
void runCmd(const char* cmd, uint cmdLen) {
	if (inputLength == 0) return;
	putChar('\n');
	(void)cmd;
	(void)cmdLen;

	kprintf("%qUnkown command! ['%s']%q\n", 20, cmd, 13);
	inputLength = 0;
	input[0] = '\0';
}
void setCharCLI(const char c) {
	if (CLIDisabled || c == '\0') return;
	if (c == '\n') {
		runCmd(input, inputLength);
		return;
	}
	if (c == '\b') {
		putChar('\b');
		if (inputLength > 0) inputLength--;
		input[inputLength] = '\0';
		return;
	}
	if (inputLength >= QUNETRA_CMD_MAX_LEN) return;
	putChar(c);
	input[inputLength] = c;
	inputLength++;
	input[inputLength] = '\0';
}
